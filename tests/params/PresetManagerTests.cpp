// =============================================================================
// LianCore - PresetManager 单元测试
// 验证: 参数化查询正确性、SQL注入防护、CRUD操作、版本历史、边界情况
// 使用 JUCE UnitTest 框架
// =============================================================================
#include <JuceHeader.h>
#include "PresetManager.h"
#include "PreparedStatement.h"

// =============================================================================
// 测试 1: 数据库初始化
// =============================================================================
class PresetManagerDatabaseTest : public juce::UnitTest {
public:
    PresetManagerDatabaseTest() : juce::UnitTest("PresetManager: 数据库初始化") {}

    void runTest() override {
        beginTest("打开内存数据库");

        juce::TemporaryFile tempFile(".db");
        LianCore::PresetManager manager;

        expect(manager.openDatabase(tempFile.getFile()), "数据库应成功打开");
        expect(manager.isDatabaseOpen(), "isDatabaseOpen() 应返回 true");

        beginTest("关闭数据库");
        manager.closeDatabase();
        expect(!manager.isDatabaseOpen(), "关闭后 isDatabaseOpen() 应返回 false");
    }
};

// =============================================================================
// 测试 2: CRUD 操作 - 参数化查询
// =============================================================================
class PresetManagerCRUDTest : public juce::UnitTest {
public:
    PresetManagerCRUDTest() : juce::UnitTest("PresetManager: CRUD操作 (参数化查询)") {}

    void runTest() override {
        juce::TemporaryFile tempFile(".db");
        LianCore::PresetManager manager;
        expect(manager.openDatabase(tempFile.getFile()));

        // ---- 创建预设 ----
        beginTest("savePreset - 正常保存");
        LianCore::PresetEntry entry;
        entry.name = "Test Bass";
        entry.category = "Bass";
        entry.tags = "[\"Bass\",\"Sub\",\"Analog\"]";
        entry.description = "A test bass preset";
        entry.author = "Tester";
        entry.jsonData = "{\"nodes\":[{\"id\":\"n1\",\"type\":\"WavetableOscillator\"}]}";
        entry.aiPrompt = "Generate a deep bass";
        entry.aiConfidence = 0.85f;
        entry.rating = 4;
        entry.usageCount = 100;

        int id = manager.savePreset(entry);
        expect(id >= 0, "savePreset 应返回有效 ID");
        expectGreaterThan(id, 0);

        // ---- 读取预设 ----
        beginTest("loadPreset - 读取已保存预设");
        LianCore::PresetEntry loaded;
        expect(manager.loadPreset(id, loaded), "loadPreset 应成功");
        expect(loaded.name == "Test Bass", "名称应匹配");
        expect(loaded.category == "Bass", "类别应匹配");
        expect(loaded.author == "Tester", "作者应匹配");
        expectEquals(loaded.aiConfidence, 0.85f, "置信度应匹配");
        expectEquals(loaded.rating, 4, "评分应匹配");
        expectEquals(loaded.usageCount, 100, "使用次数应匹配");

        // ---- 更新预设 ----
        beginTest("updatePreset - 更新预设名称");
        entry.id = id;
        entry.name = "Updated Bass";
        entry.rating = 5;
        expect(manager.updatePreset(entry), "updatePreset 应成功");

        LianCore::PresetEntry updated;
        expect(manager.loadPreset(id, updated), "更新后应能读取");
        expect(updated.name == "Updated Bass", "名称应更新");
        expectEquals(updated.rating, 5, "评分应更新");

        // ---- 删除预设 ----
        beginTest("deletePreset - 删除预设");
        expect(manager.deletePreset(id), "deletePreset 应成功");
        expect(!manager.loadPreset(id, loaded), "删除后不应能读取");

        // ---- 读取不存在的预设 ----
        beginTest("loadPreset - 不存在的ID");
        expect(!manager.loadPreset(99999, loaded), "不存在的ID应返回 false");

        manager.closeDatabase();
    }
};

// =============================================================================
// 测试 3: SQL 注入防护
// =============================================================================
class PresetManagerSQLInjectionTest : public juce::UnitTest {
public:
    PresetManagerSQLInjectionTest() : juce::UnitTest("PresetManager: SQL注入防护") {}

