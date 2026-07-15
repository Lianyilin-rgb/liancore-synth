// =============================================================================
// LianCore - OversamplingProcessor 离线渲染4x过采样 (P2-4)
// 检测离线渲染模式，自动启用4倍过采样 + 线性相位FIR降采样
// =============================================================================
#pragma once

#include <JuceHeader.h>

namespace LianCore {

// =============================================================================
// OversamplingProcessor - 4x过采样处理器
// =============================================================================
class OversamplingProcessor {
public:
    OversamplingProcessor();
    ~OversamplingProcessor();

    // 准备过采样 (在 prepareToPlay 中调用)
    void prepare(double sampleRate, int samplesPerBlock, int numChannels);

    // 重置状态
    void reset();

    // 处理: 上采样 → 用户处理 → 降采样
    // 返回 true 表示已启用过采样处理
    bool process(juce::AudioBuffer<float>& buffer);

    // 降采样: 将过采样后的buffer降采样回原始采样率
    void downsample(juce::AudioBuffer<float>& buffer);

    // 获取过采样后的内部采样率
    double getOversampledRate() const { return oversampledRate_; }

    // 是否启用过采样
    bool isEnabled() const { return enabled_; }

    // 强制启用/禁用 (用于测试)
    void setEnabled(bool enabled);

private:
    // 使用 Kaiser 窗设计线性相位 FIR 低通滤波器
    // 用于降采样前的抗混叠
    void designAntiAliasingFilter();

    // 上采样: 插入零 + FIR 滤波
    void upsample(juce::AudioBuffer<float>& buffer);

    bool enabled_ = false;
    bool forceEnabled_ = false;
    double baseSampleRate_ = 44100.0;
    double oversampledRate_ = 176400.0;
    int numChannels_ = 2;
    int maxBlockSize_ = 1024;

    // 过采样因子
    static constexpr int oversamplingFactor_ = 4;

    // 内部缓冲区
    std::vector<juce::AudioBuffer<float>> upsampledBuffers_;
    juce::AudioBuffer<float> tempBuffer_;

    // 线性相位 FIR 抗混叠滤波器
    // 截止频率: 0.22 * Nyquist (留出过渡带)
    // 阶数: 128 (高精度离线渲染)
    std::vector<float> firCoefficients_;
    std::vector<float> firUpsampleState_;    // 上采样 FIR 滤波器状态
    std::vector<float> firDownsampleState_;  // 降采样 FIR 滤波器状态 (独立)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OversamplingProcessor)
};

} // namespace LianCore