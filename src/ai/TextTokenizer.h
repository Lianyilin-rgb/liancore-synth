// =============================================================================
// LianCore Gamma - TextTokenizer 文本分词器 (升级为 BPE 子词级)
// 内部使用 BPETokenizer 实现, 向下兼容旧API
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <string>
#include <memory>

namespace LianCore {

// 前向声明
class BPETokenizer;

class TextTokenizer {
public:
    TextTokenizer();
    ~TextTokenizer();

    // 加载 BPE 模型
    bool loadModel(const juce::File& modelPath);

    // 向后兼容 API (Alpha阶段接口)
    std::vector<juce::String> tokenize(const juce::String& text);
    std::vector<int> encode(const juce::String& text);
    juce::String decode(const std::vector<int>& tokens);

    // Gamma 新增: BPE 原生接口
    std::vector<int64_t> encodeBPE(const std::string& text, int maxLen = 64);
    std::string decodeBPE(const std::vector<int64_t>& ids);
    int getVocabSize() const;
    int getId(const std::string& piece) const;
    std::string getPiece(int id) const;
    bool isBPELoaded() const;

private:
    std::unique_ptr<BPETokenizer> bpeTokenizer_;
    bool bpeLoaded_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TextTokenizer)
};

} // namespace LianCore