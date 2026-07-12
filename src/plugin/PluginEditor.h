// =============================================================================
// LianCore - PluginEditor VST3插件编辑器 (Alpha阶段: 基础UI)
// Beta阶段将替换为React+WebGL UI
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

namespace LianCore {

class PluginEditor : public juce::AudioProcessorEditor {
public:
    explicit PluginEditor(PluginProcessor& processor);
    ~PluginEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    PluginProcessor& processor_;

    // Alpha阶段基础UI组件
    juce::Label titleLabel_;
    juce::Label statusLabel_;
    juce::Label cpuLabel_;
    juce::TextButton testButton_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace LianCore