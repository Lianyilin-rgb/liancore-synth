// =============================================================================
// LianCore - RingMod 环形调制效果器
// 将输入信号与载波正弦波相乘，产生金属质感
// =============================================================================
#pragma once

#include "../core/AudioNode.h"
#include <JuceHeader.h>

namespace LianCore {

class RingMod : public AudioNode {
public:
    RingMod(const juce::String& name = "环形调制");

    void prepareToPlay(double sampleRate, int maxSamplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void releaseResources() override;

    void setFrequency(float value);    // 0.0-1.0 → 20Hz - 5000Hz 载波频率
    void setMix(float value);          // 0.0-1.0  干湿比

    int getNumParameters() const override { return 2; }
    float getParameter(int index) const override;
    void setParameter(int index, float value) override;
    juce::String getParameterName(int index) const override;

    juce::var toJson() const override;
    void fromJson(const juce::var& json) override;

private:
    float frequency_ = 0.1f;  // 归一化: 0.0→20Hz, 1.0→5000Hz
    float mix_ = 0.5f;

    float carrierPhase_ = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RingMod)
};

} // namespace LianCore