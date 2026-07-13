// =============================================================================
// LianCore - PresetManager 预设管理器
// SQLite预设库，支持标签搜索、版本历史、导入/导出
// [安全修复 B5-001] 全部用户输入查询已迁移至参数化查询 (PreparedStatement)
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <string>

// 前向声明
struct sqlite3;

namespace LianCore {

class PreparedStatement;

// 预设条目
struct PresetEntry {
    int id = -1;
    juce::String name;
    juce::String category;
    juce::String tags;          // JSON数组
    juce::String description;
    juce::String author;
    juce::String jsonData;      // 完整参数JSON
    juce::String aiPrompt;      // AI生成提示词
    float aiConfidence = 0.0f;
    int rating = 0;
    int usageCount = 0;
    juce::Time createdAt;
    juce::Time updatedAt;
};

class PresetManager {
public:
    PresetManager();
    ~PresetManager();

    // =========================================================================
    // 数据库管理
    // =========================================================================
    bool openDatabase(const juce::File& dbFile);
    void closeDatabase();
    bool isDatabaseOpen() const;

    // 获取原始 sqlite3* 句柄 (供 PreparedStatement 使用)
    sqlite3* getRawHandle() const { return rawDb_; }

    // =========================================================================
    // 预设CRUD
    // =========================================================================
    int savePreset(const PresetEntry& entry);       // [安全] 参数化查询
    bool loadPreset(int id, PresetEntry& outEntry);  // [安全] 参数化查询
    bool deletePreset(int id);                       // [安全] 参数化查询
    bool updatePreset(const PresetEntry& entry);     // [安全] 参数化查询

    // =========================================================================
    // 预设查询
    // =========================================================================
    std::vector<PresetEntry> getAllPresets();
    std::vector<PresetEntry> searchPresets(const juce::String& query);       // [安全] 参数化查询
    std::vector<PresetEntry> getPresetsByCategory(const juce::String& category); // [安全] 参数化查询
    std::vector<PresetEntry> getPresetsByTag(const juce::String& tag);       // [安全] 参数化查询
    std::vector<PresetEntry> getRecentPresets(int limit = 20);
    int getTotalPresetCount();

    // =========================================================================
    // 版本历史
    // =========================================================================
    bool savePresetVersion(int presetId, const juce::String& jsonData);      // [安全] 参数化查询
    std::vector<std::pair<int, juce::Time>> getPresetHistory(int presetId);
    bool restorePresetVersion(int presetId, int version);                     // [安全] 参数化查询

    // =========================================================================
    // 导入/导出
    // =========================================================================
    bool exportPreset(int id, const juce::File& file);
    bool importPreset(const juce::File& file);

    // =========================================================================
    // 加密导入/导出 (Beta Week 5)
    // =========================================================================
    bool exportPresetEncrypted(int id, const juce::File& file,
                               const juce::String& password);
    bool importPresetEncrypted(const juce::File& file,
                               const juce::String& password);

private:
    bool createTables();
    PresetEntry rowToEntry(class PreparedStatement& stmt);
    // JUCE 8 removed ResultSet, using raw sqlite3 handles instead

    // 辅助: 使用参数化查询执行 SELECT 并返回结果集
    std::vector<PresetEntry> selectWithParams(
        const juce::String& sql,
        std::function<void(PreparedStatement&)> bindFn);

    sqlite3* database_ = nullptr;  // raw sqlite3* handle (JUCE 8 removed SQLite wrapper)
    sqlite3* rawDb_ = nullptr;       // 原始 sqlite3* 句柄，供参数化查询使用
    juce::CriticalSection lock_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};

} // namespace LianCore