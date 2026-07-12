// =============================================================================
// LianCore - TextTokenizer 文本分词器 (Alpha阶段: 占位)
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <string>

namespace LianCore {

class TextTokenizer {
public:
    TextTokenizer();
    ~TextTokenizer() = default;

    std::vector<juce::String> tokenize(const juce::String& text);
    std::vector<int> encode(const juce::String& text);
    juce::String decode(const std::vector<int>& tokens);

private:
    // Alpha阶段: 简单空格分词
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TextTokenizer)
};

} // namespace LianCore