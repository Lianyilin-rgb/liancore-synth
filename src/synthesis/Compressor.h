// =============================================================================
// LianCore - Compressor 前馈式RMS压缩器
// 支持软拐点、RMS包络检测、立体声联动、增益补偿
// =============================================================================
#pragma once

#include "../core/AudioNode.h"
#include <JuceHeader.h>
#include <vector>
#include <array>

namespace LianCore {

class Compressor : public AudioNode {
public:
    Compressor(const juce::String& name = "压缩器");

    void prepareToPlay(double sampleRate, int maxSamplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void releaseResources() override;

    // =========================================================================
    // 参数设置
    // =========================================================================
    void setThreshold(float value);     // 0-1 映射到 -60 至 0 dB
    void setRatio(float value);         // 0-1 映射到 1:1 至 20:1
    void setAttack(float value);        // 0-1 映射到 0.1-100ms
    void setRelease(float value);       // 0-1 映射到 10-1000ms
    void setKnee(float value);          // 0-1 映射到 0-30dB
    void setMakeupGain(float value);    // 0-1 映射到 0-30dB
    void setMix(float value);           // 0-1 干湿比

    // =========================================================================
    // 参数接口
    // =========================================================================
    int getNumParameters() const override { return 7; }
    float getParameter(int index) const override;
    void setParameter(int index, float value) override;
    juce::String getParameterName(int index) const override;

    // =========================================================================
    // 序列化
    // =========================================================================
    juce::var toJson() const override;
    void fromJson(const juce::var& json) override;

private:
    // =========================================================================
    // 内部辅助方法
    // =========================================================================
    void updateInternalParams();

    // 计算增益衰减 (dB)
    float calculateGainReduction(float levelDb) const;

    // =========================================================================
    // 参数值 (归一化 0-1)
    // =========================================================================
    float threshold_ = 0.5f;
    float ratio_ = 0.3f;
    float attack_ = 0.1f;
    float release_ = 0.3f;
    float knee_ = 0.2f;
    float makeupGain_ = 0.0f;
    float mix_ = 1.0f;

    // =========================================================================
    // 内部导出参数
    // =========================================================================
    float thresholdDb_ = -24.0f;       // dB
    float ratioValue_ = 4.0f;          // 实际压缩比
    float attackTime_ = 5.0f;          // ms
    float releaseTime_ = 100.0f;       // ms
    float kneeWidth_ = 6.0f;           // dB
    float makeupGainDb_ = 0.0f;        // dB

    // =========================================================================
    // RMS检测器状态
    // =========================================================================
    static constexpr int kRmsWindowMs = 10; // 10ms RMS窗口
    int rmsWindowSize_ = 441;               // 采样数 (将在prepareToPlay中计算)

    struct RmsDetector {
        std::vector<float> rmsBuffer;
        int writePos = 0;
        float rmsSum = 0.0f;
        float currentRms = 0.0f;
    };
    std::array<RmsDetector, 2> detectors_; // 立体声双声道RMS检测

    // =========================================================================
    // 增益平滑状态
    // =========================================================================
    float attackCoeff_ = 0.0f;
    float releaseCoeff_ = 0.0f;
    float smoothedGainReduction_ = 0.0f; // 当前平滑增益衰减 (dB)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Compressor)
};

} // namespace LianCore