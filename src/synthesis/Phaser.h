// =============================================================================
// LianCore - Phaser 移相器效果器
// 级联全通滤波器+LFO调制，产生梳状扫频效果
// =============================================================================
#pragma once

#include "../core/AudioNode.h"
#include <JuceHeader.h>
#include <vector>
#include <array>

namespace LianCore {

class Phaser : public AudioNode {
public:
    Phaser(const juce::String& name = "移相");

    void prepareToPlay(double sampleRate, int maxSamplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void releaseResources() override;

    void setRate(float value);        // 0.0-1.0  LFO频率 (0.05Hz - 5Hz)
    void setDepth(float value);       // 0.0-1.0  调制深度
    void setFeedback(float value);    // 0.0-0.95 反馈量
    void setMix(float value);          // 0.0-1.0  干湿比
    void setStages(int count);        // 2-12     全通滤波器级数

    int getNumParameters() const override { return 5; }
    float getParameter(int index) const override;
    void setParameter(int index, float value) override;
    juce::String getParameterName(int index) const override;

    juce::var toJson() const override;
    void fromJson(const juce::var& json) override;

private:
    static constexpr int kMaxStages = 12;

    float rate_ = 0.3f;
    float depth_ = 0.5f;
    float feedback_ = 0.3f;
    float mix_ = 0.5f;
    int stages_ = 4;

    struct AllPassStage {
        float x1 = 0.0f, x2 = 0.0f;
        float y1 = 0.0f, y2 = 0.0f;
    };

    struct Channel {
        std::array<AllPassStage, kMaxStages> stages;
        float lfoPhase = 0.0f;
    };
    Channel channels_[2]; // 双声道

    float wetFeedback_ = 0.0f; // 反馈信号

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Phaser)
};

} // namespace LianCore