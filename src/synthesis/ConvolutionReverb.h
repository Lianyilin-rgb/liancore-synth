// =============================================================================
// LianCore - ConvolutionReverb 卷积混响效果器
// 基于指数衰减FIR滤波器的简化卷积混响，模拟真实空间
// =============================================================================
#pragma once

#include "../core/AudioNode.h"
#include <JuceHeader.h>
#include <vector>
#include <array>

namespace LianCore {

class ConvolutionReverb : public AudioNode {
public:
    ConvolutionReverb(const juce::String& name = "卷积混响");

    void prepareToPlay(double sampleRate, int maxSamplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void releaseResources() override;

    void setSize(float value);         // 0.0-1.0  房间大小 (0.1s - 5.0s)
    void setDecay(float value);        // 0.0-1.0  衰减系数
    void setDamping(float value);      // 0.0-1.0  高频衰减
    void setMix(float value);          // 0.0-1.0  干湿比

    int getNumParameters() const override { return 4; }
    float getParameter(int index) const override;
    void setParameter(int index, float value) override;
    juce::String getParameterName(int index) const override;

    juce::var toJson() const override;
    void fromJson(const juce::var& json) override;

private:
    void rebuildIR();

    static constexpr int kMaxIRLength = 240000; // 5秒 @ 48kHz

    float size_ = 0.5f;
    float decay_ = 0.5f;
    float damping_ = 0.3f;
    float mix_ = 0.3f;

    std::vector<float> impulseResponse_;
    std::vector<float> historyBuffer_[2]; // 环形历史缓冲区
    int writePos_[2] = { 0, 0 };
    int irLength_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConvolutionReverb)
};

} // namespace LianCore