// =============================================================================
// LianCore - TextTokenizer 实现
// =============================================================================
#include "TextTokenizer.h"

namespace LianCore {

TextTokenizer::TextTokenizer() = default;

std::vector<juce::String> TextTokenizer::tokenize(const juce::String& text) {
    juce::StringArray tokens;
    tokens.addTokens(text, " ,.;:!?()[]{}\"'", "\"");
    std::vector<juce::String> result;
    for (const auto& token : tokens) {
        result.push_back(token);
    }
    return result;
}

std::vector<int> TextTokenizer::encode(const juce::String& text) {
    auto tokens = tokenize(text);
    std::vector<int> encoded;
    encoded.reserve(tokens.size());
    for (const auto& token : tokens) {
        encoded.push_back(token.hashCode());
    }
    return encoded;
}

juce::String TextTokenizer::decode(const std::vector<int>& tokens) {
    // Alpha阶段: 简化实现
    juce::ignoreUnused(tokens);
    return "[模拟解码]";
}

} // namespace LianCore