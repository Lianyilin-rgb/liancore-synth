// =============================================================================
// LianCore - EffectsChainUI 效果链预设UI组件 (P6-3)
// 提供12个效果器的启用/禁用、干湿比、顺序、路由模式、预设保存/加载界面
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include "../params/EffectsChainPreset.h"

namespace LianCore {

// 12个效果器名称
static const char* kEffectNames[] = {
    "Reverb", "Delay", "Chorus", "Flanger", "Phaser",
    "Distortion", "EQ", "Compressor", "Filter", "Bitcrusher",
    "Ring Mod", "Tremolo"
};

// =============================================================================
// EffectsChainUI - 效果链预设主界面
// =============================================================================
class EffectsChainUI : public juce::Component,
                        public juce::Slider::Listener,
                        public juce::ComboBox::Listener {
public:
    using ManagerRef = std::function<EffectsChainPresetManager*()>;
    using PresetRef = std::function<EffectsChainPreset*()>;
    using SetPresetFn = std::function<void(const EffectsChainPreset&)>;

    EffectsChainUI();
    ~EffectsChainUI() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void sliderValueChanged(juce::Slider* slider) override;
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;

    void setManagerRef(ManagerRef ref) { managerRef_ = std::move(ref); }
    void setPresetRef(PresetRef ref) { presetRef_ = std::move(ref); }
    void setSetPresetFn(SetPresetFn fn) { setPresetFn_ = std::move(fn); }

    void refreshUI();

private:
    void onSavePreset();
    void onLoadPreset();
    void onDeletePreset();
    void onExportPreset();
    void onImportPreset();
    void refreshPresetList();
    void loadPresetToUI();

    ManagerRef managerRef_;
    PresetRef presetRef_;
    SetPresetFn setPresetFn_;

    // 预设名称输入
    juce::Label presetNameLabel_;
    juce::TextEditor presetNameInput_;

    // 预设列表
    juce::ComboBox presetListCombo_;
    juce::Label presetListLabel_;

    // 预设操作按钮
    juce::TextButton saveButton_;
    juce::TextButton loadButton_;
    juce::TextButton deleteButton_;
    juce::TextButton exportButton_;
    juce::TextButton importButton_;

    // 路由模式选择
    juce::ComboBox routingModeCombo_;
    juce::Label routingModeLabel_;

    // 输出增益
    juce::Slider outputGainSlider_;
    juce::Label outputGainLabel_;

    // 效果器列表 (12个)
    struct EffectRow {
        juce::ToggleButton enabledToggle;
        juce::Slider wetDrySlider;
        juce::Label nameLabel;
    };
    std::vector<std::unique_ptr<EffectRow>> effectRows_;

    // 分类标签
    juce::Label categoryLabel_;
    juce::TextEditor categoryInput_;

    // 状态标签
    juce::Label statusLabel_;

    // 颜色配置
    juce::Colour bgColor_     = juce::Colour(0xff0a0a0f);
    juce::Colour panelColor_  = juce::Colour(0xff1a1a2e);
    juce::Colour accentColor_ = juce::Colour(0xff00d4ff);
    juce::Colour textColor_   = juce::Colour(0xffe0e0e0);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectsChainUI)
};

} // namespace LianCore