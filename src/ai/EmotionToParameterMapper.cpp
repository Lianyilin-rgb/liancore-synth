// =============================================================================
// LianCore - EmotionToParameterMapper 实现
// 八锚点立方体 + 三线性插值 + 直接映射规则
// =============================================================================
#include "EmotionToParameterMapper.h"

namespace LianCore {

EmotionToParameterMapper::EmotionToParameterMapper() {
    initDefaultAnchors();
}

EmotionToParameterMapper::~EmotionToParameterMapper() = default;

// =============================================================================
// 初始化 8 个锚点预设 (情感立方体顶点)
// 每个锚点定义一组完整的合成器参数值
// =============================================================================
void EmotionToParameterMapper::initDefaultAnchors() {
    // 锚点 0: (0,0,0) 暗柔铺底 - 氛围音乐、冥想
    anchors_[0] = {
        "暗柔铺底", "氛围音乐、冥想",
        0.0f, 0.0f, 0.0f,
        {
            {"filter_cutoff", 0.25f}, {"filter_resonance", 0.15f},
            {"osc_waveform", 0.2f}, {"env_attack", 0.55f},
            {"env_decay", 0.5f}, {"env_sustain", 0.7f},
            {"env_release", 0.65f}, {"lfo_rate", 0.1f},
            {"lfo_depth", 0.05f}, {"reverb_size", 0.35f},
            {"reverb_mix", 0.25f}, {"distortion_drive", 0.0f},
            {"noise_level", 0.05f}, {"osc_detune", 0.0f},
            {"osc_fm_depth", 0.0f}, {"osc_volume", 0.55f},
            {"osc_unison", 0.0f}, {"delay_feedback", 0.1f},
            {"delay_mix", 0.1f}, {"eq_low_gain", 0.4f},
            {"eq_mid_gain", 0.5f}, {"eq_high_gain", 0.55f},
            {"compressor_ratio", 0.2f}, {"warp_amount", 0.0f},
        }
    };

    // 锚点 1: (0,0,1) 紧张暗铺底 - 恐怖配乐、悬疑
    anchors_[1] = {
        "紧张暗铺底", "恐怖配乐、悬疑",
        0.0f, 0.0f, 1.0f,
        {
            {"filter_cutoff", 0.2f}, {"filter_resonance", 0.65f},
            {"osc_waveform", 0.15f}, {"env_attack", 0.5f},
            {"env_decay", 0.45f}, {"env_sustain", 0.6f},
            {"env_release", 0.55f}, {"lfo_rate", 0.65f},
            {"lfo_depth", 0.45f}, {"reverb_size", 0.7f},
            {"reverb_mix", 0.15f}, {"distortion_drive", 0.15f},
            {"noise_level", 0.2f}, {"osc_detune", 0.35f},
            {"osc_fm_depth", 0.5f}, {"osc_volume", 0.5f},
            {"osc_unison", 0.1f}, {"delay_feedback", 0.55f},
            {"delay_mix", 0.25f}, {"eq_low_gain", 0.35f},
            {"eq_mid_gain", 0.45f}, {"eq_high_gain", 0.4f},
            {"compressor_ratio", 0.25f}, {"warp_amount", 0.45f},
        }
    };

    // 锚点 2: (0,1,0) 强力低音 - EDM、Dubstep
    anchors_[2] = {
        "强力低音", "EDM、Dubstep",
        0.0f, 1.0f, 0.0f,
        {
            {"filter_cutoff", 0.35f}, {"filter_resonance", 0.3f},
            {"osc_waveform", 0.45f}, {"env_attack", 0.05f},
            {"env_decay", 0.3f}, {"env_sustain", 0.8f},
            {"env_release", 0.15f}, {"lfo_rate", 0.2f},
            {"lfo_depth", 0.1f}, {"reverb_size", 0.2f},
            {"reverb_mix", 0.1f}, {"distortion_drive", 0.65f},
            {"noise_level", 0.1f}, {"osc_detune", 0.05f},
            {"osc_fm_depth", 0.1f}, {"osc_volume", 0.85f},
            {"osc_unison", 0.6f}, {"delay_feedback", 0.15f},
            {"delay_mix", 0.05f}, {"eq_low_gain", 0.7f},
            {"eq_mid_gain", 0.4f}, {"eq_high_gain", 0.35f},
            {"compressor_ratio", 0.7f}, {"warp_amount", 0.1f},
        }
    };

    // 锚点 3: (0,1,1) 激进失真 - 工业、金属
    anchors_[3] = {
        "激进失真", "工业、金属",
        0.0f, 1.0f, 1.0f,
        {
            {"filter_cutoff", 0.6f}, {"filter_resonance", 0.7f},
            {"osc_waveform", 0.7f}, {"env_attack", 0.03f},
            {"env_decay", 0.25f}, {"env_sustain", 0.75f},
            {"env_release", 0.1f}, {"lfo_rate", 0.7f},
            {"lfo_depth", 0.5f}, {"reverb_size", 0.15f},
            {"reverb_mix", 0.08f}, {"distortion_drive", 0.9f},
            {"noise_level", 0.3f}, {"osc_detune", 0.4f},
            {"osc_fm_depth", 0.6f}, {"osc_volume", 0.9f},
            {"osc_unison", 0.7f}, {"delay_feedback", 0.5f},
            {"delay_mix", 0.2f}, {"eq_low_gain", 0.55f},
            {"eq_mid_gain", 0.6f}, {"eq_high_gain", 0.5f},
            {"compressor_ratio", 0.85f}, {"warp_amount", 0.5f},
        }
    };

    // 锚点 4: (1,0,0) 温暖铺底 - Lo-Fi、爵士
    anchors_[4] = {
        "温暖铺底", "Lo-Fi、爵士",
        1.0f, 0.0f, 0.0f,
        {
            {"filter_cutoff", 0.45f}, {"filter_resonance", 0.35f},
            {"osc_waveform", 0.25f}, {"env_attack", 0.5f},
            {"env_decay", 0.55f}, {"env_sustain", 0.7f},
            {"env_release", 0.7f}, {"lfo_rate", 0.15f},
            {"lfo_depth", 0.08f}, {"reverb_size", 0.55f},
            {"reverb_mix", 0.35f}, {"distortion_drive", 0.15f},
            {"noise_level", 0.08f}, {"osc_detune", 0.02f},
            {"osc_fm_depth", 0.0f}, {"osc_volume", 0.55f},
            {"osc_unison", 0.1f}, {"delay_feedback", 0.2f},
            {"delay_mix", 0.15f}, {"eq_low_gain", 0.6f},
            {"eq_mid_gain", 0.5f}, {"eq_high_gain", 0.4f},
            {"compressor_ratio", 0.2f}, {"warp_amount", 0.0f},
        }
    };

    // 锚点 5: (1,0,1) 紧张氛围 - 科幻配乐
    anchors_[5] = {
        "紧张氛围", "科幻配乐",
        1.0f, 0.0f, 1.0f,
        {
            {"filter_cutoff", 0.5f}, {"filter_resonance", 0.6f},
            {"osc_waveform", 0.5f}, {"env_attack", 0.35f},
            {"env_decay", 0.4f}, {"env_sustain", 0.6f},
            {"env_release", 0.5f}, {"lfo_rate", 0.6f},
            {"lfo_depth", 0.4f}, {"reverb_size", 0.75f},
            {"reverb_mix", 0.3f}, {"distortion_drive", 0.2f},
            {"noise_level", 0.15f}, {"osc_detune", 0.3f},
            {"osc_fm_depth", 0.45f}, {"osc_volume", 0.55f},
            {"osc_unison", 0.2f}, {"delay_feedback", 0.5f},
            {"delay_mix", 0.3f}, {"eq_low_gain", 0.45f},
            {"eq_mid_gain", 0.5f}, {"eq_high_gain", 0.55f},
            {"compressor_ratio", 0.3f}, {"warp_amount", 0.4f},
        }
    };

    // 锚点 6: (1,1,0) 明亮主音 - 流行、Trance
    anchors_[6] = {
        "明亮主音", "流行、Trance",
        1.0f, 1.0f, 0.0f,
        {
            {"filter_cutoff", 0.75f}, {"filter_resonance", 0.4f},
            {"osc_waveform", 0.55f}, {"env_attack", 0.08f},
            {"env_decay", 0.35f}, {"env_sustain", 0.8f},
            {"env_release", 0.25f}, {"lfo_rate", 0.25f},
            {"lfo_depth", 0.15f}, {"reverb_size", 0.4f},
            {"reverb_mix", 0.2f}, {"distortion_drive", 0.35f},
            {"noise_level", 0.05f}, {"osc_detune", 0.08f},
            {"osc_fm_depth", 0.1f}, {"osc_volume", 0.85f},
            {"osc_unison", 0.55f}, {"delay_feedback", 0.2f},
            {"delay_mix", 0.1f}, {"eq_low_gain", 0.55f},
            {"eq_mid_gain", 0.55f}, {"eq_high_gain", 0.6f},
            {"compressor_ratio", 0.5f}, {"warp_amount", 0.05f},
        }
    };

    // 锚点 7: (1,1,1) 复杂合成 - 实验、IDM
    anchors_[7] = {
        "复杂合成", "实验、IDM",
        1.0f, 1.0f, 1.0f,
        {
            {"filter_cutoff", 0.8f}, {"filter_resonance", 0.7f},
            {"osc_waveform", 0.8f}, {"env_attack", 0.05f},
            {"env_decay", 0.3f}, {"env_sustain", 0.7f},
            {"env_release", 0.2f}, {"lfo_rate", 0.7f},
            {"lfo_depth", 0.55f}, {"reverb_size", 0.5f},
            {"reverb_mix", 0.25f}, {"distortion_drive", 0.55f},
            {"noise_level", 0.2f}, {"osc_detune", 0.35f},
            {"osc_fm_depth", 0.55f}, {"osc_volume", 0.85f},
            {"osc_unison", 0.65f}, {"delay_feedback", 0.45f},
            {"delay_mix", 0.25f}, {"eq_low_gain", 0.5f},
            {"eq_mid_gain", 0.6f}, {"eq_high_gain", 0.6f},
            {"compressor_ratio", 0.65f}, {"warp_amount", 0.5f},
        }
    };
}

// =============================================================================
// 三线性插值
// 在单位立方体的8个顶点值之间进行三维线性插值
// cXYZ: X=warmth, Y=energy, Z=tension   wx/wy/wz: 归一化坐标 (0~1)
// =============================================================================
float EmotionToParameterMapper::trilinearInterpolate(
    float c000, float c001, float c010, float c011,
    float c100, float c101, float c110, float c111,
    float wx, float wy, float wz) {

    // 沿 warmth 轴 (X) 插值
    float c00 = c000 + (c100 - c000) * wx;
    float c01 = c001 + (c101 - c001) * wx;
    float c10 = c010 + (c110 - c010) * wx;
    float c11 = c011 + (c111 - c011) * wx;

    // 沿 energy 轴 (Y) 插值
    float c0 = c00 + (c10 - c00) * wy;
    float c1 = c01 + (c11 - c01) * wy;

    // 沿 tension 轴 (Z) 插值
    return c0 + (c1 - c0) * wz;
}

// =============================================================================
// 收集所有锚点中出现的参数ID (去重)
// =============================================================================
std::vector<juce::String> EmotionToParameterMapper::collectAllParameterIds() const {
    std::unordered_set<juce::String> ids;
    for (const auto& anchor : anchors_) {
        for (const auto& [id, value] : anchor.parameters) {
            ids.insert(id);
        }
    }
    return std::vector<juce::String>(ids.begin(), ids.end());
}

// =============================================================================
// 核心映射: 情感向量 → 参数映射列表 (三线性插值)
// =============================================================================
std::vector<ParameterMapping> EmotionToParameterMapper::mapEmotionToParameters(
    float warmth, float energy, float tension) const {

    std::vector<ParameterMapping> result;

    // 钳制输入范围
    warmth  = juce::jlimit(0.0f, 1.0f, warmth);
    energy  = juce::jlimit(0.0f, 1.0f, energy);
    tension = juce::jlimit(0.0f, 1.0f, tension);

    // 收集所有参数ID
    auto allParamIds = collectAllParameterIds();

    for (const auto& paramId : allParamIds) {
        // 从8个锚点获取该参数的值
        float c000 = anchors_[0].parameters.count(paramId) ? anchors_[0].parameters.at(paramId) : 0.5f;
        float c001 = anchors_[1].parameters.count(paramId) ? anchors_[1].parameters.at(paramId) : 0.5f;
        float c010 = anchors_[2].parameters.count(paramId) ? anchors_[2].parameters.at(paramId) : 0.5f;
        float c011 = anchors_[3].parameters.count(paramId) ? anchors_[3].parameters.at(paramId) : 0.5f;
        float c100 = anchors_[4].parameters.count(paramId) ? anchors_[4].parameters.at(paramId) : 0.5f;
        float c101 = anchors_[5].parameters.count(paramId) ? anchors_[5].parameters.at(paramId) : 0.5f;
        float c110 = anchors_[6].parameters.count(paramId) ? anchors_[6].parameters.at(paramId) : 0.5f;
        float c111 = anchors_[7].parameters.count(paramId) ? anchors_[7].parameters.at(paramId) : 0.5f;

        // 三线性插值
        float interpolated = trilinearInterpolate(
            c000, c001, c010, c011,
            c100, c101, c110, c111,
            warmth, energy, tension);

        ParameterMapping mapping;
        mapping.parameterId = paramId;
        mapping.value = juce::jlimit(0.0f, 1.0f, interpolated);
        mapping.explanation = juce::String::formatted(
            "情感映射: W=%.2f E=%.2f T=%.2f", warmth, energy, tension);
        result.push_back(mapping);
    }

    return result;
}

// =============================================================================
// 直接映射规则 (快速路径, 无锚点依赖)
// 基于设计文档中定义的线性映射公式
// =============================================================================
std::vector<ParameterMapping> EmotionToParameterMapper::mapEmotionDirect(
    float warmth, float energy, float tension) {

    std::vector<ParameterMapping> result;

    warmth  = juce::jlimit(0.0f, 1.0f, warmth);
    energy  = juce::jlimit(0.0f, 1.0f, energy);
    tension = juce::jlimit(0.0f, 1.0f, tension);

    auto addMapping = [&](const juce::String& id, float value, const juce::String& desc) {
        ParameterMapping m;
        m.parameterId = id;
        m.value = juce::jlimit(0.0f, 1.0f, value);
        m.explanation = desc;
        result.push_back(m);
    };

    // --- 温暖度 (Warmth) → 频谱色彩 ---
    addMapping("filter_cutoff",   0.25f + warmth * 0.55f,  "温暖度: 截止频率");
    addMapping("filter_resonance", 0.1f + warmth * 0.4f,   "温暖度: 共振峰");
    addMapping("distortion_drive",  warmth * 0.4f,          "温暖度: 温和驱动");
    addMapping("eq_low_gain",      0.35f + warmth * 0.25f, "温暖度: 低频增益");
    addMapping("eq_high_gain",     0.55f - warmth * 0.15f, "温暖度: 高频衰减");
    addMapping("reverb_mix",       0.1f + warmth * 0.3f,   "温暖度: 混响量");

    // --- 能量感 (Energy) → 动态响应 ---
    addMapping("env_attack",       0.55f - energy * 0.5f,  "能量感: 起音速度");
    addMapping("env_release",      0.65f - energy * 0.5f,  "能量感: 释音时间");
    addMapping("osc_volume",       0.5f + energy * 0.4f,   "能量感: 音量");
    addMapping("osc_unison",       energy * 0.7f,           "能量感: 齐奏加厚");
    addMapping("compressor_ratio", 0.2f + energy * 0.65f,  "能量感: 压缩比");
    addMapping("distortion_drive", energy * 0.7f,           "能量感: 激进驱动");

    // --- 紧张度 (Tension) → 调制与不和谐度 ---
    addMapping("osc_detune",       tension * 0.4f,          "紧张度: 失谐");
    addMapping("osc_fm_depth",     tension * 0.6f,          "紧张度: FM深度");
    addMapping("lfo_rate",         0.1f + tension * 0.6f,   "紧张度: LFO速率");
    addMapping("lfo_depth",        tension * 0.5f,           "紧张度: LFO深度");
    addMapping("delay_feedback",   0.1f + tension * 0.5f,   "紧张度: 延迟反馈");
    addMapping("filter_resonance", 0.1f + tension * 0.6f,   "紧张度: 共振尖锐");
    addMapping("warp_amount",      tension * 0.5f,           "紧张度: 变形量");

    return result;
}

// =============================================================================
// 锚点管理
// =============================================================================
void EmotionToParameterMapper::setAnchorPreset(int index, const AnchorPreset& preset) {
    if (index >= 0 && index < 8) {
        anchors_[index] = preset;
    }
}

AnchorPreset EmotionToParameterMapper::getAnchorPreset(int index) const {
    if (index >= 0 && index < 8) {
        return anchors_[index];
    }
    return {};
}

int EmotionToParameterMapper::getNearestAnchorIndex(
    float warmth, float energy, float tension) const {

    warmth  = juce::jlimit(0.0f, 1.0f, warmth);
    energy  = juce::jlimit(0.0f, 1.0f, energy);
    tension = juce::jlimit(0.0f, 1.0f, tension);

    int nearest = 0;
    float minDist = std::numeric_limits<float>::max();

    for (int i = 0; i < 8; ++i) {
        float dw = warmth  - anchors_[i].warmth;
        float de = energy  - anchors_[i].energy;
        float dt = tension - anchors_[i].tension;
        float dist = dw * dw + de * de + dt * dt; // 欧氏距离平方
        if (dist < minDist) {
            minDist = dist;
            nearest = i;
        }
    }

    return nearest;
}

bool EmotionToParameterMapper::isValidEmotionVector(
    float warmth, float energy, float tension) {
    return warmth  >= 0.0f && warmth  <= 1.0f
        && energy  >= 0.0f && energy  <= 1.0f
        && tension >= 0.0f && tension <= 1.0f;
}

} // namespace LianCore