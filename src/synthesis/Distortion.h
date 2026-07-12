// =============================================================================
// LianCore - Distortion 多模式失真器
// 支持6种失真类型：SoftClip/HardClip/Tube/Fuzz/Bitcrush/Foldback
// =============================================================================
#pragma once

#include "../core/AudioNode.h"
#include <JuceHeader.h>

namespace LianCore {

// =============================================================================
// 失真类型枚举
// =============================================================================
enum class DistortionType {
    SoftClip,   // 软限幅 (tanh)
    HardClip,   // 硬限幅 (±1.0)
    Tube,       // 电子管 (非对称tanh, 偶次谐波)
    Fuzz,       // 法兹 (硬限幅+高增益)
    Bitcrush,   // 比特粉碎 (量化+降采样率)
    Foldback,   // 折返失真 (foldback distortion)
};

class Distortion : public AudioNode {
public:
    Distortion(const juce::String& name = "失真器");

    // =========================================================================
    // 生命周期
    // =========================================================================
    void prepareToPlay(double sampleRate, int maxSamplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void releaseResources() override;

    // =========================================================================
    // 参数设置
    // =========================================================================
    void setDrive(float value);         // 0.0-1.0  驱动量
    void setType(DistortionType type);  // 失真类型
    void setOutput(float value);        // 0.0-1.0  输出音量
    void setMix(float value);           // 0.0-1.0  干湿比
    void setBias(float value);          // 0.0-1.0  直流偏置
    void setTone(float value);          // 0.0-1.0  音色 (后置低通)

    // =========================================================================
    // 参数接口
    // =========================================================================
    int getNumParameters() const override { return 6; }
    float getParameter(int index) const override;
    void setParameter(int index, float value) override;
    juce::String getParameterName(int index) const override;

    // =========================================================================
    // 序列化
    // =========================================================================
    juce::var toJson() const override;
    void fromJson(const juce::var& json) override;

private:
    // 各失真类型处理函数 (单采样)
    float processSoftClip(float x) const;
    float processHardClip(float x) const;
    float processTube(float x) const;
    float processFuzz(float x) const;
    float processBitcrush(float x);
    float processFoldback(float x) const;

    // 参数
    DistortionType type_ = DistortionType::SoftClip;
    float drive_ = 0.0f;
    float output_ = 0.7f;
    float mix_ = 1.0f;
    float bias_ = 0.0f;
    float tone_ = 0.0f;

    // 比特粉碎状态
    float bitcrushHold_ = 0.0f;
    int bitcrushCounter_ = 0;
    int bitcrushStep_ = 1;

    // 音色滤波器状态 (一阶低通, 双声道)
    float toneLP_[2] = { 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Distortion)
};

} // namespace LianCore