// =============================================================================
// LianCore - PluginProcessor VST3插件处理器
// 核心音频处理入口，整合音频图引擎、合成引擎、调制矩阵、参数树
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include "../core/AudioGraphEngine.h"
#include "../modulation/ModulationMatrix.h"
#include "../params/ParameterTree.h"
#include "../params/PresetManager.h"
#include "../ai/AIInferenceEngine.h"

namespace LianCore {

class PluginProcessor : public juce::AudioProcessor {
public:
    PluginProcessor();
    ~PluginProcessor() override;

    // =========================================================================
    // AudioProcessor 接口
    // =========================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // =========================================================================
    // LianCore 特定接口
    // =========================================================================
    AudioGraphEngine& getAudioGraph() { return audioGraph_; }
    ModulationMatrix& getModulationMatrix() { return modulationMatrix_; }
    LianCoreParameterTree& getParameterTree() { return parameterTree_; }
    PresetManager& getPresetManager() { return presetManager_; }
    AIInferenceEngine& getAIEngine() { return aiEngine_; }

    // 初始化默认音频图 (Alpha阶段: 基础信号链)
    void initializeDefaultGraph();

    // 性能监控
    double getCpuUsage() const;
    size_t getMemoryUsage() const;

    // Gamma: ONNX 模型自动加载
    void initializeAI();
    bool isAIModelLoaded() const;
    juce::File getModelDirectory() const;

private:
    // 核心组件
    AudioGraphEngine audioGraph_;
    ModulationMatrix modulationMatrix_;
    LianCoreParameterTree parameterTree_;
    PresetManager presetManager_;
    AIInferenceEngine aiEngine_;

    // 合成链节点ID
    NodeId oscNodeId_;
    NodeId filterNodeId_;
    NodeId envNodeId_;
    NodeId lfoNodeId_;
    NodeId outputNodeId_;

    // 状态
    double currentCpuUsage_ = 0.0;
    int currentProgram_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};

} // namespace LianCore