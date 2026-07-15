// =============================================================================
// LianCore - Flanger 镶边效果器
// 超短调制延迟+反馈，产生喷气机式扫频效果
// =============================================================================
#pragma once

#include "../core/AudioNode.h"
#include <JuceHeader.h>
#include <vector>

namespace LianCore {

class Flanger : public AudioNode {
public:
    Flanger(const juce::String& name = "镶边");

    void prepareToPlay(double sampleRate, int maxSamplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void releaseResources() override;

    void setRate(float value);        // 0.0-1.0  LFO频率 (0.05Hz - 5Hz)
    void setDepth(float value);       // 0.0-1.0  调制深度 (0.1ms - 5ms)
    void setFeedback(float value);    // 0.0-0.95 反馈量
    void setMix(float value);         // 0.0-1.0  干湿比

    int getNumParameters() const override { return 4; }
    float getParameter(int index) const override;
    void setParameter(int index, float value) override;
    juce::String getParameterName(int index) const override;

    juce::var toJson() const override;
    void fromJson(const juce::var& json) override;

private:
    static constexpr int kMaxDelaySamples = 2400;  // ~50ms @ 48kHz

    float rate_ = 0.3f;
    float depth_ = 0.5f;
    float feedback_ = 0.5f;
    float mix_ = 0.5f;

    struct Channel {
        std::vector<float> delayLine;
        int writePos = 0;
        float lfoPhase = 0.0f;
    };
    Channel channels_[2]; // 双声道

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Flanger)
};

} // namespace LianCore