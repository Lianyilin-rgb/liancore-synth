// =============================================================================
// LianCore - ChaoticEnvelope 混沌包络发生器 (P1-3)
// 产生非周期、不可预测的包络形状，用于实验性声音设计
// 实现 ModulationSource 接口，可注册到调制矩阵
// =============================================================================
#pragma once

#include "../core/AudioNode.h"
#include "ModulationMatrix.h"

namespace LianCore {

// =============================================================================
// 混沌包络模式
// =============================================================================
enum class ChaosEnvMode {
    ChaoticDecay,      // 混沌衰减: 每次触发产生随机衰减曲线
    BurstGenerator,    // 脉冲发生器: 随机间隔产生脉冲
    RandomWalk,        // 随机游走: 连续随机变化
    StrangeAttractor,  // 奇异吸引子: 混沌轨道映射到包络
};

// =============================================================================
// ChaoticEnvelope - 混沌包络源
// =============================================================================
class ChaoticEnvelope : public AudioNode, public ModulationSource {
public:
    ChaoticEnvelope(const juce::String& name = "混沌包络");

    // AudioNode 接口
    void prepareToPlay(double sampleRate, int maxSamplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void releaseResources() override;

    // ModulationSource 接口
    float getValue() const override { return currentValue_; }
    juce::String getName() const override { return name_; }
    juce::String getUnit() const override { return {}; }
    bool isBipolar() const override { return mode_ == ChaosEnvMode::RandomWalk || mode_ == ChaosEnvMode::StrangeAttractor; }

    // 触发
    void trigger();
    void release();

    // 参数设置
    void setMode(ChaosEnvMode mode);
    void setChaosAmount(float amount);    // 0.0-1.0 混沌程度
    void setSpeed(float speed);            // 0.0-1.0 包络速度
    void setHold(float hold);              // 0.0-1.0 保持时间

    // 参数接口
    int getNumParameters() const override { return 4; }
    float getParameter(int index) const override;
    void setParameter(int index, float value) override;
    juce::String getParameterName(int index) const override;

    // 序列化
    juce::var toJson() const override;
    void fromJson(const juce::var& json) override;

private:
    ChaosEnvMode mode_ = ChaosEnvMode::ChaoticDecay;
    float chaosAmount_ = 0.5f;
    float speed_ = 0.5f;
    float hold_ = 0.0f;

    float currentValue_ = 0.0f;
    float targetValue_ = 0.0f;
    float phase_ = 0.0f;
    float holdCounter_ = 0.0f;
    int holdSamples_ = 0;
    bool triggered_ = false;
    bool released_ = false;

    float nextChaoticValue();
    void resetPhase();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChaoticEnvelope)
};

} // namespace LianCore