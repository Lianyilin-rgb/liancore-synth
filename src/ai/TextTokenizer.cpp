// =============================================================================
// LianCore Gamma - TextTokenizer 实现 (升级为 BPE 子词级)
// =============================================================================
#include "TextTokenizer.h"
#include "TransformerTextEncoder.h"

namespace LianCore {

TextTokenizer::TextTokenizer()
    : bpeTokenizer_(std::make_unique<BPETokenizer>()) {
}

TextTokenizer::~TextTokenizer() = default;

bool TextTokenizer::loadModel(const juce::File& modelPath) {
    bpeLoaded_ = bpeTokenizer_->loadModel(modelPath);
    if (bpeLoaded_) {
        DBG("TextTokenizer: BPE model loaded, vocab_size=" << bpeTokenizer_->getVocabSize());
    }
    return bpeLoaded_;
}

// =============================================================================
// 向后兼容 API
// =============================================================================

std::vector<juce::String> TextTokenizer::tokenize(const juce::String& text) {
    if (bpeLoaded_) {
        // 使用 BPE 分词
        auto ids = bpeTokenizer_->encode(text.toStdString(), 64);
        std::vector<juce::String> result;
        for (auto id : ids) {
            result.push_back(juce::String(bpeTokenizer_->getPiece(static_cast<int>(id))));
        }
        return result;
    }

    // 回退: 旧版空格分词
    juce::StringArray tokens;
    tokens.addTokens(text, " ,.;:!?()[]{}\"'", "\"");
    std::vector<juce::String> result;
    for (const auto& token : tokens) {
        result.push_back(token);
    }
    return result;
}

std::vector<int> TextTokenizer::encode(const juce::String& text) {
    if (bpeLoaded_) {
        // 使用 BPE 编码 (int64 → int, 词表2500在int范围内)
        auto ids = bpeTokenizer_->encode(text.toStdString(), 64);
        std::vector<int> result;
        result.reserve(ids.size());
        for (auto id : ids) {
            result.push_back(static_cast<int>(id));
        }
        return result;
    }

    // 回退: 旧版哈希编码
    auto tokens = tokenize(text);
    std::vector<int> encoded;
    encoded.reserve(tokens.size());
    for (const auto& token : tokens) {
        encoded.push_back(token.hashCode());
    }
    return encoded;
}

juce::String TextTokenizer::decode(const std::vector<int>& tokens) {
    if (bpeLoaded_) {
        std::vector<int64_t> ids;
        for (auto t : tokens) ids.push_back(t);
        return juce::String(bpeTokenizer_->decode(ids));
    }
    return "[BPE not loaded]";
}

// =============================================================================
// Gamma 新增: BPE 原生接口
// =============================================================================

std::vector<int64_t> TextTokenizer::encodeBPE(const std::string& text, int maxLen) {
    if (bpeLoaded_) {
        return bpeTokenizer_->encode(text, maxLen);
    }
    return {};
}

std::string TextTokenizer::decodeBPE(const std::vector<int64_t>& ids) {
    if (bpeLoaded_) {
        return bpeTokenizer_->decode(ids);
    }
    return "";
}

int TextTokenizer::getVocabSize() const {
    return bpeTokenizer_->getVocabSize();
}

int TextTokenizer::getId(const std::string& piece) const {
    return bpeTokenizer_->getId(piece);
}

std::string TextTokenizer::getPiece(int id) const {
    return bpeTokenizer_->getPiece(id);
}

bool TextTokenizer::isBPELoaded() const {
    return bpeLoaded_;
}

} // namespace LianCore