// =============================================================================
// LianCore Gamma - TransformerTextEncoder 实现
// BPE Tokenizer (纯C++贪心最长匹配) + ONNX Transformer 推理
// =============================================================================
#include "TransformerTextEncoder.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace LianCore {

// =============================================================================
// BPETokenizer 实现
// =============================================================================

bool BPETokenizer::loadVocab(const juce::File& vocabFile) {
    if (!vocabFile.existsAsFile()) {
        DBG("BPETokenizer: vocab file not found: " << vocabFile.getFullPathName());
        return false;
    }

    pieceToId_.clear();
    idToPiece_.clear();

    juce::StringArray lines;
    vocabFile.readLines(lines);

    for (int i = 0; i < lines.size(); ++i) {
        juce::String line = lines[i].trim();
        if (line.isEmpty()) continue;

        // 格式: token\tscore
        int tabPos = line.indexOfChar('\t');
        if (tabPos < 0) continue;

        juce::String piece = line.substring(0, tabPos);
        // 处理 SentencePiece 的元字符 ▁ (U+2581) → 空格
        std::string pieceStr = piece.toStdString();

        // 替换 SentencePiece 空格标记为实际空格
        // (保持原样, SentencePiece 使用 ▁ 表示词首空格)

        pieceToId_[pieceStr] = i;
        idToPiece_[i] = pieceStr;
    }

    DBG("BPETokenizer: loaded " << pieceToId_.size() << " tokens");
    return true;
}

bool BPETokenizer::loadModel(const juce::File& modelFile) {
    // SentencePiece .model 文件是 protobuf 二进制格式
    // 纯 C++ 解析过于复杂，回退到加载 vocab 文件
    juce::File vocabFile = modelFile.getSiblingFile("tokenizer.vocab");
    if (vocabFile.existsAsFile()) {
        return loadVocab(vocabFile);
    }
    DBG("BPETokenizer: no vocab file found alongside model");
    return false;
}

std::vector<std::string> BPETokenizer::tokenizeToPieces(const std::string& text) const {
    std::vector<std::string> pieces;

    if (text.empty() || pieceToId_.empty()) return pieces;

    // 贪心最长匹配分词
    size_t pos = 0;
    const size_t len = text.size();

    while (pos < len) {
        size_t bestLen = 0;
        std::string bestPiece;

        // 尝试匹配从 pos 开始的最长子串
        for (size_t end = std::min(pos + 16, len); end > pos; --end) {
            std::string candidate = text.substr(pos, end - pos);
            auto it = pieceToId_.find(candidate);
            if (it != pieceToId_.end()) {
                bestLen = end - pos;
                bestPiece = candidate;
                break;
            }
        }

        if (bestLen > 0) {
            pieces.push_back(bestPiece);
            pos += bestLen;
        } else {
            // 未匹配: 使用单字符 (回退到 [UNK] 或逐字处理)
            std::string singleChar = text.substr(pos, 1);
            auto it = pieceToId_.find(singleChar);
            if (it != pieceToId_.end()) {
                pieces.push_back(singleChar);
            } else {
                pieces.push_back("[UNK]");
            }
            pos += 1;
        }
    }

    return pieces;
}

std::vector<int64_t> BPETokenizer::encode(const std::string& text, int maxLen) const {
    std::vector<int64_t> ids;

    if (!isLoaded()) return ids;

    // 预处理: 添加 SentencePiece 风格的词首空格标记
    // SentencePiece 默认在文本前添加空格
    std::string processed = " " + text;

    auto pieces = tokenizeToPieces(processed);

    // 特殊情况: 如果只匹配到空格，去掉它
    if (!pieces.empty() && (pieces[0] == "▁" || pieces[0] == " ")) {
        pieces.erase(pieces.begin());
    }

    for (const auto& piece : pieces) {
        auto it = pieceToId_.find(piece);
        if (it != pieceToId_.end()) {
            ids.push_back(static_cast<int64_t>(it->second));
        } else {
            ids.push_back(kUnkId);
        }
    }

    // 截断到 maxLen
    if (static_cast<int>(ids.size()) > maxLen) {
        ids.resize(maxLen);
    }

    return ids;
}

