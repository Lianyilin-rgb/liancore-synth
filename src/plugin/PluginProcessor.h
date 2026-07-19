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
#include "../ai/AudioTimbreAnalyzer.h"
#include "MPEProcessor.h"
#include "MPERecorder.h"
#include "OversamplingProcessor.h"
#include "../tuning/MicrotuningManager.h"
#include "../synthesis/GranularPlayer.h"
#include "../params/EffectsChainPreset.h"

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

    // MPE: 声明支持 MPE（MIDI Polyphonic Expression），让 DAW 识别为 MPE 兼容插件
    bool supportsMPE() const override { return mpeProcessor_.isEnabled(); }

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

    // =========================================================================
    // MPE (MIDI Polyphonic Expression) 支持
    // =========================================================================
    void enableMPE(bool enable);
    bool isMPEEnabled() const;
    MPEProcessor& getMPEProcessor() { return mpeProcessor_; }
    juce::MPEInstrument& getMPEInstrument() { return mpeProcessor_.getInstrument(); }

    // =========================================================================
    // MPE 录制与回放 (P5-2 → P6-1 UI)
    // =========================================================================
    MPERecorder& getMPERecorder() { return mpeRecorder_; }
    MPEPlayer& getMPEPlayer() { return mpePlayer_; }

    // =========================================================================
    // 粒子合成引擎 (P5-1 → P6-2 UI)
    // =========================================================================
    GranularPlayer& getGranularPlayer() { return granularPlayer_; }

    // =========================================================================
    // 效果链预设管理 (P5-3 → P6-3 UI)
    // =========================================================================
    EffectsChainPresetManager& getEffectsChainPresetManager() { return effectsChainPresetManager_; }
    EffectsChainPreset& getCurrentEffectsChainPreset() { return currentEffectsChainPreset_; }
    void setCurrentEffectsChainPreset(const EffectsChainPreset& preset) { currentEffectsChainPreset_ = preset; }

    // =========================================================================
    // 微音程/调音支持 (P0-3)
    // =========================================================================
    Tuning::MicrotuningManager& getTuningManager() { return tuningManager_; }
    bool loadScalaFile(const juce::File& file);
    bool loadTuningPreset(const std::string& presetName);
    bool isTuningLoaded() const;
    std::string getTuningName() const;
    void resetTuningToDefault();
    double getTuningFrequency(int midiNote) const;

    // =========================================================================
    // 过采样 (P2-4)
    // =========================================================================
    OversamplingProcessor& getOversamplingProcessor() { return oversamplingProcessor_; }
    bool isOversamplingEnabled() const;

    // =========================================================================
    // 音色匹配 (P3-任务3)
    // =========================================================================
    /** 从内存音频缓冲区分析音色 */
    AI::AudioTimbreAnalyzer::AnalysisResult analyzeTimbreFromBuffer(
        const juce::AudioBuffer<float>& buffer, double sampleRate);
    /** 获取最后一次匹配结果 */
    const AI::AudioTimbreAnalyzer::AnalysisResult& getLastTimbreResult() const { return lastTimbreResult_; }
    /** 是否正在分析中 */
    bool isAnalyzing() const { return analyzing_; }
    /** 获取音色分析器引用 */
    AI::AudioTimbreAnalyzer& getTimbreAnalyzer() { return timbreAnalyzer_; }

private:
    // 核心组件
    AudioGraphEngine audioGraph_;
    ModulationMatrix modulationMatrix_;
    LianCoreParameterTree parameterTree_;
    PresetManager presetManager_;
    AIInferenceEngine aiEngine_;

    // MPE 支持
    MPEProcessor mpeProcessor_;

    // MPE 录制与回放 (P5-2)
    MPERecorder mpeRecorder_;
    MPEPlayer mpePlayer_;

    // 粒子合成引擎 (P5-1 → P6-2 UI)
    GranularPlayer granularPlayer_;

    // 效果链预设管理 (P5-3 → P6-3 UI)
    EffectsChainPresetManager effectsChainPresetManager_;
    EffectsChainPreset currentEffectsChainPreset_;

    // 微音程/调音支持 (P0-3)
    Tuning::MicrotuningManager tuningManager_;

    // 过采样处理器 (P2-4)
    OversamplingProcessor oversamplingProcessor_;

    // 音色分析器 (P3-任务3)
    AI::AudioTimbreAnalyzer timbreAnalyzer_;
    AI::AudioTimbreAnalyzer::AnalysisResult lastTimbreResult_;
    bool analyzing_ = false;

    // 合成链节点ID
    NodeId oscNodeId_;
    NodeId filterNodeId_;
    NodeId envNodeId_;
    NodeId lfoNodeId_;
    NodeId outputNodeId_;

    // 状态
    double currentCpuUsage_ = 0.0;
    int currentProgram_ = 0;

    // 延音踏板状态 (CC64) - 用于无 MIDI 输入时静音判断
    bool sustainPedalHeld_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};

} // namespace LianCore