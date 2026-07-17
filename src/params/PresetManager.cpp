// =============================================================================
// LianCore - PresetManager 实现 (安全修复版)
// [B5-001] 所有用户输入查询已迁移至 PreparedStatement 参数化查询
// [B5-003] 集成 AES256Encryptor 实现加密导出/导入
// JUCE 8 compatibility: use raw SQLite3 API (JUCE removed SQLite wrapper)
// 参考: OWASP SQL Injection Prevention, CWE-89
// =============================================================================
#include "PresetManager.h"
#include "PreparedStatement.h"
#include "../security/AES256Encryptor.h"
#include "sqlite3.h"
#include <unordered_map>
#include <set>

namespace LianCore {

PresetManager::PresetManager() = default;

PresetManager::~PresetManager() {
    closeDatabase();
}

bool PresetManager::openDatabase(const juce::File& dbFile) {
    juce::ScopedLock sl(lock_);

    juce::File parentDir = dbFile.getParentDirectory();
    if (!parentDir.exists()) {
        parentDir.createDirectory();
    }

    // JUCE 8 已经移除了 juce::SQLite 包装，现在完全使用原生 SQLite3 API
    int rc = sqlite3_open_v2(
        dbFile.getFullPathName().toRawUTF8(),
        &database_,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
        nullptr
    );
    if (rc != SQLITE_OK) {
        if (database_) sqlite3_close(database_);
        database_ = nullptr;
        rawDb_ = nullptr;
        return false;
    }

    // database_ 就是 rawDb_
    rawDb_ = database_;

    // 启用 WAL 模式提升并发性能
    if (rawDb_) {
        sqlite3_exec(rawDb_, "PRAGMA journal_mode=WAL", nullptr, nullptr, nullptr);
        sqlite3_exec(rawDb_, "PRAGMA foreign_keys=ON", nullptr, nullptr, nullptr);
    }

    return createTables();
}

void PresetManager::closeDatabase() {
    juce::ScopedLock sl(lock_);
    if (database_) {
        sqlite3_close(database_);
    }
    database_ = nullptr;
    rawDb_ = nullptr;
}

bool PresetManager::isDatabaseOpen() const {
    return database_ != nullptr && rawDb_ != nullptr;
}

bool PresetManager::createTables() {
    if (!database_) return false;

    const char* sql =
        "CREATE TABLE IF NOT EXISTS presets ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "category TEXT,"
        "tags TEXT,"
        "description TEXT,"
        "author TEXT,"
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "json_data TEXT,"
        "ai_prompt TEXT,"
        "ai_confidence REAL DEFAULT 0.0,"
        "rating INTEGER DEFAULT 0,"
        "usage_count INTEGER DEFAULT 0"
        ");"

        "CREATE TABLE IF NOT EXISTS preset_history ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "preset_id INTEGER NOT NULL,"
        "json_data TEXT NOT NULL,"
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "FOREIGN KEY(preset_id) REFERENCES presets(id)"
        ");";

    int rc = sqlite3_exec(database_, sql, nullptr, nullptr, nullptr);
    return rc == SQLITE_OK;
}

// =============================================================================
// 辅助: 参数化 SELECT 查询
// =============================================================================
std::vector<PresetEntry> PresetManager::selectWithParams(
    const juce::String& sql,
    std::function<void(PreparedStatement&)> bindFn
) {
    std::vector<PresetEntry> result;
    if (!rawDb_) return result;

    PreparedStatement stmt;
    if (!stmt.prepare(rawDb_, sql)) return result;

    bindFn(stmt);

    while (stmt.step()) {
        result.push_back(rowToEntry(stmt));
    }
    return result;
}

// =============================================================================
// savePreset [安全] - 参数化 INSERT
// =============================================================================
int PresetManager::savePreset(const PresetEntry& entry) {
    juce::ScopedLock sl(lock_);
    if (!rawDb_) return -1;

    PreparedStatement stmt;
    if (!stmt.prepare(rawDb_,
        "INSERT INTO presets (name, category, tags, description, author, "
        "json_data, ai_prompt, ai_confidence, rating, usage_count) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)"
    )) return -1;

    stmt.bindText(1, entry.name);
    stmt.bindText(2, entry.category);
    stmt.bindText(3, entry.tags);
    stmt.bindText(4, entry.description);
    stmt.bindText(5, entry.author);
    stmt.bindText(6, entry.jsonData);
    stmt.bindText(7, entry.aiPrompt);
    stmt.bindFloat(8, entry.aiConfidence);
    stmt.bindInt(9, entry.rating);
    stmt.bindInt(10, entry.usageCount);

    if (!stmt.execute()) return -1;
    return (int)stmt.getLastInsertRowId();
}

// =============================================================================
// loadPreset [安全] - 参数化 SELECT
// =============================================================================
bool PresetManager::loadPreset(int id, PresetEntry& outEntry) {
    juce::ScopedLock sl(lock_);
    if (!rawDb_) return false;

    PreparedStatement stmt;
    if (!stmt.prepare(rawDb_, "SELECT * FROM presets WHERE id = ?1")) return false;

    stmt.bindInt(1, id);

    if (!stmt.step()) return false;

    outEntry = rowToEntry(stmt);
    return true;
}

// =============================================================================
// deletePreset [安全] - 参数化 DELETE
// =============================================================================
bool PresetManager::deletePreset(int id) {
    juce::ScopedLock sl(lock_);
    if (!rawDb_) return false;

    PreparedStatement stmt;
    if (!stmt.prepare(rawDb_, "DELETE FROM presets WHERE id = ?1")) return false;

    stmt.bindInt(1, id);
    return stmt.execute();
}

// =============================================================================
// updatePreset [安全] - 参数化 UPDATE
// =============================================================================
bool PresetManager::updatePreset(const PresetEntry& entry) {
    juce::ScopedLock sl(lock_);
    if (!rawDb_) return false;

    PreparedStatement stmt;
    if (!stmt.prepare(rawDb_,
        "UPDATE presets SET name=?1, category=?2, tags=?3, description=?4, "
        "json_data=?5, ai_prompt=?6, ai_confidence=?7, rating=?8, "
        "usage_count=?9, updated_at=CURRENT_TIMESTAMP WHERE id=?10"
    )) return false;

    stmt.bindText(1, entry.name);
    stmt.bindText(2, entry.category);
    stmt.bindText(3, entry.tags);
    stmt.bindText(4, entry.description);
    stmt.bindText(5, entry.jsonData);
    stmt.bindText(6, entry.aiPrompt);
    stmt.bindFloat(7, entry.aiConfidence);
    stmt.bindInt(8, entry.rating);
    stmt.bindInt(9, entry.usageCount);
    stmt.bindInt(10, entry.id);

    return stmt.execute();
}

// =============================================================================
// getAllPresets - 使用原生 SQLite3 API (JUCE 8 removed SQLite wrapper)
// =============================================================================
std::vector<PresetEntry> PresetManager::getAllPresets() {
    juce::ScopedLock sl(lock_);
    std::vector<PresetEntry> result;
    if (!database_) return result;

    PreparedStatement stmt;
    if (!stmt.prepare(database_, "SELECT * FROM presets ORDER BY updated_at DESC LIMIT 1000"))
        return result;

    while (stmt.step()) {
        result.push_back(rowToEntry(stmt));
    }
    return result;
}

// =============================================================================
// searchPresets [安全] - 参数化 LIKE 查询
// 使用 LIKE ?1 配合 % 通配符拼接在绑定值中
// =============================================================================
std::vector<PresetEntry> PresetManager::searchPresets(const juce::String& query) {
    juce::ScopedLock sl(lock_);
    std::vector<PresetEntry> result;
    if (!rawDb_) return result;

    // 在绑定值中构建 % 通配符，避免 SQL 注入
    juce::String likePattern = "%" + query + "%";

    PreparedStatement stmt;
    if (!stmt.prepare(rawDb_,
        "SELECT * FROM presets WHERE "
        "name LIKE ?1 OR category LIKE ?1 OR tags LIKE ?1 "
        "OR description LIKE ?1 OR ai_prompt LIKE ?1 "
        "ORDER BY rating DESC LIMIT 100"
    )) return result;

    stmt.bindText(1, likePattern);

    while (stmt.step()) {
        result.push_back(rowToEntry(stmt));
    }
    return result;
}

// =============================================================================
// getPresetsByCategory [安全] - 参数化精确匹配
// =============================================================================
std::vector<PresetEntry> PresetManager::getPresetsByCategory(const juce::String& category) {
    juce::ScopedLock sl(lock_);
    std::vector<PresetEntry> result;
    if (!rawDb_) return result;

    PreparedStatement stmt;
    if (!stmt.prepare(rawDb_,
        "SELECT * FROM presets WHERE category = ?1 ORDER BY updated_at DESC LIMIT 100"
    )) return result;

    stmt.bindText(1, category);

    while (stmt.step()) {
        result.push_back(rowToEntry(stmt));
    }
    return result;
}

// =============================================================================
// getPresetsByTag [安全] - 参数化 LIKE 查询
// =============================================================================
std::vector<PresetEntry> PresetManager::getPresetsByTag(const juce::String& tag) {
    juce::ScopedLock sl(lock_);
    std::vector<PresetEntry> result;
    if (!rawDb_) return result;

    // 在绑定值中构建 % 通配符
    juce::String likePattern = "%" + tag + "%";

    PreparedStatement stmt;
    if (!stmt.prepare(rawDb_,
        "SELECT * FROM presets WHERE tags LIKE ?1 ORDER BY updated_at DESC LIMIT 100"
    )) return result;

    stmt.bindText(1, likePattern);

    while (stmt.step()) {
        result.push_back(rowToEntry(stmt));
    }
    return result;
}

// =============================================================================
// getRecentPresets - limit 是 int 无注入风险，但保持一致性
// =============================================================================
std::vector<PresetEntry> PresetManager::getRecentPresets(int limit) {
    juce::ScopedLock sl(lock_);
    std::vector<PresetEntry> result;
    if (!rawDb_) return result;

    PreparedStatement stmt;
    if (!stmt.prepare(rawDb_,
        "SELECT * FROM presets ORDER BY updated_at DESC LIMIT ?1"
    )) return result;

    stmt.bindInt(1, limit);

    while (stmt.step()) {
        result.push_back(rowToEntry(stmt));
    }
    return result;
}

int PresetManager::getTotalPresetCount() {
    juce::ScopedLock sl(lock_);
    if (!database_) return 0;

    PreparedStatement stmt;
    if (!stmt.prepare(database_, "SELECT COUNT(*) FROM presets"))
        return 0;

    if (stmt.step()) {
        return stmt.getColumnInt(0);
    }
    return 0;
}

// =============================================================================
// savePresetVersion [安全] - 参数化 INSERT
// =============================================================================
bool PresetManager::savePresetVersion(int presetId, const juce::String& jsonData) {
    juce::ScopedLock sl(lock_);
    if (!rawDb_) return false;

    // 获取当前版本号
    int version = 1;
    {
        PreparedStatement stmt;
        if (stmt.prepare(rawDb_,
            "SELECT MAX(version) FROM preset_history WHERE preset_id = ?1"
        )) {
            stmt.bindInt(1, presetId);
            if (stmt.step() && !stmt.isColumnNull(0)) {
                version = stmt.getColumnInt(0) + 1;
            }
        }
    }

    // 限制最多50个版本
    if (version > 50) {
        PreparedStatement stmt;
        if (stmt.prepare(rawDb_,
            "DELETE FROM preset_history WHERE preset_id = ?1 AND version = 1"
        )) {
            stmt.bindInt(1, presetId);
            stmt.execute();
        }
    }

    PreparedStatement stmt;
    if (!stmt.prepare(rawDb_,
        "INSERT INTO preset_history (preset_id, version, json_data) VALUES (?1, ?2, ?3)"
    )) return false;

    stmt.bindInt(1, presetId);
    stmt.bindInt(2, version);
    stmt.bindText(3, jsonData);

    return stmt.execute();
}

std::vector<std::pair<int, juce::Time>> PresetManager::getPresetHistory(int presetId) {
    juce::ScopedLock sl(lock_);
    std::vector<std::pair<int, juce::Time>> result;
    if (!rawDb_) return result;

    PreparedStatement stmt;
    if (!stmt.prepare(rawDb_,
        "SELECT version, created_at FROM preset_history WHERE preset_id = ?1 ORDER BY version DESC"
    )) return result;

    stmt.bindInt(1, presetId);

    while (stmt.step()) {
        int version = stmt.getColumnInt(0);
        juce::Time time = juce::Time::getCurrentTime();
        result.push_back({version, time});
    }
    return result;
}

// =============================================================================
// restorePresetVersion [安全] - 参数化 SELECT + UPDATE
// =============================================================================
bool PresetManager::restorePresetVersion(int presetId, int version) {
    juce::ScopedLock sl(lock_);
    if (!rawDb_) return false;

    // 1. 获取历史版本的 json_data
    juce::String jsonData;
    {
        PreparedStatement stmt;
        if (!stmt.prepare(rawDb_,
            "SELECT json_data FROM preset_history WHERE preset_id = ?1 AND version = ?2"
        )) return false;

        stmt.bindInt(1, presetId);
        stmt.bindInt(2, version);

        if (!stmt.step()) return false;
        jsonData = stmt.getColumnText(0);
    }

    // 2. 更新到当前预设
    {
        PreparedStatement stmt;
        if (!stmt.prepare(rawDb_,
            "UPDATE presets SET json_data = ?1, updated_at = CURRENT_TIMESTAMP WHERE id = ?2"
        )) return false;

        stmt.bindText(1, jsonData);
        stmt.bindInt(2, presetId);

        return stmt.execute();
    }
}

// =============================================================================
// 导入/导出 (暂未加密 - 将在 B5-003 集成 AES256Encryptor)
// =============================================================================
bool PresetManager::exportPreset(int id, const juce::File& file) {
    PresetEntry entry;
    if (!loadPreset(id, entry)) return false;

    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("name", entry.name);
    obj->setProperty("category", entry.category);
    obj->setProperty("tags", entry.tags);
    obj->setProperty("description", entry.description);
    obj->setProperty("author", entry.author);
    obj->setProperty("jsonData", entry.jsonData);
    obj->setProperty("format", "LianCorePreset");
    obj->setProperty("version", "3.0.0");

    return file.replaceWithText(juce::JSON::toString(juce::var(obj.get())));
}

bool PresetManager::importPreset(const juce::File& file) {
    juce::String content = file.loadFileAsString();
    auto json = juce::JSON::parse(content);
    if (auto* obj = json.getDynamicObject()) {
        PresetEntry entry;
        entry.name = obj->getProperty("name").toString();
        entry.category = obj->getProperty("category").toString();
        entry.tags = obj->getProperty("tags").toString();
        entry.description = obj->getProperty("description").toString();
        entry.author = obj->getProperty("author").toString();
        entry.jsonData = obj->getProperty("jsonData").toString();
        return savePreset(entry) >= 0;
    }
    return false;
}

// =============================================================================
// 加密导入/导出 [B5-003] - AES-256-GCM 加密
// 导出格式: IV(12B) + Ciphertext + Tag(16B) → Base64 → 文件
// 文件头: "LIANCORE_ENCRYPTED_PRESET\n" + Base64 密文
// =============================================================================
bool PresetManager::exportPresetEncrypted(int id, const juce::File& file,
                                          const juce::String& password) {
    // 1. 读取预设
    PresetEntry entry;
    if (!loadPreset(id, entry)) return false;

    // 2. 构建 JSON (与 exportPreset 一致)
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("name", entry.name);
    obj->setProperty("category", entry.category);
    obj->setProperty("tags", entry.tags);
    obj->setProperty("description", entry.description);
    obj->setProperty("author", entry.author);
    obj->setProperty("jsonData", entry.jsonData);
    obj->setProperty("format", "LianCorePreset");
    obj->setProperty("version", "3.0.0");
    juce::String plainJson = juce::JSON::toString(juce::var(obj.get()));

    // 3. 从密码派生密钥 (PBKDF2)
    auto salt = LianCore::Security::AES256Encryptor::generateSalt();
    auto key = LianCore::Security::AES256Encryptor::deriveKeyFromPassword(password, salt);

    // 4. AES-256-GCM 加密
    juce::String ciphertext = LianCore::Security::AES256Encryptor::encrypt(plainJson, key);

    // 安全擦除密钥
    LianCore::Security::AES256Encryptor::secureEraseKey(key);

    // 5. 写入文件 (格式: 盐值(16B Base64) \n 密文(Base64))
    juce::String saltB64 = juce::Base64::toBase64(salt.data(), salt.size());
    juce::String fileContent = saltB64 + "\n" + ciphertext;

    return file.replaceWithText(fileContent);
}

bool PresetManager::importPresetEncrypted(const juce::File& file,
                                          const juce::String& password) {
    // 1. 读取加密文件
    juce::String content = file.loadFileAsString();
    if (content.isEmpty()) return false;

    // 2. 解析盐值和密文
    int newlinePos = content.indexOf("\n");
    if (newlinePos < 0) return false;

    juce::String saltB64 = content.substring(0, newlinePos).trim();
    juce::String ciphertextB64 = content.substring(newlinePos + 1).trim();

    // 3. 解码盐值
    juce::MemoryBlock saltBlock;
    juce::MemoryOutputStream saltMos(saltBlock, false);
    if (!juce::Base64::convertFromBase64(saltMos, saltB64)) return false;

    std::array<uint8_t, 16> salt{};
    if (saltBlock.getSize() >= 16) {
        std::memcpy(salt.data(), saltBlock.getData(), 16);
    } else {
        return false;
    }

    // 4. 从密码派生密钥
    auto key = LianCore::Security::AES256Encryptor::deriveKeyFromPassword(password, salt);

    // 5. AES-256-GCM 解密
    juce::String plainJson = LianCore::Security::AES256Encryptor::decrypt(ciphertextB64, key);

    // 安全擦除密钥
    LianCore::Security::AES256Encryptor::secureEraseKey(key);

    if (plainJson.isEmpty()) return false;  // 解密失败（密码错误或数据损坏）

    // 6. 解析 JSON 并导入
    auto json = juce::JSON::parse(plainJson);
    if (auto* obj = json.getDynamicObject()) {
        PresetEntry entry;
        entry.name = obj->getProperty("name").toString();
        entry.category = obj->getProperty("category").toString();
        entry.tags = obj->getProperty("tags").toString();
        entry.description = obj->getProperty("description").toString();
        entry.author = obj->getProperty("author").toString();
        entry.jsonData = obj->getProperty("jsonData").toString();
        return savePreset(entry) >= 0;
    }
    return false;
}

// =============================================================================
// 批量导入/导出 (P3-任务4)
// =============================================================================

bool PresetManager::exportPresetFolder(const juce::File& folder, const juce::String& category) {
    if (!folder.exists()) {
        folder.createDirectory();
    }
    if (!folder.isDirectory()) return false;

    std::vector<PresetEntry> presets;
    if (category.isNotEmpty()) {
        presets = getPresetsByCategory(category);
    } else {
        presets = getAllPresets();
    }

    int exported = 0;
    for (const auto& preset : presets) {
        // P7修复: 堆分配 DynamicObject，避免栈分配 + juce::var(&obj) 导致的
        // 引用计数在栈对象上操作，最终触发 0xC0000374 堆损坏
        juce::var objVar(new juce::DynamicObject());
        auto* obj = objVar.getDynamicObject();
        obj->setProperty("name", preset.name);
        obj->setProperty("category", preset.category);
        obj->setProperty("tags", preset.tags);
        obj->setProperty("description", preset.description);
        obj->setProperty("author", preset.author);
        obj->setProperty("jsonData", preset.jsonData);
        obj->setProperty("aiPrompt", preset.aiPrompt);
        obj->setProperty("aiConfidence", preset.aiConfidence);
        obj->setProperty("rating", preset.rating);

        // 创建安全的文件名
        juce::String safeName = preset.name.replaceCharacter(' ', '_')
            .replaceCharacter('/', '_')
            .replaceCharacter('\\', '_')
            .replaceCharacter(':', '_')
            .replaceCharacter('"', '_')
            .replaceCharacter('<', '_')
            .replaceCharacter('>', '_')
            .replaceCharacter('|', '_')
            .replaceCharacter('?', '_')
            .replaceCharacter('*', '_');

        auto presetFile = folder.getChildFile(safeName + ".lcpreset");
        juce::FileOutputStream fos(presetFile);
        if (fos.openedOk()) {
            fos.writeText(juce::JSON::toString(objVar), false, false, "\n");
            exported++;
        }
    }

    return exported > 0;
}

int PresetManager::importPresetFolder(const juce::File& folder) {
    if (!folder.isDirectory()) return 0;

    auto files = folder.findChildFiles(juce::File::findFiles, false, "*.lcpreset");
    if (files.isEmpty()) {
        // 也尝试 .json 扩展名
        files = folder.findChildFiles(juce::File::findFiles, false, "*.json");
    }

    int imported = 0;
    for (const auto& file : files) {
        juce::String content = file.loadFileAsString();
        if (content.isEmpty()) continue;

        auto json = juce::JSON::parse(content);
        if (auto* obj = json.getDynamicObject()) {
            PresetEntry entry;
            entry.name = obj->getProperty("name").toString();
            entry.category = obj->getProperty("category").toString();
            entry.tags = obj->getProperty("tags").toString();
            entry.description = obj->getProperty("description").toString();
            entry.author = obj->getProperty("author").toString();
            entry.jsonData = obj->getProperty("jsonData").toString();
            entry.aiPrompt = obj->getProperty("aiPrompt").toString();
            entry.aiConfidence = (float)obj->getProperty("aiConfidence");
            entry.rating = obj->getProperty("rating");

            if (savePreset(entry) >= 0) {
                imported++;
            }
        }
    }

    return imported;
}

// =============================================================================
// 标签自动建议 (P3-任务4)
// =============================================================================

// 基于类别和名称关键词的标签建议映射
static const std::unordered_map<std::string, std::vector<std::string>> categoryTagMap = {
    {"bass", {"bass", "low", "sub", "808", "deep", "growl", "wobble"}},
    {"lead", {"lead", "solo", "melody", "bright", "cutting", "saw"}},
    {"pad", {"pad", "ambient", "atmosphere", "slow", "evolving", "texture", "warm"}},
    {"pluck", {"pluck", "short", "percussive", "arp", "staccato", "decay"}},
    {"keys", {"keys", "piano", "electric", "bright", "dynamic"}},
    {"organ", {"organ", "drawbar", "church", "rotary", "hammond"}},
    {"brass", {"brass", "horn", "trumpet", "sax", "loud", "ensemble"}},
    {"string", {"string", "violin", "cello", "orchestral", "ensemble", "warm"}},
    {"woodwind", {"woodwind", "flute", "reed", "breathy", "soft"}},
    {"bell", {"bell", "chime", "metallic", "glassy", "bright", "percussive"}},
    {"fx", {"fx", "sfx", "weird", "noise", "riser", "downlifter", "impact"}},
    {"drum", {"drum", "kick", "snare", "hat", "percussion", "808"}},
    {"vocal", {"vocal", "choir", "voice", "formant", "ahh", "ooh"}},
    {"synth", {"synth", "analog", "digital", "retro", "vintage", "modern"}},
    {"arpeggio", {"arpeggio", "arp", "sequence", "pattern", "rhythmic"}}
};

// 名称关键词到标签的映射
static const std::unordered_map<std::string, std::string> nameKeywordTagMap = {
    {"warm", "warm"}, {"bright", "bright"}, {"dark", "dark"}, {"soft", "soft"},
    {"hard", "hard"}, {"deep", "deep"}, {"wide", "wide"}, {"narrow", "narrow"},
    {"clean", "clean"}, {"dirty", "dirty"}, {"distorted", "distorted"},
    {"reverb", "reverb"}, {"delay", "delay"}, {"chorus", "chorus"},
    {"filter", "filter"}, {"sweep", "sweep"}, {"lfo", "lfo"},
    {"mono", "mono"}, {"stereo", "stereo"}, {"poly", "poly"},
    {"vintage", "vintage"}, {"modern", "modern"}, {"analog", "analog"},
    {"digital", "digital"}, {"fm", "fm"}, {"additive", "additive"},
    {"subtractive", "subtractive"}, {"wavetable", "wavetable"},
    {"granular", "granular"}, {"modular", "modular"},
    {"cinematic", "cinematic"}, {"epic", "epic"}, {"lo-fi", "lo-fi"},
    {"lofi", "lo-fi"}, {"retro", "retro"}, {"future", "future"},
    {"aggressive", "aggressive"}, {"gentle", "gentle"}, {"smooth", "smooth"},
    {"punchy", "punchy"}, {"fat", "fat"}, {"thin", "thin"},
    {"evolving", "evolving"}, {"static", "static"}, {"motion", "motion"},
    {"attack", "attack"}, {"sustain", "sustain"}, {"release", "release"},
    {"resonance", "resonance"}, {"cutoff", "cutoff"}
};

juce::StringArray PresetManager::suggestTags(const juce::String& name, const juce::String& category) {
    juce::StringArray tags;
    std::set<juce::String> tagSet; // 去重

    // 1. 基于类别建议标签
    juce::String catLower = category.toLowerCase();
    auto catIt = categoryTagMap.find(catLower.toStdString());
    if (catIt != categoryTagMap.end()) {
        for (const auto& tag : catIt->second) {
            tagSet.insert(tag);
        }
    }

    // 2. 基于名称关键词建议标签
    juce::String nameLower = name.toLowerCase();
    for (const auto& [keyword, tag] : nameKeywordTagMap) {
        if (nameLower.contains(keyword)) {
            tagSet.insert(tag);
        }
    }

    // 3. 添加类别本身作为标签
    if (category.isNotEmpty()) {
        tagSet.insert(category.toLowerCase());
    }

    // 4. 转换为 StringArray
    for (const auto& tag : tagSet) {
        tags.add(tag);
    }

    return tags;
}

juce::StringArray PresetManager::getAllTags() {
    juce::StringArray tags;
    if (!database_) return tags;

    juce::ScopedLock sl(lock_);

    // 查询所有预设的标签
    const char* sql = "SELECT tags FROM presets";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(database_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        std::set<juce::String> tagSet;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* tagsJson = (const char*)sqlite3_column_text(stmt, 0);
            if (tagsJson) {
                juce::String tagsStr(tagsJson);
                auto json = juce::JSON::parse(tagsStr);
                if (auto* arr = json.getArray()) {
                    for (const auto& tag : *arr) {
                        if (tag.isString()) {
                            tagSet.insert(tag.toString());
                        }
                    }
                }
            }
        }

        sqlite3_finalize(stmt);

        for (const auto& tag : tagSet) {
            tags.add(tag);
        }
    }

