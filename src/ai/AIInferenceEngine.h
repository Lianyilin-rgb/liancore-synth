// =============================================================================
// LianCore - AIInferenceEngine AI推理引擎 (Beta阶段: ONNX Runtime集成)
// 支持文本→参数、波表生成、频谱分析
// ONNX Runtime不可用时自动回退到规则引擎
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include "EmotionToParameterMapper.h"
#include <vector>
#include <string>
#include <unordered_map>

// ONNX Runtime C++ API (条件编译)
#ifdef LIANCORE_HAS_ONNX
#include <onnxruntime_cxx_api.h>
#endif

namespace LianCore {

// 参数映射
struct ParameterMapping;

// =============================================================================
// AIInferenceEngine
// =============================================================================
class AIInferenceEngine {
public:
    static constexpr int kMaxCacheSize = 128;

    enum class GenerationMode {
        TextOnly,       // 纯文本描述
        TextWithAudio,  // 文本+音频参考
        TextWithStyle,  // 文本+风格标签
    };

    struct GenerationResult {
        std::vector<ParameterMapping> parameters;
        std::vector<float> explanationEmbeddings;
        std::vector<float> wavetableData;
        juce::String presetName;
        float confidence = 0.0f;
    };

    AIInferenceEngine();
    ~AIInferenceEngine();

    // =========================================================================
    // 模型管理 (Beta阶段: ONNX Runtime + 规则引擎回退)
    // =========================================================================
    bool loadModel(const juce::File& onnxFile);
    void unloadModel();
    bool isModelLoaded() const { return modelLoaded_; }

    // =========================================================================
    // 核心推理
    // =========================================================================
    GenerationResult generateParameters(
        const juce::String& textPrompt,
        const juce::AudioSampleBuffer* audioReference = nullptr,
        const std::vector<juce::String>& styleTags = {}
    );

    // =========================================================================
    // 情感增强推理 (Beta Week 6)
    // 文本推理结果 + 情感偏置融合 (权重 0.7:0.3)
    // =========================================================================
    GenerationResult generateParametersWithEmotion(
        const juce::String& textPrompt,
        float warmth, float energy, float tension,
        const std::vector<juce::String>& styleTags = {}
    );

    // 获取情感映射器 (供外部使用)
    EmotionToParameterMapper& getEmotionMapper() { return emotionMapper_; }

    // =========================================================================
    // 波表生成
    // =========================================================================
    juce::AudioSampleBuffer generateWavetable(
        const juce::String& description,
        int numFrames = 256,
        int frameSize = 2048
    );

    // =========================================================================
    // 频谱分析
    // =========================================================================
    std::vector<float> analyzeReferenceSpectrum(const juce::AudioSampleBuffer& audio);

    // =========================================================================
    // 音频嵌入
    // =========================================================================
    std::vector<float> extractAudioEmbedding(const juce::AudioSampleBuffer& audio);

    // =========================================================================
    // 参数解释生成
    // =========================================================================
    juce::String generateParameterExplanation(
        const juce::String& parameterName,
        float currentValue,
        const juce::String& contextPrompt
    );

    // =========================================================================
    // 性能
    // =========================================================================
    double getLastInferenceTimeMs() const { return lastInferenceTimeMs_; }
    size_t getModelMemoryUsage() const { return modelMemoryUsage_; }

    // =========================================================================
    // 模型信息
    // =========================================================================
    juce::String getModelInfo() const;

private:
    // 关键词→参数映射表 (规则引擎回退)
    struct KeywordRule {
        juce::String keyword;
        juce::String parameterId;
        float targetValue;
    };
    std::vector<KeywordRule> keywordRules_;

    // 推理缓存
    std::unordered_map<std::string, GenerationResult> resultCache_;

    // ONNX Runtime会话 (Beta阶段)
#ifdef LIANCORE_HAS_ONNX
    std::unique_ptr<Ort::Env> ortEnv_;
    std::unique_ptr<Ort::Session> ortSession_;
    std::unique_ptr<Ort::SessionOptions> ortSessionOptions_;
    std::unique_ptr<Ort::MemoryInfo> ortMemoryInfo_;
    std::vector<const char*> ortInputNames_;
    std::vector<const char*> ortOutputNames_;
    std::vector<int64_t> ortInputShape_;
#endif

    bool modelLoaded_ = false;
    size_t modelMemoryUsage_ = 0;
    double lastInferenceTimeMs_ = 0.0;

    // 情感映射器 (Beta Week 6)
    EmotionToParameterMapper emotionMapper_;

    // 规则引擎
    void buildKeywordRules();
    GenerationResult applyKeywordRules(const juce::String& text) const;

    // ONNX推理
    GenerationResult runOnnxInference(const juce::String& textPrompt,
                                       const juce::AudioSampleBuffer* audioReference,
                                       const std::vector<juce::String>& styleTags);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AIInferenceEngine)
};

} // namespace LianCore