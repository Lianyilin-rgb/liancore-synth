// =============================================================================
// LianCore - Chorus 合声效果器
// 多声部调制延迟，产生丰满的合声效果
// =============================================================================
#pragma once

#include "../core/AudioNode.h"
#include <JuceHeader.h>
#include <vector>
#include <array>

namespace LianCore {

class Chorus : public AudioNode {
public:
    Chorus(const juce::String& name = "合声");

    void prepareToPlay(double sampleRate, int maxSamplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void releaseResources() override;

    void setRate(float value);       // 0.0-1.0  LFO频率 (0.1Hz - 5Hz)
    void setDepth(float value);      // 0.0-1.0  调制深度 (0-15ms)
    void setMix(float value);        // 0.0-1.0  干湿比
    void setVoices(int count);       // 1-4      合声数量

    int getNumParameters() const override { return 4; }
    float getParameter(int index) const override;
    void setParameter(int index, float value) override;
    juce::String getParameterName(int index) const override;

    juce::var toJson() const override;
    void fromJson(const juce::var& json) override;

private:
    static constexpr int kMaxVoices = 4;
    static constexpr int kMaxDelaySamples = 4800;  // ~100ms @ 48kHz

    float rate_ = 0.3f;
    float depth_ = 0.5f;
    float mix_ = 0.4f;
    int voices_ = 2;

    struct Voice {
        std::vector<float> delayLine;
        int writePos = 0;
        float lfoPhase = 0.0f;
        float lfoPhaseOffset = 0.0f;
    };
    std::array<Voice, kMaxVoices> voiceStates_[2]; // 双声道

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Chorus)
};

} // namespace LianCore