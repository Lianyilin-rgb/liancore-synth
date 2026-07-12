// =============================================================================
// LianCore - PresetManager 实现 (安全修复版)
// [B5-001] 所有用户输入查询已迁移至 PreparedStatement 参数化查询
// [B5-003] 集成 AES256Encryptor 实现加密导出/导入
// 参考: OWASP SQL Injection Prevention, CWE-89
// =============================================================================
#include "PresetManager.h"
#include "PreparedStatement.h"
#include "AES256Encryptor.h"

extern "C" {
#include <sqlite3.h>
}

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

    // 打开 JUCE 封装的数据库 (用于简单查询)
    database_ = std::make_unique<juce::SQLite::Database>();
    if (!database_->open(dbFile)) return false;

    // 同时打开原始 sqlite3* 句柄 (用于参数化查询)
    int rc = sqlite3_open_v2(
        dbFile.getFullPathName().toRawUTF8(),
        &rawDb_,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
        nullptr
    );
    if (rc != SQLITE_OK && rawDb_) {
        sqlite3_close(rawDb_);
        rawDb_ = nullptr;
    }

    // 启用 WAL 模式提升并发性能
    if (rawDb_) {
        sqlite3_exec(rawDb_, "PRAGMA journal_mode=WAL", nullptr, nullptr, nullptr);
        sqlite3_exec(rawDb_, "PRAGMA foreign_keys=ON", nullptr, nullptr, nullptr);
    }

    return createTables();
}

void PresetManager::closeDatabase() {
    juce::ScopedLock sl(lock_);
    database_.reset();
    if (rawDb_) {
        sqlite3_close(rawDb_);
        rawDb_ = nullptr;
    }
}

bool PresetManager::isDatabaseOpen() const {
    return database_ != nullptr && rawDb_ != nullptr;
}

bool PresetManager::createTables() {
    if (!database_) return false;

    database_->executeStatement(
        "CREATE TABLE IF NOT EXISTS presets ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "category TEXT,"
        "tags TEXT,"
        "description TEXT,"
        "author TEXT,"
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "json_data TEXT NOT NULL,"
        "ai_prompt TEXT,"
        "ai_confidence REAL DEFAULT 0,"
        "rating INTEGER DEFAULT 0,"
        "usage_count INTEGER DEFAULT 0"
        ")"
    );

    database_->executeStatement(
        "CREATE TABLE IF NOT EXISTS preset_history ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "preset_id INTEGER,"
        "version INTEGER,"
        "json_data TEXT NOT NULL,"
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "FOREIGN KEY (preset_id) REFERENCES presets(id)"
        ")"
    );

    return true;
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
// getAllPresets - 无用户输入，使用 JUCE 数据库
// =============================================================================
std::vector<PresetEntry> PresetManager::getAllPresets() {
    juce::ScopedLock sl(lock_);
    std::vector<PresetEntry> result;
    if (!database_) return result;

    auto rs = database_->execute("SELECT * FROM presets ORDER BY updated_at DESC LIMIT 1000");
    while (rs.next()) {
        result.push_back(rowToEntryFromRS(rs));
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

    auto rs = database_->execute("SELECT COUNT(*) FROM presets");
    if (rs.next()) {
        return rs.getColumnValue(0).getIntValue();
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
    if (!juce::Base64::convertFromBase64(saltBlock, saltB64)) return false;

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

// =============================================================================
// 结果集转换 (从 JUCE ResultSet - 兼容 getAllPresets)
// =============================================================================
PresetEntry PresetManager::rowToEntryFromRS(juce::ResultSet& rs) {
    PresetEntry entry;
    entry.id = rs.getColumnValue(0).getIntValue();
    entry.name = rs.getColumnValue(1).toString();
    entry.category = rs.getColumnValue(2).toString();
    entry.tags = rs.getColumnValue(3).toString();
    entry.description = rs.getColumnValue(4).toString();
    entry.author = rs.getColumnValue(5).toString();
    entry.jsonData = rs.getColumnValue(8).toString();
    entry.aiPrompt = rs.getColumnValue(9).toString();
    entry.aiConfidence = rs.getColumnValue(10).getFloatValue();
    entry.rating = rs.getColumnValue(11).getIntValue();
    entry.usageCount = rs.getColumnValue(12).getIntValue();
    return entry;
}

} // namespace LianCore