std::string BPETokenizer::decode(const std::vector<int64_t>& ids) const {
    std::string result;
    for (auto id : ids) {
        auto it = idToPiece_.find(static_cast<int>(id));
        if (it != idToPiece_.end()) {
            std::string piece = it->second;
            // 替换 SentencePiece 空格标记
            if (piece.size() >= 3 && static_cast<unsigned char>(piece[0]) == 0xE2 &&
                static_cast<unsigned char>(piece[1]) == 0x96 && static_cast<unsigned char>(piece[2]) == 0x81) {
                result += " ";
            } else {
                result += piece;
            }
        }
    }
    return result;
}

int BPETokenizer::getId(const std::string& piece) const {
    auto it = pieceToId_.find(piece);
    return (it != pieceToId_.end()) ? it->second : kUnkId;
}

std::string BPETokenizer::getPiece(int id) const {
    auto it = idToPiece_.find(id);
    return (it != idToPiece_.end()) ? it->second : "[UNK]";
}

// =============================================================================
// TransformerTextEncoder 实现
// =============================================================================

TransformerTextEncoder::TransformerTextEncoder() {
#ifdef LIANCORE_HAS_ONNX
    try {
        ortEnv_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "LianCoreTransformer");
        ortSessionOptions_ = std::make_unique<Ort::SessionOptions>();
        ortSessionOptions_->SetIntraOpNumThreads(1);
        ortSessionOptions_->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
        ortMemoryInfo_ = std::make_unique<Ort::MemoryInfo>(
            Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault));
    } catch (const std::exception& e) {
        DBG("TransformerTextEncoder: ONNX init failed: " << e.what());
    }
#endif
}

TransformerTextEncoder::~TransformerTextEncoder() = default;

bool TransformerTextEncoder::loadTokenizer(const juce::File& modelPath) {
    return tokenizer_.loadModel(modelPath);
}

bool TransformerTextEncoder::loadTransformer(const juce::File& onnxPath) {
#ifdef LIANCORE_HAS_ONNX
    if (!onnxPath.existsAsFile()) {
        DBG("TransformerTextEncoder: ONNX model not found: " << onnxPath.getFullPathName());
        return false;
    }

    try {
        ortSession_ = std::make_unique<Ort::Session>(
            *ortEnv_, onnxPath.getFullPathName().toWideCharPointer(), *ortSessionOptions_);
        transformerLoaded_ = true;
        DBG("TransformerTextEncoder: ONNX model loaded: " << onnxPath.getFullPathName());
        return true;
    } catch (const std::exception& e) {
        DBG("TransformerTextEncoder: ONNX load failed: " << e.what());
        transformerLoaded_ = false;
        return false;
    }
#else
    juce::ignoreUnused(onnxPath);
    DBG("TransformerTextEncoder: ONNX Runtime not compiled");
    return false;
#endif
}

std::vector<float> TransformerTextEncoder::encode(const std::string& text) {
    std::vector<float> embedding(128, 0.0f);

    if (!isLoaded()) {
        return embedding;
    }

    // 1. BPE 分词
    auto tokenIds = tokenizer_.encode(text, BPETokenizer::kMaxLen);
    if (tokenIds.empty()) {
        return embedding;
    }

#ifdef LIANCORE_HAS_ONNX
    try {
        // 2. 准备 ONNX 输入张量 [1, seq_len]
        std::vector<int64_t> inputShape = {1, static_cast<int64_t>(tokenIds.size())};

        Ort::Value inputTensor = Ort::Value::CreateTensor<int64_t>(
            *ortMemoryInfo_,
            tokenIds.data(),
            tokenIds.size(),
            inputShape.data(),
            inputShape.size()
        );

        // 3. 运行推理
        const char* inputNames[] = {"input_ids"};
        const char* outputNames[] = {"embedding"};

        auto outputTensors = ortSession_->Run(
            Ort::RunOptions{nullptr},
            inputNames,
            &inputTensor,
            1,
            outputNames,
            1
        );

        // 4. 提取输出 [1, 128]
        if (!outputTensors.empty()) {
            float* outputData = outputTensors[0].GetTensorMutableData<float>();
            auto outputShape = outputTensors[0].GetTensorTypeAndShapeInfo().GetShape();

            size_t outputSize = 1;
            for (auto dim : outputShape) {
                if (dim > 0) outputSize *= static_cast<size_t>(dim);
            }

            if (outputSize >= 128) {
                embedding.assign(outputData, outputData + 128);
            }
        }
    } catch (const std::exception& e) {
        DBG("TransformerTextEncoder: inference failed: " << e.what());
    }
#else
    juce::ignoreUnused(tokenIds);
#endif

    return embedding;
}

} // namespace LianCore