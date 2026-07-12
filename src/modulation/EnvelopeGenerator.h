// =============================================================================
// LianCore - EnvelopeGenerator ADSR包络发生器
// =============================================================================
#pragma once

#include "../core/AudioNode.h"

namespace LianCore {

enum class EnvelopeStage {
    Idle,
    Attack,
    Decay,
    Sustain,
    Release,
};

class EnvelopeGenerator : public AudioNode {
public:
    EnvelopeGenerator(const juce::String& name = "包络");

    void prepareToPlay(double sampleRate, int maxSamplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    void noteOn();
    void noteOff();

    void setAttack(float ms);
    void setDecay(float ms);
    void setSustain(float level);
    void setRelease(float ms);
    void setVelocitySensitivity(float amount);

    float getCurrentValue() const { return currentValue_; }
    EnvelopeStage getCurrentStage() const { return stage_; }

    int getNumParameters() const override { return 5; }
    float getParameter(int index) const override;
    void setParameter(int index, float value) override;
    juce::String getParameterName(int index) const override;

private:
    float attackMs_ = 10.0f;
    float decayMs_ = 100.0f;
    float sustainLevel_ = 0.7f;
    float releaseMs_ = 200.0f;
    float velocitySensitivity_ = 0.0f;

    EnvelopeStage stage_ = EnvelopeStage::Idle;
    float currentValue_ = 0.0f;
    float velocity_ = 1.0f;
    double sampleRate_ = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeGenerator)
};

} // namespace LianCore