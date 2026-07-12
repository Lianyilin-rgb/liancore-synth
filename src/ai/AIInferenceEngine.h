// =============================================================================
// LianCore - AIInferenceEngine AI推理引擎 (Alpha阶段: 接口定义+占位)
// Beta阶段集成ONNX Runtime，实现文本→参数、波表生成、频谱分析
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <string>
#include <unordered_map>

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
    // 模型管理 (Alpha阶段: 占位)
    // =========================================================================
    bool loadModel(const juce::File& onnxFile);
    void unloadModel();
    bool isModelLoaded() const { return false; } // Alpha阶段无模型

    // =========================================================================
    // 核心推理 (Alpha阶段: 返回基于规则的音色)
    // =========================================================================
    GenerationResult generateParameters(
        const juce::String& textPrompt,
        const juce::AudioSampleBuffer* audioReference = nullptr,
        const std::vector<juce::String>& styleTags = {}
    );

    // =========================================================================
    // 波表生成 (Alpha阶段: 使用WavetableBank生成)
    // =========================================================================
    juce::AudioSampleBuffer generateWavetable(
        const juce::String& description,
        int numFrames = 256,
        int frameSize = 2048
    );

    // =========================================================================
    // 频谱分析 (Alpha阶段: 基础FFT分析)
    // =========================================================================
    std::vector<float> analyzeReferenceSpectrum(const juce::AudioSampleBuffer& audio);

    // =========================================================================
    // 音频嵌入
    // =========================================================================
    std::vector<float> extractAudioEmbedding(const juce::AudioSampleBuffer& audio);

    // =========================================================================
    // 参数解释生成 (Alpha阶段: 基于模板)
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
    size_t getModelMemoryUsage() const { return 0; }

private:
    // 关键词→参数映射表 (Alpha阶段基于规则)
    struct KeywordRule {
        juce::String keyword;
        juce::String parameterId;
        float targetValue;
    };
    std::vector<KeywordRule> keywordRules_;

    // 推理缓存
    std::unordered_map<std::string, GenerationResult> resultCache_;

    double lastInferenceTimeMs_ = 0.0;

    void buildKeywordRules();
    GenerationResult applyKeywordRules(const juce::String& text) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AIInferenceEngine)
};

} // namespace LianCore