    return tags;
}

// =============================================================================
// 模糊搜索 (P3-任务4)
// =============================================================================

// 计算 Levenshtein 编辑距离
static int levenshteinDistance(const juce::String& s1, const juce::String& s2) {
    int m = s1.length();
    int n = s2.length();

    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1, 0));

    for (int i = 0; i <= m; ++i) dp[i][0] = i;
    for (int j = 0; j <= n; ++j) dp[0][j] = j;

    for (int i = 1; i <= m; ++i) {
        for (int j = 1; j <= n; ++j) {
            int cost = (juce::CharacterFunctions::toLowerCase(s1[i - 1]) ==
                        juce::CharacterFunctions::toLowerCase(s2[j - 1])) ? 0 : 1;
            dp[i][j] = std::min({
                dp[i - 1][j] + 1,      // 删除
                dp[i][j - 1] + 1,      // 插入
                dp[i - 1][j - 1] + cost // 替换
            });
        }
    }

    return dp[m][n];
}

// 计算相似度分数 (0-1, 1=完全匹配)
static float similarityScore(const juce::String& query, const juce::String& target) {
    if (query.isEmpty() || target.isEmpty()) return 0.0f;

    int maxLen = std::max(query.length(), target.length());
    if (maxLen == 0) return 1.0f;

    int dist = levenshteinDistance(query, target);
    return 1.0f - static_cast<float>(dist) / maxLen;
}

