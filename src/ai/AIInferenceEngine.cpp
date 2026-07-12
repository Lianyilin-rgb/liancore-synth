// =============================================================================
// LianCore - AIInferenceEngine 实现
// =============================================================================
#include "AIInferenceEngine.h"
#include "../params/ParameterTree.h"

namespace LianCore {

AIInferenceEngine::AIInferenceEngine() {
    buildKeywordRules();
}

AIInferenceEngine::~AIInferenceEngine() = default;

bool AIInferenceEngine::loadModel(const juce::File&) {
    // Alpha阶段: ONNX模型将在Beta阶段加载
    return false;
}

void AIInferenceEngine::unloadModel() {
    // Alpha阶段: 无模型需卸载
    resultCache_.clear();
}

// =============================================================================
// 关键词规则构建 (Alpha阶段: 模拟AI推理)
// =============================================================================
void AIInferenceEngine::buildKeywordRules() {
    // 音色形容词 → 滤波器截止频率
    keywordRules_ = {
        {"明亮", "filter_cutoff", 0.8f},
        {"温暖", "filter_cutoff", 0.3f},
        {"暗", "filter_cutoff", 0.15f},
        {"尖锐", "filter_cutoff", 0.9f},
        {"柔和", "filter_cutoff", 0.25f},
        // 情感 → 滤波器共振
        {"紧张", "filter_resonance", 0.7f},
        {"放松", "filter_resonance", 0.2f},
        // 风格 → 振荡器波形
        {"复古", "osc_waveform", 0.25f},  // 三角波
        {"现代", "osc_waveform", 0.5f},    // 锯齿波
        {"电子", "osc_waveform", 0.75f},   // 方波
        // 动态 → 包络
        {"快速", "env_attack", 0.1f},
        {"慢速", "env_attack", 0.5f},
        {"长音", "env_release", 0.8f},
        {"短促", "env_release", 0.1f},
    };
}

// =============================================================================
// 核心推理
// =============================================================================
AIInferenceEngine::GenerationResult AIInferenceEngine::generateParameters(
    const juce::String& textPrompt,
    const juce::AudioSampleBuffer* audioReference,
    const std::vector<juce::String>& styleTags) {

    auto startTime = juce::Time::getMillisecondCounterHiRes();

    // 检查缓存
    juce::String cacheKey = textPrompt;
    for (const auto& tag : styleTags) cacheKey += "_" + tag;
    if (auto it = resultCache_.find(cacheKey.toStdString()); it != resultCache_.end()) {
        lastInferenceTimeMs_ = juce::Time::getMillisecondCounterHiRes() - startTime;
        return it->second;
    }

    GenerationResult result;
    result.confidence = 0.6f; // Alpha阶段置信度较低

    // 应用关键词规则
    result = applyKeywordRules(textPrompt);

    // 应用风格标签
    juce::String combinedPrompt = textPrompt;
    for (const auto& tag : styleTags) {
        combinedPrompt += " " + tag;
    }
    auto tagResult = applyKeywordRules(combinedPrompt);
    for (auto& param : tagResult.parameters) {
        result.parameters.push_back(param);
    }

    // 生成预设名称
    result.presetName = "AI_" + textPrompt.substring(0, 30).replace(" ", "_");

    // 缓存结果
    if (resultCache_.size() < kMaxCacheSize) {
        resultCache_[cacheKey.toStdString()] = result;
    }

    lastInferenceTimeMs_ = juce::Time::getMillisecondCounterHiRes() - startTime;
    return result;
}

AIInferenceEngine::GenerationResult AIInferenceEngine::applyKeywordRules(const juce::String& text) const {
    GenerationResult result;
    juce::String lowerText = text.toLowerCase();

    for (const auto& rule : keywordRules_) {
        if (lowerText.contains(rule.keyword)) {
            ParameterMapping mapping;
            mapping.parameterId = rule.parameterId;
            mapping.value = rule.targetValue;
            mapping.explanation = "关键词 \"" + rule.keyword + "\" 触发了参数调整";
            result.parameters.push_back(mapping);
        }
    }

    return result;
}

// =============================================================================
// 波表生成
// =============================================================================
juce::AudioSampleBuffer AIInferenceEngine::generateWavetable(
    const juce::String& description, int numFrames, int frameSize) {

    juce::AudioSampleBuffer wavetable(1, numFrames * frameSize);
    wavetable.clear();

    juce::String lowerDesc = description.toLowerCase();

    // 基于描述选择基础波形
    if (lowerDesc.contains("锯齿") || lowerDesc.contains("saw")) {
        // 生成锯齿波波表
        for (int f = 0; f < numFrames; ++f) {
            float harmonics = 1.0f - static_cast<float>(f) / numFrames;
            int numHarm = std::max(1, static_cast<int>(64.0f * harmonics));
            auto* frameData = wavetable.getWritePointer(0) + f * frameSize;
            for (int i = 0; i < frameSize; ++i) {
                float phase = static_cast<float>(i) / frameSize;
                float sample = 0.0f;
                for (int h = 1; h <= numHarm; ++h) {
                    sample += std::sin(6.2831853f * phase * h) / h * ((h % 2 == 0) ? -1.0f : 1.0f);
                }
                frameData[i] = sample;
            }
        }
    } else if (lowerDesc.contains("方波") || lowerDesc.contains("square")) {
        for (int f = 0; f < numFrames; ++f) {
            float pw = 0.5f - 0.4f * static_cast<float>(f) / numFrames;
            auto* frameData = wavetable.getWritePointer(0) + f * frameSize;
            for (int i = 0; i < frameSize; ++i) {
                float phase = static_cast<float>(i) / frameSize;
                frameData[i] = phase < pw ? 1.0f : -1.0f;
            }
        }
    } else {
        // 默认: 正弦波
        for (int f = 0; f < numFrames; ++f) {
            auto* frameData = wavetable.getWritePointer(0) + f * frameSize;
            for (int i = 0; i < frameSize; ++i) {
                frameData[i] = std::sin(6.2831853f * i / frameSize);
            }
        }
    }

    return wavetable;
}

// =============================================================================
// 频谱分析
// =============================================================================
std::vector<float> AIInferenceEngine::analyzeReferenceSpectrum(const juce::AudioSampleBuffer& audio) {
    int numSamples = audio.getNumSamples();
    std::vector<float> spectrum(numSamples / 2, 0.0f);

    // 简单FFT分析 (Alpha阶段: 使用JUCE FFT)
    juce::dsp::FFT fft(static_cast<int>(std::log2(numSamples)));
    std::vector<float> fftData(numSamples * 2, 0.0f);

    const float* audioData = audio.getReadPointer(0);
    for (int i = 0; i < numSamples && i < static_cast<int>(fftData.size()); ++i) {
        fftData[i * 2] = audioData[i];
    }

    fft.performFrequencyOnlyForwardTransform(fftData.data());

    for (int i = 0; i < static_cast<int>(spectrum.size()); ++i) {
        spectrum[i] = std::abs(fftData[i]);
    }

    return spectrum;
}

std::vector<float> AIInferenceEngine::extractAudioEmbedding(const juce::AudioSampleBuffer& audio) {
    // Alpha阶段: 简单频谱特征作为嵌入
    return analyzeReferenceSpectrum(audio);
}

// =============================================================================
// 参数解释生成
// =============================================================================
juce::String AIInferenceEngine::generateParameterExplanation(
    const juce::String& parameterName,
    float currentValue,
    const juce::String& contextPrompt) {

    static const std::unordered_map<juce::String, juce::String> explanations = {
        {"filter_cutoff", "截止频率"},
        {"filter_resonance", "滤波器共振"},
        {"osc_waveform", "振荡器波形"},
        {"env_attack", "包络起音"},
        {"env_release", "包络释音"},
    };

    juce::String paramDesc = parameterName;
    auto it = explanations.find(parameterName);
    if (it != explanations.end()) {
        paramDesc = it->second;
    }

    if (currentValue > 0.8f) {
        return juce::String::formatted("AI自动提高%s以增加%s", paramDesc, contextPrompt);
    } else if (currentValue < 0.2f) {
        return juce::String::formatted("AI自动降低%s以营造%s效果", paramDesc, contextPrompt);
    }
    return juce::String::formatted("AI根据\"%s\"调整了%s", contextPrompt, paramDesc);
}

} // namespace LianCore