// =============================================================================
// LianCore - VirtualAnalogOscillator 虚拟模拟振荡器
// 支持锯齿波/方波/三角波/正弦波，带抗混叠
// =============================================================================
#pragma once

#include "../core/AudioNode.h"

namespace LianCore {

// 波形类型
enum class VAWaveform {
    Sine,
    Triangle,
    Saw,
    Square,
    Pulse,
};

class VirtualAnalogOscillator : public AudioNode {
public:
    VirtualAnalogOscillator(const juce::String& name = "虚拟模拟振荡器");

    void prepareToPlay(double sampleRate, int maxSamplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void releaseResources() override;

    // 参数设置
    void setFrequency(float hz);
    void setWaveform(VAWaveform waveform);
    void setPulseWidth(float pw); // 0.0-1.0 (仅方波有效)
    void setVolume(float volume);
    void setPhaseOffset(float offset);

    int getNumParameters() const override { return 4; }
    float getParameter(int index) const override;
    void setParameter(int index, float value) override;
    juce::String getParameterName(int index) const override;

private:
    float generateSample(float phase);

    float frequency_ = 440.0f;
    VAWaveform waveform_ = VAWaveform::Saw;
    float pulseWidth_ = 0.5f;
    float volume_ = 1.0f;
    float phaseOffset_ = 0.0f;

    float phase_ = 0.0f;
    double sampleRate_ = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VirtualAnalogOscillator)
};

} // namespace LianCore