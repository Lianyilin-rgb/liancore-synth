// =============================================================================
// LianCore - EQ 多段参数均衡器
// 8段独立可配置参数均衡器，支持6种滤波器类型
// 串行处理架构：输入 -> 频段0 -> 频段1 -> ... -> 频段7 -> 输出
// 双二阶滤波器使用转置直接II型结构
// =============================================================================
#pragma once

#include "../core/AudioNode.h"
#include "../utils/AudioUtils.h"
#include <JuceHeader.h>
#include <array>

namespace LianCore {

// =============================================================================
// 均衡器频段类型枚举
// =============================================================================
enum class EQBandType {
    Peaking,    // 峰值/钟形滤波器 (Bell)
    LowShelf,   // 低频搁架滤波器
    HighShelf,  // 高频搁架滤波器
    LowPass,    // 低通滤波器 12dB/oct (Butterworth)
    HighPass,   // 高通滤波器 12dB/oct (Butterworth)
    Notch,      // 陷波滤波器 (窄带带阻)
    BandPass,   // 带通滤波器 (恒定裙边增益)
};

// =============================================================================
// EQ 多段参数均衡器
// =============================================================================
class EQ : public AudioNode {
public:
    EQ(const juce::String& name = "均衡器");

    // =========================================================================
    // 生命周期
    // =========================================================================
    void prepareToPlay(double sampleRate, int maxSamplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void releaseResources() override;

    // =========================================================================
    // 参数接口 (扁平参数方案)
    // 每个频段5个参数: 类型, 频率, 增益, Q值, 旁路
    // 频段0: 参数0-4, 频段1: 参数5-9, ..., 频段7: 参数35-39
    // 参数40: 输出增益, 参数41: 干湿比
    // =========================================================================
    int getNumParameters() const override { return 42; }
    float getParameter(int index) const override;
    void setParameter(int index, float value) override;
    juce::String getParameterName(int index) const override;
    juce::String getParameterText(int index) const override;

    // =========================================================================
    // 序列化
    // =========================================================================
    juce::var toJson() const override;
    void fromJson(const juce::var& json) override;

    // =========================================================================
    // 便捷访问接口
    // =========================================================================
    void setBandType(int band, EQBandType type);
    void setBandFrequency(int band, float hz);
    void setBandGain(int band, float db);
    void setBandQ(int band, float q);
    void setBandBypass(int band, bool bypass);
    void setOutputGain(float gain);
    void setMix(float mix);

    EQBandType getBandType(int band) const;
    float getBandFrequency(int band) const;
    float getBandGain(int band) const;
    float getBandQ(int band) const;
    bool getBandBypass(int band) const;
    float getOutputGain() const;
    float getMix() const;

private:
    // =========================================================================
    // 辅助方法
    // =========================================================================

    // 归一化值 [0,1] -> 频率 [20Hz, 20000Hz] (对数刻度)
    float frequencyToHz(float normalized) const;

    // 频率 [20Hz, 20000Hz] -> 归一化值 [0,1] (对数刻度)
    float hzToFrequency(float hz) const;

    // 重新计算指定频段的双二阶滤波器系数
    void updateCoefficients(int band);

    // =========================================================================
    // 内部数据结构
    // =========================================================================

    // 双二阶滤波器状态 (转置直接II型)
    struct BiquadState {
        float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float w1 = 0.0f, w2 = 0.0f; // 转置直接II型状态变量
    };

    // 单个频段状态
    struct BandState {
        EQBandType type = EQBandType::Peaking;
        float frequency = 1000.0f;  // 中心频率 (Hz)
        float gain = 0.0f;          // 增益 (dB), -18 到 +18
        float q = 1.0f;             // Q值, 0.1 到 10.0
        bool bypass = false;        // 旁路开关
        std::array<BiquadState, 2> channels; // 双声道状态
    };

    // =========================================================================
    // 成员变量
    // =========================================================================
    std::array<BandState, 8> bands_;
    float outputGain_ = 1.0f;   // 输出增益 (线性)
    float mix_ = 1.0f;          // 干湿比 [0, 1]

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQ)
};

} // namespace LianCore