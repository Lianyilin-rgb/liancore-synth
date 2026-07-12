// =============================================================================
// LianCore - Delay 立体声延迟器
// 支持Ping-Pong模式、节拍同步、反馈路径高低切滤波
// =============================================================================
#pragma once

#include "../core/AudioNode.h"
#include <JuceHeader.h>
#include <vector>

namespace LianCore {

// =============================================================================
// 节拍同步音符时值
// =============================================================================
enum class NoteDivision {
    Whole,          // 1/1
    Half,           // 1/2
    Quarter,        // 1/4
    Eighth,         // 1/8
    Sixteenth,      // 1/16
    ThirtySecond,   // 1/32
    DottedHalf,     // 附点1/2
    DottedQuarter,  // 附点1/4
    DottedEighth,   // 附点1/8
    TripletHalf,    // 三连音1/2
    TripletQuarter, // 三连音1/4
    TripletEighth,  // 三连音1/8
};

class Delay : public AudioNode {
public:
    Delay(const juce::String& name = "延迟器");

    // =========================================================================
    // 生命周期
    // =========================================================================
    void prepareToPlay(double sampleRate, int maxSamplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void releaseResources() override;

    // =========================================================================
    // 参数设置
    // =========================================================================
    void setTime(float value);           // 0.0-1.0  延迟时间 (1ms-2000ms 或 音符时值)
    void setFeedback(float value);       // 0.0-0.95 反馈量
    void setMix(float value);            // 0.0-1.0  干湿比
    void setPingPong(bool enabled);      // Ping-Pong模式
    void setLowPassCutoff(float value);  // 0.0-1.0  反馈路径低通截止
    void setHighPassCutoff(float value); // 0.0-1.0  反馈路径高通截止
    void setTempoSync(bool enabled);     // 节拍同步
    void setNoteDivision(NoteDivision div); // 音符时值
    void setBPM(double bpm);             // 设置BPM (用于节拍同步)

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
    // 计算延迟时间(采样数)
    float computeDelayTimeSamples() const;
    // 计算一阶滤波器系数
    float computeLPCoefficient(float cutoffNorm) const;

    // 参数
    float time_ = 0.25f;                // 延迟时间参数
    float feedback_ = 0.3f;             // 反馈量
    float mix_ = 0.4f;                  // 干湿比
    bool pingPong_ = false;             // Ping-Pong
    float lowPassCutoff_ = 1.0f;        // 低通截止 (归一化)
    float highPassCutoff_ = 0.0f;       // 高通截止 (归一化)
    bool tempoSync_ = false;            // 节拍同步
    NoteDivision noteDivision_ = NoteDivision::Quarter;
    double bpm_ = 120.0;                // 节拍

    // 延迟线环形缓冲区 (双声道, 最大48000采样)
    static constexpr int kMaxDelaySamples = 48000;
    std::vector<float> delayLine_[2];
    int writePos_[2] = { 0, 0 };
    int delaySamples_ = 0;              // 当前延迟采样数

    // 平滑过渡
    float smoothDelaySamples_ = 0.0f;

    // 反馈滤波器状态 (双声道, 一阶LP + 一阶HP)
    float lpState_[2] = { 0.0f, 0.0f };
    float hpState_[2] = { 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Delay)
};

} // namespace LianCore