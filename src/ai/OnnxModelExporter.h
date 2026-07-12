// =============================================================================
// LianCore - OnnxModelExporter ONNX模型导出器 (Beta Week 7)
// 运行时编程创建ONNX模型，无需Python依赖
// 支持加载预训练 ONNX 模型文件
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <string>
#include <memory>

namespace LianCore {

// =============================================================================
// OnnxModelExporter - 运行时 ONNX 模型创建与管理
// =============================================================================
class OnnxModelExporter {
public:
    // 模型架构配置
    struct ModelConfig {
        int inputDim = 128;       // 文本特征维度
        int hiddenDim = 64;       // 隐藏层1维度
        int hiddenDim2 = 32;      // 隐藏层2维度
        int outputDim = 11;       // 输出参数维度
        int opsetVersion = 14;    // ONNX opset版本
    };

    // 模型信息
    struct ModelInfo {
        juce::String modelPath;
        juce::String modelName;
        juce::String producerName;
        juce::String producerVersion;
        int64_t inputDim;
        int64_t outputDim;
        int64_t fileSize;
        bool isValid;
    };

    OnnxModelExporter();
    ~OnnxModelExporter();

    // =========================================================================
    // 模型导出
    // =========================================================================

    // 检查是否支持 ONNX 导出
    static bool isOnnxAvailable();

    // 从关键词规则生成训练数据
    struct TrainingSample {
        std::vector<float> inputFeatures;   // 128维文本特征
        std::vector<float> targetParams;    // 11维参数向量
    };

    // 生成合成训练数据 (基于关键词规则)
    static std::vector<TrainingSample> generateTrainingData(
        int numSamples = 2000,
        unsigned int seed = 42
    );

    // 文本特征编码 (与 C++ AIInferenceEngine 编码方式一致)
    static std::vector<float> encodeTextFeatures(
        const juce::String& text,
        int dim = 128
    );

    // 验证 ONNX 模型文件
    static ModelInfo validateModelFile(const juce::File& onnxFile);

    // 获取输出参数名称列表
    static const std::vector<juce::String>& getOutputParameterNames();

    // =========================================================================
    // 端到端推理测试 (Beta阶段: 验证 ONNX 推理流程)
    // =========================================================================

    // 运行推理基准测试
    struct BenchmarkResult {
        double avgInferenceTimeMs = 0.0;
        double minInferenceTimeMs = 0.0;
        double maxInferenceTimeMs = 0.0;
        int numSamples = 0;
        int numSuccess = 0;
        int numFailures = 0;
    };

    // 验证推理结果有效性
    static bool validateInferenceOutput(
        const std::vector<float>& output,
        float minValue = 0.0f,
        float maxValue = 1.0f
    );

private:
    ModelConfig config_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OnnxModelExporter)
};

// =============================================================================
// 关键词规则定义 (与 AIInferenceEngine::buildKeywordRules 同步)
// =============================================================================
struct KeywordRuleExport {
    juce::String keyword;
    juce::String parameterId;
    float targetValue;
};

const std::vector<KeywordRuleExport>& getKeywordRulesForExport();

} // namespace LianCore