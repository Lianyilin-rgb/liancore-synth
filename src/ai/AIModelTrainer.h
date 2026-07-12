// =============================================================================
// LianCore - AIModelTrainer AI模型训练器 (Beta阶段: 简单线性回归 + ONNX导出)
// 支持文本→参数映射训练、ONNX模型导出、模型评估
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <string>
#include <random>
#include <cmath>
#include <functional>

namespace LianCore {

// =============================================================================
// TrainingData - 训练数据结构
// =============================================================================
struct TrainingData {
    // 文本提示词列表
    std::vector<std::string> textPrompts;
    // 参数向量 (每个128维浮点向量)
    std::vector<std::vector<float>> parameterVectors;
    // 音频嵌入 (128维)
    std::vector<std::vector<float>> audioEmbeddings;
    // 风格标签列表
    std::vector<std::string> styleTags;

    // ---- 序列化 ----
    void loadFromCSV(const juce::File& file);
    void saveToCSV(const juce::File& file);

    // ---- JSON ----
    juce::var toJson() const;
    void fromJson(const juce::var& json);

    // ---- 工具 ----
    int size() const { return static_cast<int>(textPrompts.size()); }
    bool isEmpty() const { return textPrompts.empty(); }
    void clear();
    void addExample(const std::string& text, const std::vector<float>& params,
                    const std::vector<float>& audioEmbed, const std::string& tags);
};

// =============================================================================
// TrainingConfig - 训练配置
// =============================================================================
struct TrainingConfig {
    int epochs = 100;               // 训练轮数
    int batchSize = 32;             // 批量大小
    float learningRate = 0.001f;    // 学习率
    float validationSplit = 0.2f;   // 验证集比例
    int earlyStoppingPatience = 10; // 早停耐心值(连续无改善轮数)
    float l2Regularization = 0.0f;  // L2正则化系数
    std::string outputPath;         // 模型输出路径

    juce::var toJson() const;
    void fromJson(const juce::var& json);
};

// =============================================================================
// ModelMetrics - 模型评估指标
// =============================================================================
struct ModelMetrics {
    float mse = 0.0f;   // 均方误差
    float mae = 0.0f;   // 平均绝对误差
    float r2 = 0.0f;    // R²决定系数
    float rmse = 0.0f;  // 均方根误差

    juce::var toJson() const;
    void fromJson(const juce::var& json);
};

// =============================================================================
// AIModelTrainer - AI模型训练器
// =============================================================================
class AIModelTrainer {
public:
    // ONNX模型元数据常量
    static constexpr const char* kInputName = "text_features";
    static constexpr const char* kOutputName = "parameters";
    static constexpr const char* kWeightName = "W";
    static constexpr const char* kBiasName = "b";
    static constexpr const char* kModelProducer = "LianCore";
    static constexpr int kFeatureDim = 128;  // 特征维度

    AIModelTrainer();
    ~AIModelTrainer();

    // =========================================================================
    // 数据管理
    // =========================================================================
    // 添加单个训练样本
    void addTrainingExample(const std::string& text,
                            const std::vector<float>& params,
                            const std::vector<float>& audioEmbedding,
                            const std::string& tags);

    // 从CSV加载训练数据
    bool loadTrainingData(const juce::File& csvFile);

    // 导出训练数据到CSV
    bool exportTrainingData(const juce::File& csvFile);

    // 获取训练数据集
    const TrainingData& getTrainingData() const { return trainingData_; }
    int getSampleCount() const { return trainingData_.size(); }

    // =========================================================================
    // 模型训练
    // =========================================================================
    // 训练模型 (占位，当前调用简单线性模型训练)
    bool trainModel(const TrainingConfig& config);

    // 简单线性回归模型训练 (文本特征 → 参数映射)
    // 实现: 哈希文本特征 → 128维向量 → 线性变换 W(128×128)+b(128) → 参数预测
    bool trainSimpleLinearModel(const TrainingConfig& config);

    // =========================================================================
    // 模型导出
    // =========================================================================
    // 导出训练好的模型为ONNX格式
    bool exportONNXModel(const juce::File& outputPath);

    // =========================================================================
    // 模型评估
    // =========================================================================
    // 评估模型性能 (返回MSE/MAE/R²)
    ModelMetrics evaluateModel();

    // =========================================================================
    // 状态查询
    // =========================================================================
    // 获取训练进度 (0.0 ~ 1.0)
    float getTrainingProgress() const { return trainingProgress_; }

    // 获取模型指标
    ModelMetrics getModelMetrics() const { return currentMetrics_; }

    // 是否已训练
    bool isModelTrained() const { return modelTrained_; }

    // =========================================================================
    // 推理 (用于验证)
    // =========================================================================
    // 使用训练好的模型进行预测
    std::vector<float> predict(const std::string& text);

private:
    // =========================================================================
    // 文本特征提取
    // =========================================================================
    // 基于哈希的文本→128维特征向量
    std::vector<float> extractTextFeatures(const std::string& text);

    // =========================================================================
    // 训练辅助
    // =========================================================================
    // 前向传播: y = W * x + b
    std::vector<float> forwardPass(const std::vector<float>& features);

    // 计算MSE损失
    float computeMSELoss(const std::vector<std::vector<float>>& predictions,
                         const std::vector<std::vector<float>>& targets);

    // 随机梯度下降一步 (对单个样本)
    void sgdStep(const std::vector<float>& features,
                 const std::vector<float>& target,
                 float learningRate);

    // 随机分割训练集/验证集
    void splitData(std::vector<int>& trainIndices,
                   std::vector<int>& valIndices,
                   float validationSplit);

    // =========================================================================
    // ONNX序列化辅助
    // =========================================================================
    // Protobuf编码辅助函数
    static void writeVarint(std::vector<uint8_t>& buf, uint64_t value);
    static void writeTag(std::vector<uint8_t>& buf, int fieldNum, int wireType);
    static void writeLengthDelimited(std::vector<uint8_t>& buf, int fieldNum,
                                     const std::vector<uint8_t>& data);
    static void writeString(std::vector<uint8_t>& buf, int fieldNum,
                            const std::string& str);
    static void writeInt32(std::vector<uint8_t>& buf, int fieldNum, int32_t value);
    static void writeInt64(std::vector<uint8_t>& buf, int fieldNum, int64_t value);
    static void writeFloat(std::vector<uint8_t>& buf, int fieldNum, float value);
    static void writeFloatRepeated(std::vector<uint8_t>& buf, int fieldNum,
                                   const std::vector<float>& values);
    static void writeInt64Repeated(std::vector<uint8_t>& buf, int fieldNum,
                                   const std::vector<int64_t>& values);

    // 构建ONNX ModelProto
    std::vector<uint8_t> buildONNXModelProto();

    // =========================================================================
    // 成员变量
    // =========================================================================
    TrainingData trainingData_;           // 训练数据集
    std::vector<std::vector<float>> W_;   // 权重矩阵 128×128
    std::vector<float> b_;                // 偏置向量 128
    bool modelTrained_ = false;           // 模型是否已训练
    float trainingProgress_ = 0.0f;       // 训练进度
    ModelMetrics currentMetrics_;         // 当前模型指标
    int totalEpochs_ = 0;                 // 已完成的总轮数

    // 随机数生成器
    std::mt19937 rng_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AIModelTrainer)
};

} // namespace LianCore