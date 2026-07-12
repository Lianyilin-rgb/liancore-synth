// =============================================================================
// LianCore - LFOGenerator 低频振荡器
// =============================================================================
#pragma once

#include "../core/AudioNode.h"

namespace LianCore {

enum class LFOWaveform {
    Sine,
    Triangle,
    Saw,
    Square,
    Random,
    SampleAndHold,
};

enum class LFOSyncMode {
    Free,       // 自由运行
    Tempo,      // BPM同步
    KeyTrigger, // 按键触发
};

class LFOGenerator : public AudioNode {
public:
    LFOGenerator(const juce::String& name = "LFO");

    void prepareToPlay(double sampleRate, int maxSamplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    void setFrequency(float hz);
    void setWaveform(LFOWaveform waveform);
    void setSyncMode(LFOSyncMode mode);
    void setPhase(float phase);
    void setSmooth(float amount);
    void setDepth(float depth);

    float getCurrentValue() const { return currentValue_; }

    int getNumParameters() const override { return 6; }
    float getParameter(int index) const override;
    void setParameter(int index, float value) override;
    juce::String getParameterName(int index) const override;

private:
    float generateValue(float phase);

    float frequency_ = 1.0f;
    LFOWaveform waveform_ = LFOWaveform::Sine;
    LFOSyncMode syncMode_ = LFOSyncMode::Free;
    float phaseOffset_ = 0.0f;
    float smooth_ = 0.0f;
    float depth_ = 1.0f;

    float phase_ = 0.0f;
    float currentValue_ = 0.0f;
    double sampleRate_ = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LFOGenerator)
};

} // namespace LianCore