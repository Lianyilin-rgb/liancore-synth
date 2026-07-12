// =============================================================================
// LianCore Gamma - TransformerTextEncoder
// 基于 BPE Tokenizer + 2层 Transformer ONNX 模型的文本编码器
// 输出: 128维文本嵌入向量
// 推理延迟: <1ms (CPU)
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

// ONNX Runtime C++ API
#ifdef LIANCORE_HAS_ONNX
#include <onnxruntime_cxx_api.h>
#endif

namespace LianCore {

// =============================================================================
// BPETokenizer - 纯 C++ 实现的 BPE 子词分词器
// 从 SentencePiece 训练的 vocab 文件加载词表
// 使用贪心最长匹配算法进行分词
// =============================================================================
class BPETokenizer {
public:
    static constexpr int kPadId = 0;
    static constexpr int kUnkId = 1;
    static constexpr int kBosId = 2;
    static constexpr int kEosId = 3;
    static constexpr int kMaxLen = 64;

    BPETokenizer() = default;
    ~BPETokenizer() = default;

    // 从 vocab 文件加载词表 (格式: token\tscore)
    bool loadVocab(const juce::File& vocabFile);

    // 从 tokenizer.model 文件加载 (SentencePiece protobuf 格式)
    bool loadModel(const juce::File& modelFile);

    // 分词: 文本 → token ID 列表
    std::vector<int64_t> encode(const std::string& text, int maxLen = kMaxLen) const;

    // 解码: token ID 列表 → 文本
    std::string decode(const std::vector<int64_t>& ids) const;

    // 查询
    int getVocabSize() const { return static_cast<int>(idToPiece_.size()); }
    int getId(const std::string& piece) const;
    std::string getPiece(int id) const;
    bool isLoaded() const { return !pieceToId_.empty(); }

private:
    // 词表映射
    std::unordered_map<std::string, int> pieceToId_;
    std::unordered_map<int, std::string> idToPiece_;

    // 最长匹配分词
    std::vector<std::string> tokenizeToPieces(const std::string& text) const;

    // 加载 SentencePiece protobuf 模型文件
    bool loadSentencePieceModel(const juce::File& modelFile);
};

// =============================================================================
// TransformerTextEncoder - ONNX Transformer 推理封装
// =============================================================================
class TransformerTextEncoder {
public:
    TransformerTextEncoder();
    ~TransformerTextEncoder();

    // 加载模型
    bool loadTokenizer(const juce::File& modelPath);
    bool loadTransformer(const juce::File& onnxPath);
    bool isLoaded() const { return tokenizer_.isLoaded() && transformerLoaded_; }

    // 文本编码: 文本 → 128维嵌入向量
    std::vector<float> encode(const std::string& text);

    // 查询
    int getEmbeddingDim() const { return 128; }
    int getVocabSize() const { return tokenizer_.getVocabSize(); }
    BPETokenizer& getTokenizer() { return tokenizer_; }

private:
    BPETokenizer tokenizer_;
    bool transformerLoaded_ = false;

#ifdef LIANCORE_HAS_ONNX
    std::unique_ptr<Ort::Env> ortEnv_;
    std::unique_ptr<Ort::Session> ortSession_;
    std::unique_ptr<Ort::SessionOptions> ortSessionOptions_;
    std::unique_ptr<Ort::MemoryInfo> ortMemoryInfo_;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransformerTextEncoder)
};

} // namespace LianCore