std::vector<PresetEntry> PresetManager::fuzzySearch(const juce::String& query, int maxResults) {
    std::vector<PresetEntry> results;

    if (query.isEmpty() || !database_) return results;

    // 先精确搜索
    juce::String exactPattern = "%" + query + "%";
    auto exactResults = searchPresets(query);

    // 如果精确结果足够，直接返回
    if (exactResults.size() >= static_cast<size_t>(maxResults)) {
        return exactResults;
    }

    // 否则获取所有预设进行模糊匹配
    auto allPresets = getAllPresets();

    // 计算每个预设的相似度
    struct ScoredPreset {
        PresetEntry entry;
        float score;
    };
    std::vector<ScoredPreset> scored;

    juce::String queryLower = query.toLowerCase();

    for (auto& preset : allPresets) {
        // 检查是否已经在精确结果中
        bool inExact = false;
        for (const auto& e : exactResults) {
            if (e.id == preset.id) { inExact = true; break; }
        }
        if (inExact) continue;

        float score = 0.0f;

        // 名称相似度 (权重最高)
        float nameScore = similarityScore(queryLower, preset.name.toLowerCase());
        score += nameScore * 0.5f;

        // 前缀匹配加分
        if (preset.name.toLowerCase().startsWith(queryLower)) {
            score += 0.3f;
        }

        // 分类匹配
        float catScore = similarityScore(queryLower, preset.category.toLowerCase());
        score += catScore * 0.15f;

        // 标签匹配
        if (preset.tags.toLowerCase().contains(queryLower)) {
            score += 0.15f;
        }

        // 描述包含查询词
        if (preset.description.toLowerCase().contains(queryLower)) {
            score += 0.1f;
        }

        if (score > 0.1f) { // 阈值过滤
            scored.push_back({std::move(preset), score});
        }
    }

    // 按分数降序排序
    std::sort(scored.begin(), scored.end(),
              [](const ScoredPreset& a, const ScoredPreset& b) {
                  return a.score > b.score;
              });

    // 合并精确结果和模糊结果
    results = exactResults;
    for (const auto& sp : scored) {
        if (results.size() >= static_cast<size_t>(maxResults)) break;
        results.push_back(sp.entry);
    }

    return results;
}

// =============================================================================
// 结果集转换 (从 PreparedStatement)
// =============================================================================
PresetEntry PresetManager::rowToEntry(PreparedStatement& stmt) {
    PresetEntry entry;
    entry.id = stmt.getColumnInt(0);
    entry.name = stmt.getColumnText(1);
    entry.category = stmt.getColumnText(2);
    entry.tags = stmt.getColumnText(3);
    entry.description = stmt.getColumnText(4);
    entry.author = stmt.getColumnText(5);
    entry.jsonData = stmt.getColumnText(8);
    entry.aiPrompt = stmt.getColumnText(9);
    entry.aiConfidence = (float)stmt.getColumnFloat(10);
    entry.rating = stmt.getColumnInt(11);
    entry.usageCount = stmt.getColumnInt(12);
    return entry;
}

} // namespace LianCore