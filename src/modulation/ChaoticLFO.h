// =============================================================================
// LianCore - ChaoticLFO 混沌低频振荡器 (P1-3)
// 支持多种混沌映射，产生非周期但确定的调制信号
// 实现 ModulationSource 接口，可注册到调制矩阵
// =============================================================================
#pragma once

#include "../core/AudioNode.h"
#include "ModulationMatrix.h"
#include <array>

namespace LianCore {

// =============================================================================
// 混沌映射类型
// =============================================================================
enum class ChaosMap {
    Logistic,       // Logistic Map: x_{n+1} = r * x_n * (1 - x_n)
    Lorenz,         // Lorenz Attractor: 3D ODE 离散化
    Henon,          // Henon Map: 2D 离散动力系统
    Tent,           // Tent Map: 分段线性混沌
    Rossler,        // Rossler Attractor: 简化的 Lorenz 类系统
};

// =============================================================================
// ChaoticLFO - 混沌调制源
// =============================================================================
class ChaoticLFO : public AudioNode, public ModulationSource {
public:
    ChaoticLFO(const juce::String& name = "混沌LFO");

    // AudioNode 接口
    void prepareToPlay(double sampleRate, int maxSamplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void releaseResources() override;

    // ModulationSource 接口
    float getValue() const override { return lastOutput_; }
    juce::String getName() const override { return name_; }
    juce::String getUnit() const override { return {}; }
    bool isBipolar() const override { return true; }

    // 参数设置
    void setChaosMap(ChaosMap map);
    void setChaosAmount(float amount);    // 0.0-1.0 混沌程度 (r 参数范围)
    void setRate(float rate);              // 0.0-1.0 更新速率 (相对采样率)
    void setSmooth(float amount);          // 0.0-1.0 平滑量
    void setDepth(float depth);            // 0.0-1.0 输出深度

    // 参数接口
    int getNumParameters() const override { return 5; }
    float getParameter(int index) const override;
    void setParameter(int index, float value) override;
    juce::String getParameterName(int index) const override;

    // 序列化
    juce::var toJson() const override;
    void fromJson(const juce::var& json) override;

    // 获取当前混沌状态 (用于可视化)
    std::array<float, 3> getChaosState() const { return { x_, y_, z_ }; }

private:
    void resetChaosState();
    float stepLogistic();
    float stepLorenz();
    float stepHenon();
    float stepTent();
    float stepRossler();

    ChaosMap chaosMap_ = ChaosMap::Logistic;
    float chaosAmount_ = 0.5f;    // 归一化混沌程度
    float rate_ = 0.1f;           // 归一化速率
    float smooth_ = 0.3f;         // 平滑量
    float depth_ = 1.0f;          // 输出深度

    // 混沌状态变量
    float x_ = 0.5f;  // Logistic/Henon x
    float y_ = 0.5f;  // Lorenz/Henon y
    float z_ = 0.5f;  // Lorenz z

    float lastOutput_ = 0.0f;
    float lastRaw_ = 0.0f;

    int sampleCounter_ = 0;
    int updateInterval_ = 1; // 每N个采样更新一次混沌状态

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChaoticLFO)
};

} // namespace LianCore