// =============================================================================
// LianCore - EmotionToParameterMapper 情感→参数映射器
// 八锚点立方体 + 三线性插值，将三维情感向量映射到合成器参数空间
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <array>
#include "../params/ParameterTree.h"

namespace LianCore {

// =============================================================================
// 锚点预设: 情感立方体 8 个顶点
// =============================================================================
struct AnchorPreset {
    juce::String name;       // 预设名称
    juce::String description; // 典型场景描述
    float warmth = 0.0f;     // 温暖度 (0~1)
    float energy = 0.0f;     // 能量感 (0~1)
    float tension = 0.0f;    // 紧张度 (0~1)

    // 参数表: 参数ID → 目标值
    std::unordered_map<juce::String, float> parameters;
};

// =============================================================================
// EmotionToParameterMapper
// =============================================================================
class EmotionToParameterMapper {
public:
    EmotionToParameterMapper();
    ~EmotionToParameterMapper();

    // =========================================================================
    // 核心映射: 情感向量 → 参数映射列表
    // 使用三线性插值在8个锚点之间插值
    // =========================================================================
    std::vector<ParameterMapping> mapEmotionToParameters(
        float warmth, float energy, float tension) const;

    // =========================================================================
    // 锚点预设管理
    // =========================================================================
    const std::array<AnchorPreset, 8>& getAnchorPresets() const { return anchors_; }
    void setAnchorPreset(int index, const AnchorPreset& preset);
    AnchorPreset getAnchorPreset(int index) const;

    // 获取最近的锚点 (按欧氏距离)
    int getNearestAnchorIndex(float warmth, float energy, float tension) const;

    // =========================================================================
    // 情感向量有效性检查
    // =========================================================================
    static bool isValidEmotionVector(float warmth, float energy, float tension);

    // =========================================================================
    // 直接映射规则 (快速路径, 不依赖锚点)
    // =========================================================================
    static std::vector<ParameterMapping> mapEmotionDirect(
        float warmth, float energy, float tension);

private:
    // 8 个锚点预设 (情感立方体顶点)
    std::array<AnchorPreset, 8> anchors_;

    // 初始化默认锚点
    void initDefaultAnchors();

    // 三线性插值
    static float trilinearInterpolate(
        float c000, float c001, float c010, float c011,
        float c100, float c101, float c110, float c111,
        float wx, float wy, float wz);

    // 收集所有锚点中出现的参数ID
    std::vector<juce::String> collectAllParameterIds() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EmotionToParameterMapper)
};

} // namespace LianCore