    void runTest() override {
        juce::TemporaryFile tempFile(".db");
        LianCore::PresetManager manager;
        expect(manager.openDatabase(tempFile.getFile()));

        // ---- 恶意名称注入测试 ----
        beginTest("SQL注入: 单引号绕过");
        LianCore::PresetEntry malicious;
        malicious.name = "Test'; DROP TABLE presets; --";
        malicious.category = "Bass";
        malicious.tags = "[]";
        malicious.description = "test";
        malicious.author = "test";
        malicious.jsonData = "{}";

        int id = manager.savePreset(malicious);
        expect(id >= 0, "恶意名称不应导致插入失败");

        // 验证表未被删除
        LianCore::PresetEntry loaded;
        expect(manager.loadPreset(id, loaded), "注入后应仍能读取预设");
        expect(loaded.name == malicious.name, "名称应完整保留（含注入字符）");

        // ---- SQL 注释注入 ----
        beginTest("SQL注入: 注释绕过");
        LianCore::PresetEntry commentInjection;
        commentInjection.name = "Test";
        commentInjection.category = "Bass' OR '1'='1";
        commentInjection.tags = "[]";
        commentInjection.description = "test";
        commentInjection.author = "test";
        commentInjection.jsonData = "{}";
        commentInjection.aiPrompt = "test' UNION SELECT * FROM presets; --";

        int id2 = manager.savePreset(commentInjection);
        expect(id2 >= 0, "注释注入不应导致插入失败");

        LianCore::PresetEntry loaded2;
        expect(manager.loadPreset(id2, loaded2), "注入后应仍能读取");

        // 查询不应该返回所有记录
        auto results = manager.searchPresets("' OR '1'='1");
        expect(results.size() == 0, "OR 注入不应返回额外记录");

        // ---- Unicode 注入 ----
        beginTest("SQL注入: Unicode 特殊字符");
        LianCore::PresetEntry unicodeEntry;
        unicodeEntry.name = "测试'\u2018\u2019\u300e\u300f";
        unicodeEntry.category = "Bass";
        unicodeEntry.tags = "[]";
        unicodeEntry.description = "test";
        unicodeEntry.author = "test";
        unicodeEntry.jsonData = "{}";

        int id3 = manager.savePreset(unicodeEntry);
        expect(id3 >= 0, "Unicode 特殊字符不应导致插入失败");

        LianCore::PresetEntry loaded3;
        expect(manager.loadPreset(id3, loaded3), "Unicode 注入后应仍能读取");

        // ---- 空字符串边界测试 ----
        beginTest("边界: 空字符串");
        LianCore::PresetEntry emptyEntry;
        emptyEntry.name = "";
        emptyEntry.category = "";
        emptyEntry.tags = "";
        emptyEntry.description = "";
        emptyEntry.author = "";
        emptyEntry.jsonData = "{}";

        int id4 = manager.savePreset(emptyEntry);
        expect(id4 >= 0, "空字符串应能保存");

        // ---- 超长字符串 ----
        beginTest("边界: 超长字符串");
        LianCore::PresetEntry longEntry;
        longEntry.name = juce::String::repeatedString("A", 1000);
        longEntry.category = "Test";
        longEntry.tags = "[]";
        longEntry.description = juce::String::repeatedString("B", 5000);
        longEntry.author = "test";
        longEntry.jsonData = juce::String::repeatedString("C", 10000);

        int id5 = manager.savePreset(longEntry);
        expect(id5 >= 0, "超长字符串应能保存");

        LianCore::PresetEntry loaded5;
        expect(manager.loadPreset(id5, loaded5), "超长字符串应能读取");
        expect(loaded5.name.length() == 1000, "超长名称长度应匹配");

        manager.closeDatabase();
    }
};

// =============================================================================
// 测试 4: 搜索和查询
// =============================================================================
class PresetManagerSearchTest : public juce::UnitTest {
public:
    PresetManagerSearchTest() : juce::UnitTest("PresetManager: 搜索与查询") {}

