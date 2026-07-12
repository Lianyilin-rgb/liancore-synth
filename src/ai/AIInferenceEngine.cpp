// =============================================================================
// LianCore - AIInferenceEngine 实现 (Beta阶段: ONNX Runtime + 规则引擎回退)
// =============================================================================
#include "AIInferenceEngine.h"
#include "../params/ParameterTree.h"
#include "../utils/AudioUtils.h"
#include <unordered_set>

namespace LianCore {

AIInferenceEngine::AIInferenceEngine() {
    buildKeywordRules();

#ifdef LIANCORE_HAS_ONNX
    try {
        // 初始化ONNX Runtime环境
        ortEnv_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "LianCore");
        ortSessionOptions_ = std::make_unique<Ort::SessionOptions>();
        ortSessionOptions_->SetIntraOpNumThreads(2);  // 限制线程数，降低CPU占用
        ortSessionOptions_->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
        ortMemoryInfo_ = std::make_unique<Ort::MemoryInfo>(
            Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault));
    } catch (const std::exception& e) {
        DBG("ONNX Runtime init failed: " << e.what());
    }
#endif
}

AIInferenceEngine::~AIInferenceEngine() {
    unloadModel();
}

// =============================================================================
// 模型管理
// =============================================================================
bool AIInferenceEngine::loadModel(const juce::File& onnxFile) {
    unloadModel();

#ifdef LIANCORE_HAS_ONNX
    if (!onnxFile.existsAsFile()) {
        DBG("ONNX model not found: " << onnxFile.getFullPathName());
        return false;
    }

    try {
        // 加载ONNX模型
        std::string modelPath = onnxFile.getFullPathName().toStdString();
        ortSession_ = std::make_unique<Ort::Session>(
            *ortEnv_, modelPath.c_str(), *ortSessionOptions_);

        // 获取输入输出名称
        Ort::AllocatorWithDefaultOptions allocator;
        size_t numInputs = ortSession_->GetInputCount();
        for (size_t i = 0; i < numInputs; ++i) {
            auto name = ortSession_->GetInputNameAllocated(i, allocator);
            ortInputNames_.push_back(name.release());
        }

        size_t numOutputs = ortSession_->GetOutputCount();
        for (size_t i = 0; i < numOutputs; ++i) {
            auto name = ortSession_->GetOutputNameAllocated(i, numOutputs);
            ortOutputNames_.push_back(name.release());
        }

        // 获取输入形状
        if (numInputs > 0) {
            Ort::TypeInfo typeInfo = ortSession_->GetInputTypeInfo(0);
            auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();
            ortInputShape_ = tensorInfo.GetShape();
        }

        modelLoaded_ = true;
        modelMemoryUsage_ = onnxFile.getSize();
        DBG("ONNX model loaded: " << onnxFile.getFullPathName() << " (" << modelMemoryUsage_ / 1024 / 1024 << "MB)");
        return true;
    } catch (const std::exception& e) {
        DBG("ONNX model load failed: " << e.what());
        modelLoaded_ = false;
        return false;
    }
#else
    juce::ignoreUnused(onnxFile);
    DBG("ONNX Runtime not compiled. Using rule engine fallback.");
    return false;
#endif
}

void AIInferenceEngine::unloadModel() {
#ifdef LIANCORE_HAS_ONNX
    ortSession_.reset();
    for (auto* name : ortInputNames_) {
        if (name) free(const_cast<char*>(name));
    }
    ortInputNames_.clear();
    for (auto* name : ortOutputNames_) {
        if (name) free(const_cast<char*>(name));
    }
    ortOutputNames_.clear();
    ortInputShape_.clear();
#endif
    modelLoaded_ = false;
    modelMemoryUsage_ = 0;
    resultCache_.clear();
}

juce::String AIInferenceEngine::getModelInfo() const {
    if (!modelLoaded_) {
        return "未加载模型 (使用规则引擎)";
    }
#ifdef LIANCORE_HAS_ONNX
    return juce::String::formatted("ONNX模型已加载 (%.1fMB)", modelMemoryUsage_ / 1024.0 / 1024.0);
#else
    return "规则引擎模式";
#endif
}

