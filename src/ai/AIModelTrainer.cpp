// =============================================================================
// LianCore - AIModelTrainer 实现 (Beta阶段: 简单线性回归 + ONNX导出)
// 完整的文本→参数映射训练管线
// =============================================================================
#include "AIModelTrainer.h"
#include "../utils/AudioUtils.h"
#include <algorithm>
#include <numeric>
#include <sstream>
#include <cstring>

namespace LianCore {

// =============================================================================
// TrainingData 实现
// =============================================================================

void TrainingData::clear() {
    textPrompts.clear();
    parameterVectors.clear();
    audioEmbeddings.clear();
    styleTags.clear();
}

void TrainingData::addExample(const std::string& text,
                              const std::vector<float>& params,
                              const std::vector<float>& audioEmbed,
                              const std::string& tags) {
    textPrompts.push_back(text);
    // 确保参数向量是128维，不足补0，超出截断
    std::vector<float> paddedParams(128, 0.0f);
    size_t copySize = std::min(params.size(), static_cast<size_t>(128));
    std::copy_n(params.begin(), copySize, paddedParams.begin());
    parameterVectors.push_back(std::move(paddedParams));

    // 音频嵌入同理
    std::vector<float> paddedEmbed(128, 0.0f);
    size_t embedCopy = std::min(audioEmbed.size(), static_cast<size_t>(128));
    std::copy_n(audioEmbed.begin(), embedCopy, paddedEmbed.begin());
    audioEmbeddings.push_back(std::move(paddedEmbed));

    styleTags.push_back(tags);
}

// =============================================================================
// CSV格式:
// text_prompt,param0,param1,...,param127,embed0,embed1,...,embed127,style_tags
// 文本提示词用引号包裹以处理逗号
// =============================================================================
void TrainingData::loadFromCSV(const juce::File& file) {
    clear();

    if (!file.existsAsFile()) {
        DBG("TrainingData CSV file not found: " << file.getFullPathName());
        return;
    }

    juce::StringArray lines;
    file.readLines(lines);

    for (const auto& line : lines) {
        if (line.isEmpty() || line.startsWith("#")) continue; // 跳过空行和注释

        // 解析CSV行：处理引号包裹的文本字段
        auto tokens = juce::StringArray::fromTokens(line, ",", "\"");
        if (tokens.size() < 258) continue; // 至少需要: 1文本 + 128参数 + 128嵌入 + 1标签

        // 第0列: 文本提示词
        std::string text = tokens[0].unquoted().toStdString();

        // 第1~128列: 参数向量
        std::vector<float> params(128, 0.0f);
        for (int i = 0; i < 128; ++i) {
            params[i] = tokens[i + 1].getFloatValue();
        }

        // 第129~256列: 音频嵌入
        std::vector<float> embeds(128, 0.0f);
        for (int i = 0; i < 128; ++i) {
            embeds[i] = tokens[i + 129].getFloatValue();
        }

        // 第257列: 风格标签
        std::string tags = (tokens.size() > 257)
            ? tokens[257].unquoted().toStdString()
            : "";

        addExample(text, params, embeds, tags);
    }

    DBG("TrainingData loaded " << size() << " samples from CSV: " << file.getFullPathName());
}

void TrainingData::saveToCSV(const juce::File& file) {
    // 确保目录存在
    auto parentDir = file.getParentDirectory();
    if (!parentDir.exists()) {
        parentDir.createDirectory();
    }

    juce::StringArray lines;

    // 写入注释头
    lines.add("# LianCore Training Data");
    lines.add("# Format: text_prompt,param[0..127],audio_embed[0..127],style_tags");
    lines.add("");

    for (int i = 0; i < size(); ++i) {
        juce::String row;
        // 文本提示词 (用引号包裹)
        row << "\"" << juce::String(textPrompts[i]).replace("\"", "\"\"") << "\"";

        // 参数向量
        for (int j = 0; j < 128; ++j) {
            float val = (j < static_cast<int>(parameterVectors[i].size()))
                ? parameterVectors[i][j] : 0.0f;
            row << "," << juce::String(val, 6);
        }

        // 音频嵌入
        for (int j = 0; j < 128; ++j) {
            float val = (j < static_cast<int>(audioEmbeddings[i].size()))
                ? audioEmbeddings[i][j] : 0.0f;
            row << "," << juce::String(val, 6);
        }

        // 风格标签
        row << ",\"" << juce::String(styleTags[i]).replace("\"", "\"\"") << "\"";

        lines.add(row);
    }

    file.replaceWithText(lines.joinIntoString("\n"));
    DBG("TrainingData saved " << size() << " samples to CSV: " << file.getFullPathName());
}

juce::var TrainingData::toJson() const {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();

    juce::Array<juce::var> texts;
    for (const auto& t : textPrompts) texts.add(juce::String(t));
    obj->setProperty("textPrompts", texts);

    juce::Array<juce::var> params;
    for (const auto& pv : parameterVectors) {
        juce::Array<juce::var> arr;
        for (float v : pv) arr.add(v);
        params.add(arr);
    }
    obj->setProperty("parameterVectors", params);

    juce::Array<juce::var> embeds;
    for (const auto& ev : audioEmbeddings) {
        juce::Array<juce::var> arr;
        for (float v : ev) arr.add(v);
        embeds.add(arr);
    }
    obj->setProperty("audioEmbeddings", embeds);

    juce::Array<juce::var> tags;
    for (const auto& t : styleTags) tags.add(juce::String(t));
    obj->setProperty("styleTags", tags);

    return juce::var(obj.get());
}

void TrainingData::fromJson(const juce::var& json) {
    clear();
    if (!json.isObject()) return;

    auto obj = json.getDynamicObject();
    if (!obj) return;

    auto texts = obj->getProperty("textPrompts");
    auto params = obj->getProperty("parameterVectors");
    auto embeds = obj->getProperty("audioEmbeddings");
    auto tags = obj->getProperty("styleTags");

    int count = std::min({
        texts.size(),
        params.size(),
        embeds.size(),
        tags.size()
    });

    for (int i = 0; i < count; ++i) {
        std::string text = texts[i].toString().toStdString();

        std::vector<float> pv(128, 0.0f);
        auto& pArr = *params[i].getArray();
        for (int j = 0; j < std::min(128, pArr.size()); ++j) {
            pv[j] = static_cast<float>(pArr[j]);
        }

        std::vector<float> ev(128, 0.0f);
        auto& eArr = *embeds[i].getArray();
        for (int j = 0; j < std::min(128, eArr.size()); ++j) {
            ev[j] = static_cast<float>(eArr[j]);
        }

        std::string tag = tags[i].toString().toStdString();

        addExample(text, pv, ev, tag);
    }
}

// =============================================================================
// TrainingConfig 实现
// =============================================================================
juce::var TrainingConfig::toJson() const {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("epochs", epochs);
    obj->setProperty("batchSize", batchSize);
    obj->setProperty("learningRate", learningRate);
    obj->setProperty("validationSplit", validationSplit);
    obj->setProperty("earlyStoppingPatience", earlyStoppingPatience);
    obj->setProperty("l2Regularization", l2Regularization);
    obj->setProperty("outputPath", juce::String(outputPath));
    return juce::var(obj.get());
}

void TrainingConfig::fromJson(const juce::var& json) {
    if (!json.isObject()) return;
    auto obj = json.getDynamicObject();
    if (!obj) return;

    epochs = obj->getProperty("epochs");
    batchSize = obj->getProperty("batchSize");
    learningRate = obj->getProperty("learningRate");
    validationSplit = obj->getProperty("validationSplit");
    earlyStoppingPatience = obj->getProperty("earlyStoppingPatience");
    l2Regularization = obj->getProperty("l2Regularization");
    outputPath = obj->getProperty("outputPath").toString().toStdString();
}

// =============================================================================
// ModelMetrics 实现
// =============================================================================
juce::var ModelMetrics::toJson() const {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("mse", mse);
    obj->setProperty("mae", mae);
    obj->setProperty("r2", r2);
    obj->setProperty("rmse", rmse);
    return juce::var(obj.get());
}

void ModelMetrics::fromJson(const juce::var& json) {
    if (!json.isObject()) return;
    auto obj = json.getDynamicObject();
    if (!obj) return;

    mse = obj->getProperty("mse");
    mae = obj->getProperty("mae");
    r2 = obj->getProperty("r2");
    rmse = obj->getProperty("rmse");
}

// =============================================================================
// AIModelTrainer 实现
// =============================================================================
AIModelTrainer::AIModelTrainer()
    : rng_(std::random_device{}()) {
    // 初始化模型参数为零
    W_.resize(kFeatureDim, std::vector<float>(kFeatureDim, 0.0f));
    b_.resize(kFeatureDim, 0.0f);

    // Xavier初始化权重矩阵
    float scale = std::sqrt(2.0f / static_cast<float>(kFeatureDim));
    std::normal_distribution<float> dist(0.0f, scale);
    for (int i = 0; i < kFeatureDim; ++i) {
        for (int j = 0; j < kFeatureDim; ++j) {
            W_[i][j] = dist(rng_);
        }
    }
}

AIModelTrainer::~AIModelTrainer() = default;

// =============================================================================
// 数据管理
// =============================================================================
void AIModelTrainer::addTrainingExample(const std::string& text,
                                        const std::vector<float>& params,
                                        const std::vector<float>& audioEmbedding,
                                        const std::string& tags) {
    trainingData_.addExample(text, params, audioEmbedding, tags);
}

bool AIModelTrainer::loadTrainingData(const juce::File& csvFile) {
    trainingData_.loadFromCSV(csvFile);
    return !trainingData_.isEmpty();
}

bool AIModelTrainer::exportTrainingData(const juce::File& csvFile) {
    if (trainingData_.isEmpty()) {
        DBG("No training data to export");
        return false;
    }
    trainingData_.saveToCSV(csvFile);
    return true;
}

// =============================================================================
// 文本特征提取: 基于哈希的128维特征向量
// =============================================================================
std::vector<float> AIModelTrainer::extractTextFeatures(const std::string& text) {
    std::vector<float> features(kFeatureDim, 0.0f);

    if (text.empty()) return features;

    // 使用FNV-1a哈希为每个字符位置生成特征索引
    // 多个哈希函数组合以增加特征丰富度
    for (size_t i = 0; i < text.length(); ++i) {
        unsigned char ch = static_cast<unsigned char>(text[i]);

        // 哈希1: 主特征 (字符直接映射到维度)
        int idx1 = static_cast<int>(ch) % kFeatureDim;
        features[idx1] += 1.0f;

        // 哈希2: 位置加权 (字符在文本中的位置影响特征)
        int idx2 = (static_cast<int>(ch) * 31 + static_cast<int>(i)) % kFeatureDim;
        features[idx2] += 1.0f / static_cast<float>(i + 1);

        // 哈希3: 双字符组合 (bigram)
        if (i + 1 < text.length()) {
            unsigned char ch2 = static_cast<unsigned char>(text[i + 1]);
            int idx3 = (static_cast<int>(ch) * 257 + static_cast<int>(ch2)) % kFeatureDim;
            features[idx3] += 0.5f;
        }

        // 哈希4: 三字符组合 (trigram)
        if (i + 2 < text.length()) {
            unsigned char ch2 = static_cast<unsigned char>(text[i + 1]);
            unsigned char ch3 = static_cast<unsigned char>(text[i + 2]);
            int idx4 = (static_cast<int>(ch) * 65537 + static_cast<int>(ch2) * 257
                        + static_cast<int>(ch3)) % kFeatureDim;
            features[idx4] += 0.25f;
        }
    }

    // 词级特征 (基于空格分词)
    std::istringstream iss(text);
    std::string word;
    int wordIdx = 0;
    while (iss >> word && wordIdx < 32) {
        // 词的FNV-1a哈希
        uint32_t hash = 2166136261u;
        for (char c : word) {
            hash ^= static_cast<uint32_t>(static_cast<unsigned char>(c));
            hash *= 16777619u;
        }
        int idx = static_cast<int>(hash % static_cast<uint32_t>(kFeatureDim));
        features[idx] += 0.5f;
        ++wordIdx;
    }

    // 归一化特征向量到单位长度
    float norm = 0.0f;
    for (float f : features) norm += f * f;
    norm = std::sqrt(norm);
    if (norm > 1e-8f) {
        float invNorm = 1.0f / norm;
        for (auto& f : features) f *= invNorm;
    }

    return features;
}

// =============================================================================
// 前向传播: y = W * x + b
// =============================================================================
std::vector<float> AIModelTrainer::forwardPass(const std::vector<float>& features) {
    std::vector<float> output(kFeatureDim, 0.0f);

    for (int i = 0; i < kFeatureDim; ++i) {
        float sum = b_[i];
        for (int j = 0; j < kFeatureDim; ++j) {
            sum += W_[i][j] * features[j];
        }
        output[i] = sum;
    }

    return output;
}

// =============================================================================
// 计算MSE损失
// =============================================================================
float AIModelTrainer::computeMSELoss(const std::vector<std::vector<float>>& predictions,
                                     const std::vector<std::vector<float>>& targets) {
    if (predictions.empty() || targets.empty()) return 0.0f;

    float totalLoss = 0.0f;
    int count = 0;

    for (size_t i = 0; i < predictions.size(); ++i) {
        for (size_t j = 0; j < predictions[i].size() && j < targets[i].size(); ++j) {
            float diff = predictions[i][j] - targets[i][j];
            totalLoss += diff * diff;
            ++count;
        }
    }

    return (count > 0) ? (totalLoss / static_cast<float>(count)) : 0.0f;
}

// =============================================================================
// 随机梯度下降一步 (对单个样本)
// 梯度计算: dL/dW[i][j] = 2 * (y_pred[i] - y_target[i]) * x[j] / dim
//           dL/db[i]   = 2 * (y_pred[i] - y_target[i]) / dim
// =============================================================================
void AIModelTrainer::sgdStep(const std::vector<float>& features,
                             const std::vector<float>& target,
                             float learningRate) {
    // 前向传播获取预测值
    std::vector<float> predictions = forwardPass(features);

    // 对于每个输出维度计算梯度并更新
    float scale = 2.0f * learningRate / static_cast<float>(kFeatureDim);

    for (int i = 0; i < kFeatureDim; ++i) {
        float error = predictions[i] - target[i];
        float gradScale = error * scale;

        // 更新偏置
        b_[i] -= gradScale;

        // 更新权重
        for (int j = 0; j < kFeatureDim; ++j) {
            W_[i][j] -= gradScale * features[j];
        }
    }
}

// =============================================================================
// 随机分割训练集/验证集
// =============================================================================
void AIModelTrainer::splitData(std::vector<int>& trainIndices,
                               std::vector<int>& valIndices,
                               float validationSplit) {
    int totalSamples = trainingData_.size();
    trainIndices.clear();
    valIndices.clear();

    if (totalSamples == 0) return;

    // 创建索引列表并随机打乱
    std::vector<int> allIndices(totalSamples);
    std::iota(allIndices.begin(), allIndices.end(), 0);
    std::shuffle(allIndices.begin(), allIndices.end(), rng_);

    int valSize = std::max(1, static_cast<int>(totalSamples * validationSplit));
    int trainSize = totalSamples - valSize;

    trainIndices.assign(allIndices.begin(), allIndices.begin() + trainSize);
    valIndices.assign(allIndices.begin() + trainSize, allIndices.end());
}

// =============================================================================
// 简单线性回归模型训练
// =============================================================================
bool AIModelTrainer::trainSimpleLinearModel(const TrainingConfig& config) {
    if (trainingData_.isEmpty()) {
        DBG("No training data available");
        return false;
    }

    int totalSamples = trainingData_.size();
    DBG("Starting training with " << totalSamples << " samples, "
        << config.epochs << " epochs, batch size " << config.batchSize
        << ", lr=" << config.learningRate);

    // 分割训练集和验证集
    std::vector<int> trainIndices, valIndices;
    splitData(trainIndices, valIndices, config.validationSplit);

    DBG("Train set: " << trainIndices.size() << " samples, Val set: " << valIndices.size() << " samples");

    // 初始化模型参数 (Xavier初始化)
    float scale = std::sqrt(2.0f / static_cast<float>(kFeatureDim));
    std::normal_distribution<float> initDist(0.0f, scale);
    for (int i = 0; i < kFeatureDim; ++i) {
        b_[i] = 0.0f;
        for (int j = 0; j < kFeatureDim; ++j) {
            W_[i][j] = initDist(rng_);
        }
    }

    // 预计算所有训练样本的特征向量 (避免重复计算)
    std::vector<std::vector<float>> trainFeatures;
    std::vector<std::vector<float>> trainTargets;
    trainFeatures.reserve(trainIndices.size());
    trainTargets.reserve(trainIndices.size());

    for (int idx : trainIndices) {
        trainFeatures.push_back(extractTextFeatures(trainingData_.textPrompts[idx]));
        trainTargets.push_back(trainingData_.parameterVectors[idx]);
    }

    // 预计算验证集特征
    std::vector<std::vector<float>> valFeatures;
    std::vector<std::vector<float>> valTargets;
    valFeatures.reserve(valIndices.size());
    valTargets.reserve(valIndices.size());

    for (int idx : valIndices) {
        valFeatures.push_back(extractTextFeatures(trainingData_.textPrompts[idx]));
        valTargets.push_back(trainingData_.parameterVectors[idx]);
    }

    // 训练循环
    float bestValLoss = std::numeric_limits<float>::max();
    int patienceCounter = 0;
    int trainSetSize = static_cast<int>(trainFeatures.size());

    for (int epoch = 0; epoch < config.epochs; ++epoch) {
        // 每个epoch开始时打乱训练数据顺序
        std::vector<int> shuffleOrder(trainSetSize);
        std::iota(shuffleOrder.begin(), shuffleOrder.end(), 0);
        std::shuffle(shuffleOrder.begin(), shuffleOrder.end(), rng_);

        // Mini-batch SGD
        float epochTrainLoss = 0.0f;
        int numBatches = 0;

        for (int batchStart = 0; batchStart < trainSetSize; batchStart += config.batchSize) {
            int batchEnd = std::min(batchStart + config.batchSize, trainSetSize);
            float batchLoss = 0.0f;

            for (int bi = batchStart; bi < batchEnd; ++bi) {
                int sampleIdx = shuffleOrder[bi];
                const auto& features = trainFeatures[sampleIdx];
                const auto& target = trainTargets[sampleIdx];

                // 单样本SGD
                sgdStep(features, target, config.learningRate);

                // 累积损失
                auto pred = forwardPass(features);
                for (int d = 0; d < kFeatureDim; ++d) {
                    float diff = pred[d] - target[d];
                    batchLoss += diff * diff;
                }
            }

            epochTrainLoss += batchLoss / static_cast<float>(batchEnd - batchStart);
            ++numBatches;
        }

        epochTrainLoss /= static_cast<float>(std::max(1, numBatches));

        // 验证集评估
        float valLoss = 0.0f;
        if (!valFeatures.empty()) {
            for (size_t vi = 0; vi < valFeatures.size(); ++vi) {
                auto pred = forwardPass(valFeatures[vi]);
                for (int d = 0; d < kFeatureDim; ++d) {
                    float diff = pred[d] - valTargets[vi][d];
                    valLoss += diff * diff;
                }
            }
            valLoss /= static_cast<float>(valFeatures.size() * kFeatureDim);
        }

        // 更新进度
        trainingProgress_ = static_cast<float>(epoch + 1) / static_cast<float>(config.epochs);
        totalEpochs_ = epoch + 1;

        // 每10轮或最后一轮打印日志
        if ((epoch + 1) % 10 == 0 || epoch == 0 || epoch == config.epochs - 1) {
            DBG("Epoch " << (epoch + 1) << "/" << config.epochs
                << " | Train Loss: " << juce::String(epochTrainLoss, 6)
                << " | Val Loss: " << juce::String(valLoss, 6));
        }

        // 早停检查
        if (valLoss < bestValLoss) {
            bestValLoss = valLoss;
            patienceCounter = 0;
        } else {
            ++patienceCounter;
            if (patienceCounter >= config.earlyStoppingPatience) {
                DBG("Early stopping at epoch " << (epoch + 1) << " (patience=" << config.earlyStoppingPatience << ")");
                break;
            }
        }
    }

    modelTrained_ = true;
    trainingProgress_ = 1.0f;

    // 训练完成后评估
    currentMetrics_ = evaluateModel();

    DBG("Training complete. MSE=" << juce::String(currentMetrics_.mse, 6)
        << " MAE=" << juce::String(currentMetrics_.mae, 6)
        << " R²=" << juce::String(currentMetrics_.r2, 6));

    return true;
}

// =============================================================================
// trainModel (包装器，调用简单线性模型训练)
// =============================================================================
bool AIModelTrainer::trainModel(const TrainingConfig& config) {
    return trainSimpleLinearModel(config);
}

// =============================================================================
// 模型评估
// =============================================================================
ModelMetrics AIModelTrainer::evaluateModel() {
    if (!modelTrained_ || trainingData_.isEmpty()) {
        return ModelMetrics{};
    }

    int totalSamples = trainingData_.size();
    float totalMSE = 0.0f;
    float totalMAE = 0.0f;
    float targetMean = 0.0f;
    int totalElements = 0;

    // 计算所有目标值的均值 (用于R²)
    for (int si = 0; si < totalSamples; ++si) {
        for (int d = 0; d < kFeatureDim; ++d) {
            targetMean += trainingData_.parameterVectors[si][d];
        }
    }
    targetMean /= static_cast<float>(totalSamples * kFeatureDim);

    // 计算各项指标
    float ssRes = 0.0f; // 残差平方和
    float ssTot = 0.0f; // 总平方和

    for (int si = 0; si < totalSamples; ++si) {
        auto features = extractTextFeatures(trainingData_.textPrompts[si]);
        auto pred = forwardPass(features);

        for (int d = 0; d < kFeatureDim; ++d) {
            float diff = pred[d] - trainingData_.parameterVectors[si][d];
            totalMSE += diff * diff;
            totalMAE += std::abs(diff);
            ssRes += diff * diff;

            float targetDiff = trainingData_.parameterVectors[si][d] - targetMean;
            ssTot += targetDiff * targetDiff;

            ++totalElements;
        }
    }

    ModelMetrics metrics;
    if (totalElements > 0) {
        metrics.mse = totalMSE / static_cast<float>(totalElements);
        metrics.mae = totalMAE / static_cast<float>(totalElements);
        metrics.rmse = std::sqrt(metrics.mse);
        metrics.r2 = (ssTot > 1e-10f) ? (1.0f - ssRes / ssTot) : 0.0f;
    }

    currentMetrics_ = metrics;
    return metrics;
}

// =============================================================================
// 预测
// =============================================================================
std::vector<float> AIModelTrainer::predict(const std::string& text) {
    if (!modelTrained_) {
        DBG("Model not trained, returning zero vector");
        return std::vector<float>(kFeatureDim, 0.0f);
    }

    auto features = extractTextFeatures(text);
    return forwardPass(features);
}

// =============================================================================
// ONNX Protobuf编码辅助函数
// =============================================================================
// Protobuf wire types:
//   VARINT = 0
//   LENGTH_DELIMITED = 2 (for strings, bytes, embedded messages, packed repeated)
//   I32 = 5 (fixed 32-bit, not used here)
// Tag = (field_number << 3) | wire_type

void AIModelTrainer::writeVarint(std::vector<uint8_t>& buf, uint64_t value) {
    while (value >= 0x80) {
        buf.push_back(static_cast<uint8_t>((value & 0x7F) | 0x80));
        value >>= 7;
    }
    buf.push_back(static_cast<uint8_t>(value & 0x7F));
}

void AIModelTrainer::writeTag(std::vector<uint8_t>& buf, int fieldNum, int wireType) {
    writeVarint(buf, (static_cast<uint64_t>(fieldNum) << 3) | static_cast<uint64_t>(wireType));
}

void AIModelTrainer::writeLengthDelimited(std::vector<uint8_t>& buf, int fieldNum,
                                          const std::vector<uint8_t>& data) {
    writeTag(buf, fieldNum, 2); // LENGTH_DELIMITED
    writeVarint(buf, data.size());
    buf.insert(buf.end(), data.begin(), data.end());
}

void AIModelTrainer::writeString(std::vector<uint8_t>& buf, int fieldNum,
                                 const std::string& str) {
    std::vector<uint8_t> data(str.begin(), str.end());
    writeLengthDelimited(buf, fieldNum, data);
}

void AIModelTrainer::writeInt32(std::vector<uint8_t>& buf, int fieldNum, int32_t value) {
    writeTag(buf, fieldNum, 0); // VARINT
    // Protobuf varint编码：有符号int32使用zigzag编码后varint
    uint64_t encoded = static_cast<uint64_t>(static_cast<uint32_t>(value));
    writeVarint(buf, encoded);
}

void AIModelTrainer::writeInt64(std::vector<uint8_t>& buf, int fieldNum, int64_t value) {
    writeTag(buf, fieldNum, 0); // VARINT
    writeVarint(buf, static_cast<uint64_t>(value));
}

void AIModelTrainer::writeFloat(std::vector<uint8_t>& buf, int fieldNum, float value) {
    writeTag(buf, fieldNum, 5); // I32 (fixed 32-bit)
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    buf.push_back(static_cast<uint8_t>(bits & 0xFF));
    buf.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((bits >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((bits >> 24) & 0xFF));
}

void AIModelTrainer::writeFloatRepeated(std::vector<uint8_t>& buf, int fieldNum,
                                        const std::vector<float>& values) {
    // Packed repeated float (使用length-delimited)
    std::vector<uint8_t> packed;
    packed.reserve(values.size() * 4);
    for (float v : values) {
        uint32_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        packed.push_back(static_cast<uint8_t>(bits & 0xFF));
        packed.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
        packed.push_back(static_cast<uint8_t>((bits >> 16) & 0xFF));
        packed.push_back(static_cast<uint8_t>((bits >> 24) & 0xFF));
    }
    writeLengthDelimited(buf, fieldNum, packed);
}

void AIModelTrainer::writeInt64Repeated(std::vector<uint8_t>& buf, int fieldNum,
                                        const std::vector<int64_t>& values) {
    // Packed repeated int64 (使用length-delimited)
    std::vector<uint8_t> packed;
    for (int64_t v : values) {
        // 写入varint编码的int64
        uint64_t uv = static_cast<uint64_t>(v);
        while (uv >= 0x80) {
            packed.push_back(static_cast<uint8_t>((uv & 0x7F) | 0x80));
            uv >>= 7;
        }
        packed.push_back(static_cast<uint8_t>(uv & 0x7F));
    }
    writeLengthDelimited(buf, fieldNum, packed);
}

// =============================================================================
// 构建ONNX ModelProto
// =============================================================================
// ONNX IR版本7的ModelProto结构:
// ModelProto {
//   ir_version: 7                    (field 1)
//   producer_name: "LianCore"        (field 2)
//   graph: GraphProto {              (field 7)
//     name: "LianCore_LinearModel"   (field 1)
//     input: [ValueInfoProto]        (field 11)
//     output: [ValueInfoProto]       (field 12)
//     initializer: [TensorProto]     (field 5)
//     node: [NodeProto]              (field 8)
//   }
//   opset_import: [OperatorSetIdProto] (field 8)
// }
//
// GraphProto.node[0] = NodeProto {
//   input: ["text_features", "W", "b"]  (field 1)
//   output: ["parameters"]              (field 2)
//   op_type: "Gemm"                     (field 4)
//   attribute: [                        (field 5)
//     AttributeProto { name: "alpha" f: 1.0 },
//     AttributeProto { name: "beta" f: 0.0 },
//     AttributeProto { name: "transB" i: 0 }
//   ]
// }
// =============================================================================
std::vector<uint8_t> AIModelTrainer::buildONNXModelProto() {
    if (!modelTrained_) {
        DBG("Cannot build ONNX model: model not trained");
        return {};
    }

    std::vector<uint8_t> modelProto;

    // === ModelProto ===

    // field 1: ir_version = 7
    writeInt64(modelProto, 1, 7);

    // field 2: producer_name = "LianCore"
    writeString(modelProto, 2, kModelProducer);

    // field 8: opset_import (repeated)
    {
        std::vector<uint8_t> opsetMsg;
        // OperatorSetIdProto.domain = "" (field 1, 省略表示空字符串)
        // OperatorSetIdProto.version = 13 (field 2)
        writeInt64(opsetMsg, 2, 13);

        // 写入repeated opset_import
        writeLengthDelimited(modelProto, 8, opsetMsg);
    }

    // field 7: graph (GraphProto)
    {
        std::vector<uint8_t> graphProto;

        // GraphProto.name = "LianCore_LinearModel" (field 1)
        writeString(graphProto, 1, "LianCore_LinearModel");

        // === GraphProto.input (field 11): ValueInfoProto ===
        {
            std::vector<uint8_t> inputVI;
            // ValueInfoProto.name = "text_features" (field 1)
            writeString(inputVI, 1, kInputName);
            // ValueInfoProto.type (field 2): TypeProto
            {
                std::vector<uint8_t> typeProto;
                // TypeProto.tensor_type (field 1): TypeProto.Tensor
                {
                    std::vector<uint8_t> tensorType;
                    // TensorShapeProto.elem_type = FLOAT = 1 (field 1)
                    writeInt32(tensorType, 1, 1); // FLOAT
                    // TensorShapeProto.shape (field 2): TensorShapeProto
                    {
                        std::vector<uint8_t> shapeProto;
                        // TensorShapeProto.dim (field 1): repeated Dimension
                        // dim[0] = batch_size (动态, 不设置dim_value → dim_param="batch")
                        {
                            std::vector<uint8_t> dimMsg;
                            writeString(dimMsg, 1, "batch_size");
                            writeLengthDelimited(shapeProto, 1, dimMsg);
                        }
                        // dim[1] = 128
                        {
                            std::vector<uint8_t> dimMsg;
                            writeInt64(dimMsg, 2, 128); // dim_value
                            writeLengthDelimited(shapeProto, 1, dimMsg);
                        }
                        writeLengthDelimited(tensorType, 2, shapeProto);
                    }
                    writeLengthDelimited(typeProto, 1, tensorType);
                }
                writeLengthDelimited(inputVI, 2, typeProto);
            }
            writeLengthDelimited(graphProto, 11, inputVI);
        }

        // === GraphProto.output (field 12): ValueInfoProto ===
        {
            std::vector<uint8_t> outputVI;
            writeString(outputVI, 1, kOutputName);
            {
                std::vector<uint8_t> typeProto;
                {
                    std::vector<uint8_t> tensorType;
                    writeInt32(tensorType, 1, 1); // FLOAT
                    {
                        std::vector<uint8_t> shapeProto;
                        {
                            std::vector<uint8_t> dimMsg;
                            writeString(dimMsg, 1, "batch_size");
                            writeLengthDelimited(shapeProto, 1, dimMsg);
                        }
                        {
                            std::vector<uint8_t> dimMsg;
                            writeInt64(dimMsg, 2, 128);
                            writeLengthDelimited(shapeProto, 1, dimMsg);
                        }
                        writeLengthDelimited(tensorType, 2, shapeProto);
                    }
                    writeLengthDelimited(typeProto, 1, tensorType);
                }
                writeLengthDelimited(outputVI, 2, typeProto);
            }
            writeLengthDelimited(graphProto, 12, outputVI);
        }

        // === GraphProto.initializer (field 5): repeated TensorProto ===
        // initializer[0]: W (128×128 float矩阵)
        {
            std::vector<uint8_t> tensorW;
            // TensorProto.name = "W" (field 1)
            writeString(tensorW, 1, kWeightName);
            // TensorProto.data_type = FLOAT = 1 (field 2)
            writeInt32(tensorW, 2, 1);
            // TensorProto.dims = [128, 128] (field 3, packed repeated)
            writeInt64Repeated(tensorW, 3, {128, 128});

            // TensorProto.raw_data (field 4): 原始float数据
            std::vector<uint8_t> rawData;
            rawData.reserve(kFeatureDim * kFeatureDim * 4);
            for (int i = 0; i < kFeatureDim; ++i) {
                for (int j = 0; j < kFeatureDim; ++j) {
                    float val = W_[i][j];
                    uint32_t bits;
                    std::memcpy(&bits, &val, sizeof(bits));
                    rawData.push_back(static_cast<uint8_t>(bits & 0xFF));
                    rawData.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
                    rawData.push_back(static_cast<uint8_t>((bits >> 16) & 0xFF));
                    rawData.push_back(static_cast<uint8_t>((bits >> 24) & 0xFF));
                }
            }
            writeLengthDelimited(tensorW, 4, rawData);

            writeLengthDelimited(graphProto, 5, tensorW);
        }

        // initializer[1]: b (128维float偏置)
        {
            std::vector<uint8_t> tensorB;
            writeString(tensorB, 1, kBiasName);
            writeInt32(tensorB, 2, 1); // FLOAT
            writeInt64Repeated(tensorB, 3, {128});

            std::vector<uint8_t> rawData;
            rawData.reserve(kFeatureDim * 4);
            for (int i = 0; i < kFeatureDim; ++i) {
                float val = b_[i];
                uint32_t bits;
                std::memcpy(&bits, &val, sizeof(bits));
                rawData.push_back(static_cast<uint8_t>(bits & 0xFF));
                rawData.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
                rawData.push_back(static_cast<uint8_t>((bits >> 16) & 0xFF));
                rawData.push_back(static_cast<uint8_t>((bits >> 24) & 0xFF));
            }
            writeLengthDelimited(tensorB, 4, rawData);

            writeLengthDelimited(graphProto, 5, tensorB);
        }

        // === GraphProto.node (field 8): repeated NodeProto ===
        // Gemm节点: Y = alpha * A * B + beta * C
        // 其中 A=text_features(1×128), B=W(128×128), C=b(1×128)
        {
            std::vector<uint8_t> nodeProto;

            // NodeProto.input (field 1): repeated string
            writeString(nodeProto, 1, kInputName);   // A
            writeString(nodeProto, 1, kWeightName);  // B
            writeString(nodeProto, 1, kBiasName);    // C

            // NodeProto.output (field 2): repeated string
            writeString(nodeProto, 2, kOutputName);

            // NodeProto.op_type = "Gemm" (field 4)
            writeString(nodeProto, 4, "Gemm");

            // NodeProto.attribute (field 5): repeated AttributeProto
            // alpha = 1.0
            {
                std::vector<uint8_t> attr;
                writeString(attr, 1, "alpha");   // name
                writeFloat(attr, 4, 1.0f);       // f (field 4 = float)
                writeLengthDelimited(nodeProto, 5, attr);
            }
            // beta = 0.0
            {
                std::vector<uint8_t> attr;
                writeString(attr, 1, "beta");
                writeFloat(attr, 4, 0.0f);
                writeLengthDelimited(nodeProto, 5, attr);
            }
            // transB = 0 (不转置)
            {
                std::vector<uint8_t> attr;
                writeString(attr, 1, "transB");
                writeInt64(attr, 2, 0); // i (field 2 = int64)
                writeLengthDelimited(nodeProto, 5, attr);
            }

            writeLengthDelimited(graphProto, 8, nodeProto);
        }

        // 将GraphProto写入ModelProto
        writeLengthDelimited(modelProto, 7, graphProto);
    }

    // field 14: producer_version = "1.0.0"
    writeString(modelProto, 14, "1.0.0");

    return modelProto;
}

// =============================================================================
// 导出ONNX模型文件
// =============================================================================
bool AIModelTrainer::exportONNXModel(const juce::File& outputPath) {
    if (!modelTrained_) {
        DBG("Cannot export ONNX: model not trained");
        return false;
    }

    // 构建ModelProto
    std::vector<uint8_t> modelProtoBytes = buildONNXModelProto();
    if (modelProtoBytes.empty()) {
        DBG("Failed to build ONNX model proto");
        return false;
    }

    // 确保输出目录存在
    auto parentDir = outputPath.getParentDirectory();
    if (!parentDir.exists()) {
        parentDir.createDirectory();
    }

    // 写入ONNX文件
    // ONNX文件格式: [8字节魔数] [4字节IR版本] [4字节protobuf长度] [protobuf数据]
    // 魔数: "ONNX" + 0x08 (用于版本检测)
    juce::MemoryOutputStream mos(1024 * 1024); // 1MB预分配

    // 魔数: 0x08 + "ONNX" (第一个字节是protobuf版本指示)
    const uint8_t magic[] = {0x08, 'O', 'N', 'N', 'X', '\n'};
    // 注意: 标准ONNX魔数其实是 0x08 0x4F 0x4E 0x4E 0x58 0x0A
    // 验证: 0x08=protobuf varint编码前缀, "ONNX"=标识, 0x0A=换行
    mos.write(magic, sizeof(magic));

    // IR版本 (4字节小端)
    int32_t irVersion = 7;
    mos.write(&irVersion, sizeof(irVersion));

    // Protobuf数据长度 (4字节小端)
    int32_t protoSize = static_cast<int32_t>(modelProtoBytes.size());
    mos.write(&protoSize, sizeof(protoSize));

    // Protobuf数据
    mos.write(modelProtoBytes.data(), modelProtoBytes.size());

    // 写入文件
    juce::FileOutputStream fos(outputPath);
    if (!fos.openedOk()) {
        DBG("Failed to open ONNX output file: " << outputPath.getFullPathName());
        return false;
    }

    fos.write(mos.getData(), mos.getDataSize());
    fos.flush();

    DBG("ONNX model exported to: " << outputPath.getFullPathName()
        << " (" << mos.getDataSize() / 1024 << "KB)");
    return true;
}

} // namespace LianCore