    void runTest() override {
        juce::TemporaryFile tempFile(".db");
        LianCore::PresetManager manager;
        expect(manager.openDatabase(tempFile.getFile()));

        // 插入测试数据
        for (int i = 0; i < 10; i++) {
            LianCore::PresetEntry entry;
            entry.name = juce::String("Preset ") + juce::String(i);
            entry.category = i < 5 ? "Bass" : "Lead";
            entry.tags = i < 3 ? "[\"Bass\",\"Sub\"]" : "[\"Lead\",\"Saw\"]";
            entry.description = "Test preset " + juce::String(i);
            entry.author = "Tester";
            entry.jsonData = "{}";
            entry.rating = i;
            manager.savePreset(entry);
        }

        // ---- searchPresets ----
        beginTest("searchPresets - 按名称搜索");
        auto results = manager.searchPresets("Preset 3");
        expect(results.size() >= 1, "应找到至少1个匹配");
        expect(results[0].name == "Preset 3", "名称应匹配");

        // ---- searchPresets - 大小写搜索 ----
        beginTest("searchPresets - 部分匹配");
        auto results2 = manager.searchPresets("preset");
        expect(results2.size() >= 10, "大小写搜索应找到所有预设");

        // ---- getPresetsByCategory ----
        beginTest("getPresetsByCategory - Bass");
        auto bassResults = manager.getPresetsByCategory("Bass");
        expectEquals(bassResults.size(), 5, "Bass 类别应有 5 个预设");

        // ---- getPresetsByCategory - 不存在的类别 ----
        beginTest("getPresetsByCategory - 不存在的类别");
        auto emptyResults = manager.getPresetsByCategory("NonexistentCategory");
        expectEquals(emptyResults.size(), 0, "不存在的类别应返回空");

        // ---- getPresetsByTag ----
        beginTest("getPresetsByTag - Sub 标签");
        auto subResults = manager.getPresetsByTag("Sub");
        expectEquals(subResults.size(), 3, "Sub 标签应有 3 个预设");

        // ---- getAllPresets ----
        beginTest("getAllPresets");
        auto allResults = manager.getAllPresets();
        expectEquals(allResults.size(), 10, "应有 10 个预设");

        // ---- getRecentPresets ----
        beginTest("getRecentPresets - 限制");
        auto recent = manager.getRecentPresets(3);
        expectEquals(recent.size(), 3, "应返回 3 个最近预设");

        manager.closeDatabase();
    }
};

// =============================================================================
// 测试 5: 版本历史
// =============================================================================
class PresetManagerVersionTest : public juce::UnitTest {
public:
    PresetManagerVersionTest() : juce::UnitTest("PresetManager: 版本历史") {}

    void runTest() override {
        juce::TemporaryFile tempFile(".db");
        LianCore::PresetManager manager;
        expect(manager.openDatabase(tempFile.getFile()));

        // 创建预设
        LianCore::PresetEntry entry;
        entry.name = "Version Test";
        entry.category = "Pad";
        entry.tags = "[]";
        entry.description = "test";
        entry.author = "test";
        entry.jsonData = "{\"version\":1}";
        int id = manager.savePreset(entry);

        // ---- savePresetVersion ----
        beginTest("savePresetVersion - 保存版本");
        expect(manager.savePresetVersion(id, "{\"version\":2}"), "版本2应保存成功");
        expect(manager.savePresetVersion(id, "{\"version\":3}"), "版本3应保存成功");

        // ---- getPresetHistory ----
        beginTest("getPresetHistory - 版本列表");
        auto history = manager.getPresetHistory(id);
        expect(history.size() >= 2, "至少应有2个版本");

        // ---- restorePresetVersion ----
        beginTest("restorePresetVersion - 恢复版本");
        expect(manager.restorePresetVersion(id, 2), "版本2应能恢复");

        LianCore::PresetEntry restored;
        expect(manager.loadPreset(id, restored), "恢复后应能读取");
        expect(restored.jsonData == "{\"version\":2}", "jsonData应为版本2的内容");

        // ---- 恢复不存在的版本 ----
        beginTest("restorePresetVersion - 不存在的版本");
        expect(!manager.restorePresetVersion(id, 999), "恢复不存在的版本应失败");

        manager.closeDatabase();
    }
};

