// =============================================================================
// LianCore - PresetManager 预设管理器
// SQLite预设库，支持标签搜索、版本历史、导入/导出
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <string>

namespace LianCore {

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

    // =========================================================================
    // 预设CRUD
    // =========================================================================
    int savePreset(const PresetEntry& entry);
    bool loadPreset(int id, PresetEntry& outEntry);
    bool deletePreset(int id);
    bool updatePreset(const PresetEntry& entry);

    // =========================================================================
    // 预设查询
    // =========================================================================
    std::vector<PresetEntry> getAllPresets();
    std::vector<PresetEntry> searchPresets(const juce::String& query);
    std::vector<PresetEntry> getPresetsByCategory(const juce::String& category);
    std::vector<PresetEntry> getPresetsByTag(const juce::String& tag);
    std::vector<PresetEntry> getRecentPresets(int limit = 20);
    int getTotalPresetCount();

    // =========================================================================
    // 版本历史
    // =========================================================================
    bool savePresetVersion(int presetId, const juce::String& jsonData);
    std::vector<std::pair<int, juce::Time>> getPresetHistory(int presetId);
    bool restorePresetVersion(int presetId, int version);

    // =========================================================================
    // 导入/导出
    // =========================================================================
    bool exportPreset(int id, const juce::File& file);
    bool importPreset(const juce::File& file);

private:
    bool createTables();
    PresetEntry rowToEntry(juce::ResultSet& rs);

    std::unique_ptr<juce::SQLite::Database> database_;
    juce::CriticalSection lock_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};

} // namespace LianCore