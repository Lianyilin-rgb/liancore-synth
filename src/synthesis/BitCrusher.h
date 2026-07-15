// =============================================================================
// LianCore - BitCrusher 比特粉碎效果器
// 降低采样率和位深度，产生Lo-Fi数字失真效果
// =============================================================================
#pragma once

#include "../core/AudioNode.h"
#include <JuceHeader.h>

namespace LianCore {

class BitCrusher : public AudioNode {
public:
    BitCrusher(const juce::String& name = "比特粉碎");

    void prepareToPlay(double sampleRate, int maxSamplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void releaseResources() override;

    void setBitDepth(float value);     // 0.0-1.0 → 1-16 bits
    void setSampleRateReduction(float value); // 0.0-1.0 → 无降采样→强降采样
    void setMix(float value);          // 0.0-1.0  干湿比

    int getNumParameters() const override { return 3; }
    float getParameter(int index) const override;
    void setParameter(int index, float value) override;
    juce::String getParameterName(int index) const override;

    juce::var toJson() const override;
    void fromJson(const juce::var& json) override;

private:
    float bitDepth_ = 0.5f;         // 归一化: 0.0→1bit, 1.0→16bit
    float sampleRateReduction_ = 0.0f; // 归一化: 0.0→无降采样, 1.0→强降采样
    float mix_ = 0.5f;

    // 降采样状态
    float holdSample_[2] = { 0.0f, 0.0f };
    int sampleCounter_[2] = { 0, 0 };
    int holdLength_ = 1; // 保持采样数

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BitCrusher)
};

} // namespace LianCore