// =============================================================================
// 关键词规则构建
// =============================================================================
void AIInferenceEngine::buildKeywordRules() {
    keywordRules_ = {
        // 音色形容词 → 滤波器截止频率
        {"明亮", "filter_cutoff", 0.8f},
        {"温暖", "filter_cutoff", 0.3f},
        {"暗", "filter_cutoff", 0.15f},
        {"尖锐", "filter_cutoff", 0.9f},
        {"柔和", "filter_cutoff", 0.25f},
        {"厚重", "filter_cutoff", 0.2f},
        {"轻盈", "filter_cutoff", 0.7f},
        // 情感 → 滤波器共振
        {"紧张", "filter_resonance", 0.7f},
        {"放松", "filter_resonance", 0.2f},
        {"梦幻", "filter_resonance", 0.6f},
        {"空灵", "filter_resonance", 0.5f},
        // 风格 → 振荡器波形
        {"复古", "osc_waveform", 0.25f},
        {"现代", "osc_waveform", 0.5f},
        {"电子", "osc_waveform", 0.75f},
        {"经典", "osc_waveform", 0.3f},
        {"管弦", "osc_waveform", 0.15f},
        {"贝斯", "osc_waveform", 0.4f},
        {"主音", "osc_waveform", 0.6f},
        {"铺底", "osc_waveform", 0.35f},
        // 动态 → 包络
        {"快速", "env_attack", 0.1f},
        {"慢速", "env_attack", 0.5f},
        {"长音", "env_release", 0.8f},
        {"短促", "env_release", 0.1f},
        {"打击", "env_attack", 0.05f},
        {"弹拨", "env_attack", 0.15f},
        // 混响相关
        {"大厅", "reverb_size", 0.8f},
        {"房间", "reverb_size", 0.3f},
        {"环境", "reverb_size", 0.5f},
        // 音高相关
        {"低音", "osc_pitch", 0.0f},
        {"高音", "osc_pitch", 1.0f},
        // 噪声层
        {"噪声", "noise_level", 0.3f},
        {"纯净", "noise_level", 0.0f},
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

    // Beta阶段: 优先使用ONNX Runtime
    if (modelLoaded_) {
        auto result = runOnnxInference(textPrompt, audioReference, styleTags);
        if (result.confidence > 0.5f) {
            lastInferenceTimeMs_ = juce::Time::getMillisecondCounterHiRes() - startTime;
            if (resultCache_.size() < kMaxCacheSize) {
                resultCache_[cacheKey.toStdString()] = result;
            }
            return result;
        }
    }

    // 规则引擎回退
    GenerationResult result = applyKeywordRules(textPrompt);
    result.confidence = 0.6f;

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

    // 音频参考处理
    if (audioReference && audioReference->getNumSamples() > 0) {
        auto spectrum = analyzeReferenceSpectrum(*audioReference);
        result.explanationEmbeddings = spectrum;
        result.confidence = AudioUtils::clamp(result.confidence + 0.1f, 0.0f, 1.0f);
    }

    lastInferenceTimeMs_ = juce::Time::getMillisecondCounterHiRes() - startTime;
    return result;
}

// =============================================================================
// 情感增强推理 (Beta Week 6)
// 文本推理结果 (70%) + 情感偏置 (30%) 融合
// =============================================================================
AIInferenceEngine::GenerationResult AIInferenceEngine::generateParametersWithEmotion(
    const juce::String& textPrompt,
    float warmth, float energy, float tension,
    const std::vector<juce::String>& styleTags) {

    auto startTime = juce::Time::getMillisecondCounterHiRes();

    // 1. 先执行文本推理
    GenerationResult textResult = generateParameters(textPrompt, nullptr, styleTags);

    // 2. 获取情感偏置参数 (直接映射规则, 快速路径)
    auto emotionParams = emotionMapper_.mapEmotionDirect(warmth, energy, tension);

    // 3. 融合: 文本参数权重 0.7, 情感偏置权重 0.3
    // 构建文本参数查找表
    std::unordered_map<juce::String, float> textParamMap;
    for (const auto& p : textResult.parameters) {
        textParamMap[p.parameterId] = p.value;
    }

    // 收集所有参数ID
    std::unordered_set<juce::String> allParamIds;
    for (const auto& p : textResult.parameters)  allParamIds.insert(p.parameterId);
    for (const auto& p : emotionParams)          allParamIds.insert(p.parameterId);

    std::vector<ParameterMapping> fusedParams;
    for (const auto& id : allParamIds) {
        float textVal = 0.5f;
        float emotionVal = 0.5f;

        auto textIt = textParamMap.find(id);
        if (textIt != textParamMap.end()) textVal = textIt->second;

        for (const auto& ep : emotionParams) {
            if (ep.parameterId == id) {
                emotionVal = ep.value;
                break;
            }
        }

        // 融合权重: 文本 0.7 + 情感 0.3
        float fusedValue = textVal * 0.7f + emotionVal * 0.3f;

        ParameterMapping mapping;
        mapping.parameterId = id;
        mapping.value = juce::jlimit(0.0f, 1.0f, fusedValue);
        mapping.explanation = juce::String::formatted(
            "AI融合: 文本(%.2f)*0.7 + 情感(W=%.2f,E=%.2f,T=%.2f)*0.3",
            textVal, warmth, energy, tension);
        fusedParams.push_back(mapping);
    }

    // 4. 构建结果
    GenerationResult result;
    result.parameters = fusedParams;
    result.presetName = textResult.presetName + juce::String::formatted(
        "_情感[W%.0fE%.0fT%.0f]", warmth * 100, energy * 100, tension * 100);
    result.confidence = textResult.confidence * 0.85f; // 融合后置信度略降
    result.explanationEmbeddings = textResult.explanationEmbeddings;
    result.wavetableData = textResult.wavetableData;

    lastInferenceTimeMs_ = juce::Time::getMillisecondCounterHiRes() - startTime;
    return result;
}

// =============================================================================
// ONNX Runtime推理
// =============================================================================
AIInferenceEngine::GenerationResult AIInferenceEngine::runOnnxInference(
    const juce::String& textPrompt,
    const juce::AudioSampleBuffer* audioReference,
    const std::vector<juce::String>& styleTags) {

    GenerationResult result;
    result.confidence = 0.0f;

#ifdef LIANCORE_HAS_ONNX
    try {
        if (!ortSession_) return result;

        // 准备输入张量 (文本编码为浮点向量)
        // 简化实现: 将文本转换为特征向量
        std::vector<float> textFeatures(128, 0.0f);
        juce::String lowerText = textPrompt.toLowerCase();
        
        // 简单文本编码: 基于字符哈希的特征
        for (int i = 0; i < lowerText.length() && i < 128; ++i) {
            textFeatures[i % 128] += static_cast<float>(lowerText[i]) / 255.0f;
        }

        // 创建输入张量
        std::vector<int64_t> inputShape = {1, 128};
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            *ortMemoryInfo_,
            textFeatures.data(),
            textFeatures.size(),
            inputShape.data(),
            inputShape.size()
        );

        // 运行推理
        auto outputTensors = ortSession_->Run(
            Ort::RunOptions{nullptr},
            ortInputNames_.data(),
            &inputTensor,
            1,
            ortOutputNames_.data(),
            ortOutputNames_.size()
        );

        if (!outputTensors.empty()) {
            // 解析输出 (参数向量)
            float* outputData = outputTensors[0].GetTensorMutableData<float>();
            auto outputShape = outputTensors[0].GetTensorTypeAndShapeInfo().GetShape();
            size_t outputSize = outputShape[0] > 0 ? static_cast<size_t>(outputShape[1]) : 0;

            // 映射到参数
            result.confidence = 0.85f;
            result.presetName = "ONNX_" + textPrompt.substring(0, 25).replace(" ", "_");

            // 将输出向量映射到已知参数
            static const std::vector<juce::String> paramNames = {
                "filter_cutoff", "filter_resonance", "osc_waveform",
                "env_attack", "env_decay", "env_sustain", "env_release",
                "lfo_rate", "lfo_depth", "reverb_size", "noise_level"
            };

            for (size_t i = 0; i < outputSize && i < paramNames.size(); ++i) {
                ParameterMapping mapping;
                mapping.parameterId = paramNames[i];
                mapping.value = AudioUtils::clamp(outputData[i], 0.0f, 1.0f);
                mapping.explanation = "ONNX模型推理: " + paramNames[i];
                result.parameters.push_back(mapping);
            }
        }
    } catch (const std::exception& e) {
        DBG("ONNX inference failed: " << e.what());
        result.confidence = 0.0f;
    }
#else
    juce::ignoreUnused(textPrompt, audioReference, styleTags);
#endif

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
        for (int f = 0; f < numFrames; ++f) {
            float harmonics = 1.0f - static_cast<float>(f) / numFrames;
            int numHarm = std::max(1, static_cast<int>(64.0f * harmonics));
            auto* frameData = wavetable.getWritePointer(0) + f * frameSize;
            for (int i = 0; i < frameSize; ++i) {
                float phase = static_cast<float>(i) / frameSize;
                float sample = 0.0f;
                for (int h = 1; h <= numHarm; ++h) {
                    sample += std::sin(static_cast<float>(AudioUtils::kTwoPI) * phase * h) / h * ((h % 2 == 0) ? -1.0f : 1.0f);
                }
                frameData[i] = sample;
            }
        }
    } else if (lowerDesc.contains("方波") || lowerDesc.contains("square") || lowerDesc.contains("脉冲")) {
        for (int f = 0; f < numFrames; ++f) {
            float pw = 0.5f - 0.4f * static_cast<float>(f) / numFrames;
            auto* frameData = wavetable.getWritePointer(0) + f * frameSize;
            for (int i = 0; i < frameSize; ++i) {
                float phase = static_cast<float>(i) / frameSize;
                frameData[i] = phase < pw ? 1.0f : -1.0f;
            }
        }
    } else if (lowerDesc.contains("三角") || lowerDesc.contains("triangle")) {
        for (int f = 0; f < numFrames; ++f) {
            auto* frameData = wavetable.getWritePointer(0) + f * frameSize;
            for (int i = 0; i < frameSize; ++i) {
                float phase = static_cast<float>(i) / frameSize;
                frameData[i] = 4.0f * std::abs(phase - 0.5f) - 1.0f;
            }
        }
    } else if (lowerDesc.contains("噪声") || lowerDesc.contains("noise")) {
        auto& rng = AudioUtils::getThreadLocalRNG();
        for (int f = 0; f < numFrames; ++f) {
            auto* frameData = wavetable.getWritePointer(0) + f * frameSize;
            for (int i = 0; i < frameSize; ++i) {
                frameData[i] = rng.nextFloat(-1.0f, 1.0f);
            }
        }
    } else {
        // 默认: 正弦波
        for (int f = 0; f < numFrames; ++f) {
            auto* frameData = wavetable.getWritePointer(0) + f * frameSize;
            for (int i = 0; i < frameSize; ++i) {
                frameData[i] = std::sin(static_cast<float>(AudioUtils::kTwoPI) * i / frameSize);
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
    int fftOrder = static_cast<int>(std::log2(numSamples));
    if (fftOrder < 6) fftOrder = 6;
    if (fftOrder > 14) fftOrder = 14;

    int fftSize = 1 << fftOrder;
    std::vector<float> spectrum(fftSize / 2, 0.0f);

    juce::dsp::FFT fft(fftOrder);
    std::vector<float> fftData(fftSize * 2, 0.0f);

    const float* audioData = audio.getReadPointer(0);
    int copySize = std::min(numSamples, fftSize);
    for (int i = 0; i < copySize; ++i) {
        fftData[i * 2] = audioData[i];
    }

    fft.performFrequencyOnlyForwardTransform(fftData.data());

    for (int i = 0; i < static_cast<int>(spectrum.size()); ++i) {
        spectrum[i] = std::abs(fftData[i]);
    }

    return spectrum;
}

std::vector<float> AIInferenceEngine::extractAudioEmbedding(const juce::AudioSampleBuffer& audio) {
    // Beta阶段: 使用频谱特征作为音频嵌入
    auto spectrum = analyzeReferenceSpectrum(audio);

    // 降采样到128维嵌入
    std::vector<float> embedding(128, 0.0f);
    if (!spectrum.empty()) {
        float ratio = static_cast<float>(spectrum.size()) / 128.0f;
        for (int i = 0; i < 128; ++i) {
            embedding[i] = spectrum[static_cast<size_t>(i * ratio)];
        }
    }

    return embedding;
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
        {"env_decay", "包络衰减"},
        {"env_sustain", "包络保持"},
        {"env_release", "包络释音"},
        {"lfo_rate", "LFO速率"},
        {"lfo_depth", "LFO深度"},
        {"reverb_size", "混响空间"},
        {"noise_level", "噪声电平"},
        {"osc_pitch", "音高偏移"},
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