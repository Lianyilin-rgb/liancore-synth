// =============================================================================
// LianCore - ParameterTree 参数树
// 注册/管理所有VST3参数，支持批量更新、预设、撤销/重做
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <unordered_map>
#include <functional>

namespace LianCore {

// 参数映射 (AI推理结果)
#ifndef PARAMETER_MAPPING_DEFINED
#define PARAMETER_MAPPING_DEFINED
struct ParameterMapping {
    juce::String parameterId;
    float value;
    juce::String explanation; // AI生成的原因
};
#endif

// 参数树监听器
class ParameterTreeListener {
public:
    virtual ~ParameterTreeListener() = default;
    virtual void onParameterChanged(const juce::String& id, float value) = 0;
    virtual void onParameterBatchApplied() {}
};

// =============================================================================
// LianCoreParameterTree
// =============================================================================
class LianCoreParameterTree {
public:
    static constexpr int kMaxUndoSteps = 50;

    LianCoreParameterTree(juce::AudioProcessor& processor);
    ~LianCoreParameterTree();

    // =========================================================================
    // 参数注册
    // =========================================================================
    void registerParameter(const juce::String& id, const juce::String& name,
                          float defaultValue, float minValue, float maxValue,
                          float step = 0.0f, const juce::String& unit = "",
                          const juce::String& description = "",
                          const juce::String& category = "");

    void beginGroup(const juce::String& groupId, const juce::String& groupName);
    void endGroup();

    // =========================================================================
    // 参数访问
    // =========================================================================
    float getParameterValue(const juce::String& id) const;
    void setParameterValue(const juce::String& id, float value);
    void setParameter(const juce::String& id, float value) { setParameterValue(id, value); }
    juce::RangedAudioParameter* getParameter(const juce::String& id);

    // 获取所有参数ID
    std::vector<juce::String> getAllParameterIds() const;

    // =========================================================================
    // 批量更新 (AI推理结果)
    // =========================================================================
    void applyParameterBatch(const std::vector<ParameterMapping>& mappings);

    // =========================================================================
    // 参数渐变过渡 (Beta Week 7: morphTo 平滑音色切换)
    // 使用三次缓入缓出曲线，在指定时间内平滑过渡到目标参数
    // =========================================================================
    struct MorphTarget {
        juce::String parameterId;
        float targetValue;
    };

    // 启动渐变过渡 (非阻塞, 使用 Timer 驱动)
    // durationMs: 过渡时长 (毫秒), 默认 300ms
    void morphTo(const std::vector<MorphTarget>& targets, int durationMs = 300);

    // 检查是否有正在进行的渐变
    bool isMorphing() const;

    // 取消当前渐变
    void cancelMorph();

    // =========================================================================
    // 撤销/重做 (最多50步)
    // =========================================================================
    void pushUndoState();
    bool undo();
    bool redo();
    int getUndoDepth() const;
    int getRedoDepth() const;

    // =========================================================================
    // 预设管理
    // =========================================================================
    juce::var getPresetAsJson() const;
    void restorePresetFromJson(const juce::var& json);

    // =========================================================================
    // 监听器
    // =========================================================================
    void addListener(ParameterTreeListener* listener);
    void removeListener(ParameterTreeListener* listener);

    // =========================================================================
    // 获取JUCE参数树 (用于VST3接口)
    // =========================================================================
    juce::AudioProcessorValueTreeState& getState() { return state_; }

private:
    juce::AudioProcessorValueTreeState state_;
    juce::UndoManager undoManager_;

    // 参数元数据
    struct ParameterMeta {
        juce::String id;
        juce::String name;
        juce::String description;
        juce::String category;
        juce::String unit;
        float defaultValue;
        float minValue;
        float maxValue;
    };
    std::vector<ParameterMeta> parameterMeta_;

    // 监听器列表
    juce::ListenerList<ParameterTreeListener> listeners_;

    // 渐变过渡 (Beta Week 7)
    struct MorphState {
        bool active = false;
        std::vector<MorphTarget> targets;
        int durationMs = 300;
        int elapsedMs = 0;
        std::vector<float> startValues;
        std::vector<float> endValues;
    };
    MorphState morphState_;

    // 渐变更新 (由 Timer 回调)
    void updateMorphStep();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LianCoreParameterTree)
};

} // namespace LianCore