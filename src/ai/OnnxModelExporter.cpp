// =============================================================================
// LianCore - OnnxModelExporter 实现 (Beta Week 7)
// =============================================================================
#include "OnnxModelExporter.h"
#include <random>
#include <algorithm>
#include <cmath>

namespace LianCore {

// =============================================================================
// 关键词规则 (与 AIInferenceEngine::buildKeywordRules 完全同步)
// =============================================================================
static const std::vector<KeywordRuleExport> s_keywordRules = {
    // 音色 → 滤波器截止频率
    {"明亮", "filter_cutoff", 0.8f}, {"温暖", "filter_cutoff", 0.3f},
    {"暗", "filter_cutoff", 0.15f}, {"尖锐", "filter_cutoff", 0.9f},
    {"柔和", "filter_cutoff", 0.25f}, {"厚重", "filter_cutoff", 0.2f},
    {"轻盈", "filter_cutoff", 0.7f},
    // 情感 → 滤波器共振
    {"紧张", "filter_resonance", 0.7f}, {"放松", "filter_resonance", 0.2f},
    {"梦幻", "filter_resonance", 0.6f}, {"空灵", "filter_resonance", 0.5f},
    // 风格 → 振荡器波形
    {"复古", "osc_waveform", 0.25f}, {"现代", "osc_waveform", 0.5f},
    {"电子", "osc_waveform", 0.75f}, {"经典", "osc_waveform", 0.3f},
    {"管弦", "osc_waveform", 0.15f}, {"贝斯", "osc_waveform", 0.4f},
    {"主音", "osc_waveform", 0.6f}, {"铺底", "osc_waveform", 0.35f},
    // 动态 → 包络
    {"快速", "env_attack", 0.1f}, {"慢速", "env_attack", 0.5f},
    {"长音", "env_release", 0.8f}, {"短促", "env_release", 0.1f},
    {"打击", "env_attack", 0.05f}, {"弹拨", "env_attack", 0.15f},
    // 混响
    {"大厅", "reverb_size", 0.8f}, {"房间", "reverb_size", 0.3f},
    {"环境", "reverb_size", 0.5f},
    // 音高
    {"低音", "osc_pitch", 0.0f}, {"高音", "osc_pitch", 1.0f},
    // 噪声
    {"噪声", "noise_level", 0.3f}, {"纯净", "noise_level", 0.0f},
};

const std::vector<KeywordRuleExport>& getKeywordRulesForExport() {
    return s_keywordRules;
}

// =============================================================================
// 输出参数名称列表
// =============================================================================
static const std::vector<juce::String> s_outputParamNames = {
    "filter_cutoff", "filter_resonance", "osc_waveform",
    "env_attack", "env_decay", "env_sustain", "env_release",
    "lfo_rate", "lfo_depth", "reverb_size", "noise_level"
};

const std::vector<juce::String>& OnnxModelExporter::getOutputParameterNames() {
    return s_outputParamNames;
}

// =============================================================================
// 构造与析构
// =============================================================================
OnnxModelExporter::OnnxModelExporter() {
    config_.inputDim = 128;
    config_.hiddenDim = 64;
    config_.hiddenDim2 = 32;
    config_.outputDim = 11;
    config_.opsetVersion = 14;
}

OnnxModelExporter::~OnnxModelExporter() = default;

// =============================================================================
// ONNX 可用性检查
// =============================================================================
bool OnnxModelExporter::isOnnxAvailable() {
#ifdef LIANCORE_HAS_ONNX
    return true;
#else
    return false;
#endif
}

// =============================================================================
// 文本特征编码
// =============================================================================
std::vector<float> OnnxModelExporter::encodeTextFeatures(
    const juce::String& text, int dim) {

    std::vector<float> features(static_cast<size_t>(dim), 0.0f);
    juce::String lowerText = text.toLowerCase();

    // 简单字符哈希编码 (与 AIInferenceEngine::runOnnxInference 一致)
    for (int i = 0; i < lowerText.length() && i < dim; ++i) {
        features[static_cast<size_t>(i % dim)] +=
            static_cast<float>(lowerText[i]) / 255.0f;
    }

    // L2归一化
    float norm = 0.0f;
    for (float f : features) norm += f * f;
    norm = std::sqrt(norm);
    if (norm > 1e-6f) {
        for (float& f : features) f /= norm;
    }

    return features;
}

// =============================================================================
// 生成合成训练数据
// =============================================================================
std::vector<OnnxModelExporter::TrainingSample>
OnnxModelExporter::generateTrainingData(int numSamples, unsigned int seed) {

    std::vector<TrainingSample> samples;
    samples.reserve(static_cast<size_t>(numSamples));

    std::mt19937 rng(seed);
    std::uniform_int_distribution<size_t> ruleDist(0, s_keywordRules.size() - 1);
    std::uniform_int_distribution<int> numKeywordsDist(1, 5);

    // 默认参数值 (0.5 = 中间值)
    std::vector<float> defaultParams(11, 0.5f);

    for (int i = 0; i < numSamples; ++i) {
        int numKeywords = numKeywordsDist(rng);
        std::vector<size_t> selectedIndices;
        selectedIndices.reserve(static_cast<size_t>(numKeywords));

        // 随机选择关键词 (无重复)
        std::vector<bool> used(s_keywordRules.size(), false);
        for (int k = 0; k < numKeywords; ++k) {
            size_t idx;
            do {
                idx = ruleDist(rng);
            } while (used[idx]);
            used[idx] = true;
            selectedIndices.push_back(idx);
        }

        // 构建文本
        juce::String text;
        for (size_t idx : selectedIndices) {
            text += s_keywordRules[idx].keyword + " ";
        }

        // 编码文本特征
        auto features = encodeTextFeatures(text.trimEnd(), 128);

        // 计算目标参数
        auto targets = defaultParams;
        for (size_t idx : selectedIndices) {
            const auto& rule = s_keywordRules[idx];
            auto paramIt = std::find(
                s_outputParamNames.begin(),
                s_outputParamNames.end(),
                rule.parameterId
            );
            if (paramIt != s_outputParamNames.end()) {
                size_t paramIdx = static_cast<size_t>(
                    std::distance(s_outputParamNames.begin(), paramIt));
                targets[paramIdx] = rule.targetValue * 0.7f + targets[paramIdx] * 0.3f;
            }
        }

        TrainingSample sample;
        sample.inputFeatures = std::move(features);
        sample.targetParams = std::move(targets);
        samples.push_back(std::move(sample));
    }

    return samples;
}

// =============================================================================
// 验证 ONNX 模型文件
// =============================================================================
OnnxModelExporter::ModelInfo OnnxModelExporter::validateModelFile(
    const juce::File& onnxFile) {

    ModelInfo info;
    info.modelPath = onnxFile.getFullPathName();
    info.modelName = onnxFile.getFileName();
    info.fileSize = onnxFile.existsAsFile() ? onnxFile.getSize() : 0;
    info.isValid = false;

    if (!onnxFile.existsAsFile()) {
        info.modelName = "文件不存在";
        return info;
    }

    // 检查文件扩展名
    if (!onnxFile.getFileExtension().equalsIgnoreCase(".onnx")) {
        info.modelName = "非ONNX文件";
        return info;
    }

    // 检查文件大小 (至少应该有几个KB)
    if (info.fileSize < 1024) {
        info.modelName = "文件过小";
        return info;
    }

    // 读取 ONNX 文件头验证 (ONNX 文件以特定的 protobuf 格式开头)
    juce::FileInputStream stream(onnxFile);
    if (!stream.openedOk()) {
        info.modelName = "无法读取文件";
        return info;
    }

    // ONNX 文件是 protobuf 二进制格式，简单验证前几个字节
    // protobuf 通常以 0x08 或 0x0A 开头
    char header[4];
    if (stream.read(header, 4) == 4) {
        // 尝试读取文件中的模型信息
        // 简化的验证: 检查文件头是否合理
        if (static_cast<unsigned char>(header[0]) < 0x20) {
            info.isValid = true;
            info.producerName = "LianCore";
            info.producerVersion = "3.0.0";
            info.inputDim = 128;
            info.outputDim = 11;
        }
    }

    return info;
}

// =============================================================================
// 验证推理输出有效性
// =============================================================================
bool OnnxModelExporter::validateInferenceOutput(
    const std::vector<float>& output,
    float minValue,
    float maxValue) {

    if (output.empty()) return false;

    // 检查所有值在有效范围内
    for (float val : output) {
        if (val < minValue || val > maxValue) {
            return false;
        }
        // 检查 NaN 和 Inf
        if (std::isnan(val) || std::isinf(val)) {
            return false;
        }
    }

    return true;
}

} // namespace LianCore