// =============================================================================
// 测试 6: 导入/导出
// =============================================================================
class PresetManagerImportExportTest : public juce::UnitTest {
public:
    PresetManagerImportExportTest() : juce::UnitTest("PresetManager: 导入/导出") {}

    void runTest() override {
        juce::TemporaryFile tempFile(".db");
        LianCore::PresetManager manager;
        expect(manager.openDatabase(tempFile.getFile()));

        // 创建预设
        LianCore::PresetEntry entry;
        entry.name = "Export Test";
        entry.category = "Keys";
        entry.tags = "[\"Piano\",\"Bright\"]";
        entry.description = "Export test preset";
        entry.author = "Exporter";
        entry.jsonData = "{\"nodes\":[{\"id\":\"n1\",\"type\":\"WavetableOscillator\",\"params\":{\"freq\":0.5}}]}";
        int id = manager.savePreset(entry);

        // ---- exportPreset ----
        beginTest("exportPreset - 导出到文件");
        juce::TemporaryFile exportFile(".liancore");
        expect(manager.exportPreset(id, exportFile.getFile()), "导出应成功");
        expect(exportFile.getFile().existsAsFile(), "导出文件应存在");

        // 验证文件内容
        juce::String content = exportFile.getFile().loadFileAsString();
        auto json = juce::JSON::parse(content);
        expect(json.isObject(), "导出应为有效 JSON");
        expect(json.getProperty("name", "").toString() == "Export Test", "导出名称应匹配");
        expect(json.getProperty("format", "").toString() == "LianCorePreset", "格式标识应正确");

        // ---- importPreset ----
        beginTest("importPreset - 从文件导入");
        expect(manager.importPreset(exportFile.getFile()), "导入应成功");

        // 验证导入的预设
        auto imported = manager.searchPresets("Export Test");
        expect(imported.size() >= 2, "导入后应有两个同名预设");

        // ---- 导入无效文件 ----
        beginTest("importPreset - 无效文件");
        juce::TemporaryFile invalidFile(".txt");
        invalidFile.getFile().replaceWithText("not a valid preset");
        expect(!manager.importPreset(invalidFile.getFile()), "无效文件导入应失败");

        manager.closeDatabase();
    }
};

// =============================================================================
// 测试 7: 并发安全
// =============================================================================
class PresetManagerConcurrencyTest : public juce::UnitTest {
public:
    PresetManagerConcurrencyTest() : juce::UnitTest("PresetManager: 并发安全") {}

    void runTest() override {
        juce::TemporaryFile tempFile(".db");
        LianCore::PresetManager manager;
        expect(manager.openDatabase(tempFile.getFile()));

        beginTest("多线程并发保存");

        juce::Array<int> ids;
        juce::CriticalSection cs;

        // 10 个线程同时保存
        juce::ThreadPool pool(10);
        for (int i = 0; i < 10; i++) {
            pool.addJob([&manager, &ids, &cs, i]() {
                LianCore::PresetEntry entry;
                entry.name = "Thread " + juce::String(i);
                entry.category = "Test";
                entry.tags = "[]";
                entry.description = "test";
                entry.author = "test";
                entry.jsonData = "{}";
                int id = manager.savePreset(entry);
                {
                    juce::ScopedLock sl(cs);
                    ids.add(id);
                }
            });
        }

        pool.waitForJobsToFinish(5000, -1);

        beginTest("验证并发保存结果");
        for (int id : ids) {
            expect(id >= 0, "每个线程的保存都应成功");
        }

        // 验证所有预设都保存了
        auto all = manager.getAllPresets();
        expect(all.size() >= 10, "至少应有10个预设");

        manager.closeDatabase();
    }
};

// =============================================================================
// 测试运行器
// =============================================================================
static PresetManagerDatabaseTest pmDatabaseTest;
static PresetManagerCRUDTest pmCRUDTest;
static PresetManagerSQLInjectionTest pmSQLInjectionTest;
static PresetManagerSearchTest pmSearchTest;
static PresetManagerVersionTest pmVersionTest;
static PresetManagerImportExportTest pmImportExportTest;
static PresetManagerConcurrencyTest pmConcurrencyTest;