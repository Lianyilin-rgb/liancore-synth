// =============================================================================
// LianCore - FilterProcessor 多模式滤波器
// 支持LP/HP/BP/BR/Peak滤波器，带共振
// =============================================================================
#pragma once

#include "../core/AudioNode.h"
#include <JuceHeader.h>

namespace LianCore {

enum class FilterMode {
    LowPass,
    HighPass,
    BandPass,
    BandReject,
    Peak,
};

class FilterProcessor : public AudioNode {
public:
    FilterProcessor(const juce::String& name = "滤波器");

    void prepareToPlay(double sampleRate, int maxSamplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void releaseResources() override;

    void setFilterMode(FilterMode mode);
    void setCutoff(float hz);
    void setResonance(float q); // 0.0-1.0
    void setDrive(float drive); // 0.0-1.0
    void setMix(float mix);     // dry/wet

    int getNumParameters() const override { return 5; }
    float getParameter(int index) const override;
    void setParameter(int index, float value) override;
    juce::String getParameterName(int index) const override;

private:
    void updateFilterCoefficients();

    FilterMode filterMode_ = FilterMode::LowPass;
    float cutoff_ = 1000.0f;
    float resonance_ = 0.1f;
    float drive_ = 0.0f;
    float mix_ = 1.0f;

    double sampleRate_ = 44100.0;

    // 双二阶滤波器状态 (双声道)
    struct BiquadState {
        float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float x1 = 0.0f, x2 = 0.0f;
        float y1 = 0.0f, y2 = 0.0f;
    };
    std::array<BiquadState, 2> states_; // 双声道

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterProcessor)
};

} // namespace LianCore