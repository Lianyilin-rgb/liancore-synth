// =============================================================================
// LianCore - NoiseGenerator 噪声发生器
// 支持白噪声、粉红噪声、棕色噪声、采样保持
// =============================================================================
#pragma once

#include "../core/AudioNode.h"

namespace LianCore {

enum class NoiseType {
    White,      // 白噪声
    Pink,       // 粉红噪声 (-3dB/oct)
    Brown,      // 棕色噪声 (-6dB/oct)
    SampleHold, // 采样保持
};

class NoiseGenerator : public AudioNode {
public:
    NoiseGenerator(const juce::String& name = "噪声发生器");

    void prepareToPlay(double sampleRate, int maxSamplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void releaseResources() override;

    void setNoiseType(NoiseType type);
    void setVolume(float volume);
    void setSampleHoldRate(float hz); // 采样保持频率

    int getNumParameters() const override { return 3; }
    float getParameter(int index) const override;
    void setParameter(int index, float value) override;
    juce::String getParameterName(int index) const override;

private:
    float generateWhiteNoise();
    float generatePinkNoise();
    float generateBrownNoise();
    float generateSampleHold();

    NoiseType noiseType_ = NoiseType::White;
    float volume_ = 0.5f;
    float sampleHoldRate_ = 100.0f;

    // 粉红噪声状态
    float pinkB0_ = 0.0f, pinkB1_ = 0.0f, pinkB2_ = 0.0f;
    float pinkB3_ = 0.0f, pinkB4_ = 0.0f, pinkB5_ = 0.0f;

    // 棕色噪声状态
    float brownLast_ = 0.0f;

    // 采样保持状态
    float holdValue_ = 0.0f;
    int holdCounter_ = 0;
    int holdInterval_ = 0;

    double sampleRate_ = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoiseGenerator)
};

} // namespace LianCore