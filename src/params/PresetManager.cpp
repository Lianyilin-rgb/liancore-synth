// =============================================================================
// LianCore - PresetManager 实现
// =============================================================================
#include "PresetManager.h"

namespace LianCore {

PresetManager::PresetManager() = default;
PresetManager::~PresetManager() { closeDatabase(); }

bool PresetManager::openDatabase(const juce::File& dbFile) {
    juce::ScopedLock sl(lock_);

    juce::File parentDir = dbFile.getParentDirectory();
    if (!parentDir.exists()) {
        parentDir.createDirectory();
    }

    database_ = std::make_unique<juce::SQLite::Database>();
    if (!database_->open(dbFile)) return false;

    return createTables();
}

void PresetManager::closeDatabase() {
    juce::ScopedLock sl(lock_);
    database_.reset();
}

bool PresetManager::isDatabaseOpen() const {
    return database_ != nullptr;
}

bool PresetManager::createTables() {
    if (!database_) return false;

    // 预设表
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

    // 版本历史表
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

int PresetManager::savePreset(const PresetEntry& entry) {
    juce::ScopedLock sl(lock_);
    if (!database_) return -1;

    juce::String sql = juce::String::formatted(
        "INSERT INTO presets (name, category, tags, description, author, json_data, ai_prompt, ai_confidence) "
        "VALUES ('%s', '%s', '%s', '%s', '%s', '%s', '%s', %f)",
        entry.name.replace("'", "''").toRawUTF8(),
        entry.category.replace("'", "''").toRawUTF8(),
        entry.tags.replace("'", "''").toRawUTF8(),
        entry.description.replace("'", "''").toRawUTF8(),
        entry.author.replace("'", "''").toRawUTF8(),
        entry.jsonData.replace("'", "''").toRawUTF8(),
        entry.aiPrompt.replace("'", "''").toRawUTF8(),
        entry.aiConfidence
    );

    database_->executeStatement(sql);
    return database_->getLastInsertRowId();
}

bool PresetManager::loadPreset(int id, PresetEntry& outEntry) {
    juce::ScopedLock sl(lock_);
    if (!database_) return false;

    juce::String sql = juce::String::formatted("SELECT * FROM presets WHERE id = %d", id);
    auto rs = database_->execute(sql);
    if (!rs.next()) return false;

    outEntry = rowToEntry(rs);
    return true;
}

bool PresetManager::deletePreset(int id) {
    juce::ScopedLock sl(lock_);
    if (!database_) return false;

    database_->executeStatement(juce::String::formatted("DELETE FROM presets WHERE id = %d", id));
    return true;
}

bool PresetManager::updatePreset(const PresetEntry& entry) {
    juce::ScopedLock sl(lock_);
    if (!database_) return false;

    juce::String sql = juce::String::formatted(
        "UPDATE presets SET name='%s', category='%s', tags='%s', description='%s', "
        "json_data='%s', ai_prompt='%s', ai_confidence=%f, rating=%d, "
        "usage_count=%d, updated_at=CURRENT_TIMESTAMP WHERE id=%d",
        entry.name.replace("'", "''").toRawUTF8(),
        entry.category.replace("'", "''").toRawUTF8(),
        entry.tags.replace("'", "''").toRawUTF8(),
        entry.description.replace("'", "''").toRawUTF8(),
        entry.jsonData.replace("'", "''").toRawUTF8(),
        entry.aiPrompt.replace("'", "''").toRawUTF8(),
        entry.aiConfidence,
        entry.rating,
        entry.usageCount,
        entry.id
    );

    database_->executeStatement(sql);
    return true;
}

std::vector<PresetEntry> PresetManager::getAllPresets() {
    juce::ScopedLock sl(lock_);
    std::vector<PresetEntry> result;
    if (!database_) return result;

    auto rs = database_->execute("SELECT * FROM presets ORDER BY updated_at DESC LIMIT 1000");
    while (rs.next()) {
        result.push_back(rowToEntry(rs));
    }
    return result;
}

std::vector<PresetEntry> PresetManager::searchPresets(const juce::String& query) {
    juce::ScopedLock sl(lock_);
    std::vector<PresetEntry> result;
    if (!database_) return result;

    juce::String sql = juce::String::formatted(
        "SELECT * FROM presets WHERE name LIKE '%%%s%%' OR category LIKE '%%%s%%' "
        "OR tags LIKE '%%%s%%' OR description LIKE '%%%s%%' OR ai_prompt LIKE '%%%s%%' "
        "ORDER BY rating DESC LIMIT 100",
        query.toRawUTF8(), query.toRawUTF8(), query.toRawUTF8(),
        query.toRawUTF8(), query.toRawUTF8()
    );

    auto rs = database_->execute(sql);
    while (rs.next()) {
        result.push_back(rowToEntry(rs));
    }
    return result;
}

std::vector<PresetEntry> PresetManager::getPresetsByCategory(const juce::String& category) {
    juce::ScopedLock sl(lock_);
    std::vector<PresetEntry> result;
    if (!database_) return result;

    juce::String sql = juce::String::formatted(
        "SELECT * FROM presets WHERE category = '%s' ORDER BY updated_at DESC LIMIT 100",
        category.replace("'", "''").toRawUTF8()
    );

    auto rs = database_->execute(sql);
    while (rs.next()) {
        result.push_back(rowToEntry(rs));
    }
    return result;
}

std::vector<PresetEntry> PresetManager::getPresetsByTag(const juce::String& tag) {
    juce::ScopedLock sl(lock_);
    std::vector<PresetEntry> result;
    if (!database_) return result;

    juce::String sql = juce::String::formatted(
        "SELECT * FROM presets WHERE tags LIKE '%%%s%%' ORDER BY updated_at DESC LIMIT 100",
        tag.replace("'", "''").toRawUTF8()
    );

    auto rs = database_->execute(sql);
    while (rs.next()) {
        result.push_back(rowToEntry(rs));
    }
    return result;
}

std::vector<PresetEntry> PresetManager::getRecentPresets(int limit) {
    juce::ScopedLock sl(lock_);
    std::vector<PresetEntry> result;
    if (!database_) return result;

    auto rs = database_->execute(
        juce::String::formatted("SELECT * FROM presets ORDER BY updated_at DESC LIMIT %d", limit));
    while (rs.next()) {
        result.push_back(rowToEntry(rs));
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

bool PresetManager::savePresetVersion(int presetId, const juce::String& jsonData) {
    juce::ScopedLock sl(lock_);
    if (!database_) return false;

    // 获取当前版本号
    int version = 1;
    auto rs = database_->execute(
        juce::String::formatted("SELECT MAX(version) FROM preset_history WHERE preset_id = %d", presetId));
    if (rs.next() && !rs.getColumnValue(0).isNull()) {
        version = rs.getColumnValue(0).getIntValue() + 1;
    }

    // 限制最多50个版本
    if (version > 50) {
        database_->executeStatement(
            juce::String::formatted("DELETE FROM preset_history WHERE preset_id = %d AND version = 1", presetId));
    }

    juce::String sql = juce::String::formatted(
        "INSERT INTO preset_history (preset_id, version, json_data) VALUES (%d, %d, '%s')",
        presetId, version, jsonData.replace("'", "''").toRawUTF8()
    );

    database_->executeStatement(sql);
    return true;
}

std::vector<std::pair<int, juce::Time>> PresetManager::getPresetHistory(int presetId) {
    juce::ScopedLock sl(lock_);
    std::vector<std::pair<int, juce::Time>> result;
    if (!database_) return result;

    auto rs = database_->execute(
        juce::String::formatted("SELECT version, created_at FROM preset_history WHERE preset_id = %d ORDER BY version DESC",
                               presetId));
    while (rs.next()) {
        int version = rs.getColumnValue(0).getIntValue();
        juce::Time time = juce::Time::getCurrentTime(); // 简化: 实际应从数据库解析
        result.push_back({version, time});
    }
    return result;
}

bool PresetManager::restorePresetVersion(int presetId, int version) {
    juce::ScopedLock sl(lock_);
    if (!database_) return false;

    auto rs = database_->execute(
        juce::String::formatted(
            "SELECT json_data FROM preset_history WHERE preset_id = %d AND version = %d",
            presetId, version));

    if (rs.next()) {
        juce::String jsonData = rs.getColumnValue(0).toString();
        database_->executeStatement(
            juce::String::formatted(
                "UPDATE presets SET json_data = '%s', updated_at = CURRENT_TIMESTAMP WHERE id = %d",
                jsonData.replace("'", "''").toRawUTF8(), presetId));
        return true;
    }
    return false;
}

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

PresetEntry PresetManager::rowToEntry(juce::ResultSet& rs) {
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