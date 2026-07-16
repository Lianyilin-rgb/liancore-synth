// =============================================================================
// LianCore - EffectsChainUI 实现 (P6-3)
// 效果链预设UI组件，包含12个效果器的控制界面
// =============================================================================
#include "EffectsChainUI.h"

namespace LianCore {

// =============================================================================
EffectsChainUI::EffectsChainUI() {
    // 初始化12个效果器行
    for (int i = 0; i < 12; ++i) {
        auto row = std::make_unique<EffectRow>();

        row->nameLabel.setText(kEffectNames[i], juce::dontSendNotification);
        row->nameLabel.setFont(juce::Font(10.0f, juce::Font::bold));
        row->nameLabel.setColour(juce::Label::textColourId, textColor_);
        row->nameLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(row->nameLabel);

        row->enabledToggle.setButtonText("");
        row->enabledToggle.setColour(juce::ToggleButton::tickColourId, accentColor_);
        addAndMakeVisible(row->enabledToggle);

        row->wetDrySlider.setRange(0.0, 1.0, 0.01);
        row->wetDrySlider.setValue(0.5);
        row->wetDrySlider.setSliderStyle(juce::Slider::LinearHorizontal);
        row->wetDrySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 14);
        row->wetDrySlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff00d4ff));
        row->wetDrySlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff2a2a3a));
        row->wetDrySlider.setColour(juce::Slider::textBoxTextColourId, textColor_);
        row->wetDrySlider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff0a0a0f));
        row->wetDrySlider.addListener(this);
        addAndMakeVisible(row->wetDrySlider);

        effectRows_.push_back(std::move(row));
    }

    // ---- 预设名称 ----
    presetNameLabel_.setText("预设名称", juce::dontSendNotification);
    presetNameLabel_.setFont(juce::Font(10.0f));
    presetNameLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff8888a0));
    addAndMakeVisible(presetNameLabel_);

    presetNameInput_.setText("New Preset");
    presetNameInput_.setFont(juce::Font(11.0f));
    presetNameInput_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1a1a2e));
    presetNameInput_.setColour(juce::TextEditor::textColourId, textColor_);
    presetNameInput_.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2a2a3a));
    addAndMakeVisible(presetNameInput_);

    // ---- 分类 ----
    categoryLabel_.setText("分类", juce::dontSendNotification);
    categoryLabel_.setFont(juce::Font(10.0f));
    categoryLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff8888a0));
    addAndMakeVisible(categoryLabel_);

    categoryInput_.setText("Custom");
    categoryInput_.setFont(juce::Font(11.0f));
    categoryInput_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1a1a2e));
    categoryInput_.setColour(juce::TextEditor::textColourId, textColor_);
    categoryInput_.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2a2a3a));
    addAndMakeVisible(categoryInput_);

    // ---- 预设列表 ----
    presetListLabel_.setText("预设列表", juce::dontSendNotification);
    presetListLabel_.setFont(juce::Font(10.0f));
    presetListLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff8888a0));
    addAndMakeVisible(presetListLabel_);

    presetListCombo_.addListener(this);
    addAndMakeVisible(presetListCombo_);

    // ---- 操作按钮 ----
    saveButton_.setButtonText("保存");
    saveButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a3a));
    saveButton_.onClick = [this]() { onSavePreset(); };
    addAndMakeVisible(saveButton_);

    loadButton_.setButtonText("加载");
    loadButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a3a));
    loadButton_.onClick = [this]() { onLoadPreset(); };
    addAndMakeVisible(loadButton_);

    deleteButton_.setButtonText("删除");
    deleteButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a3a));
    deleteButton_.onClick = [this]() { onDeletePreset(); };
    addAndMakeVisible(deleteButton_);

    exportButton_.setButtonText("导出");
    exportButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a3a));
    exportButton_.onClick = [this]() { onExportPreset(); };
    addAndMakeVisible(exportButton_);

    importButton_.setButtonText("导入");
    importButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a3a));
    importButton_.onClick = [this]() { onImportPreset(); };
    addAndMakeVisible(importButton_);

    // ---- 路由模式 ----
    routingModeLabel_.setText("路由模式", juce::dontSendNotification);
    routingModeLabel_.setFont(juce::Font(10.0f));
    routingModeLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff8888a0));
    addAndMakeVisible(routingModeLabel_);

    routingModeCombo_.addItem("串行", 1);
    routingModeCombo_.addItem("并行", 2);
    routingModeCombo_.addItem("混合", 3);
    routingModeCombo_.setSelectedId(1);
    routingModeCombo_.addListener(this);
    addAndMakeVisible(routingModeCombo_);

    // ---- 输出增益 ----
    outputGainLabel_.setText("输出增益", juce::dontSendNotification);
    outputGainLabel_.setFont(juce::Font(10.0f));
    outputGainLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff8888a0));
    addAndMakeVisible(outputGainLabel_);

    outputGainSlider_.setRange(-24.0, 24.0, 0.1);
    outputGainSlider_.setValue(0.0);
    outputGainSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    outputGainSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 14);
    outputGainSlider_.setColour(juce::Slider::thumbColourId, accentColor_);
    outputGainSlider_.setColour(juce::Slider::trackColourId, juce::Colour(0xff2a2a3a));
    outputGainSlider_.setColour(juce::Slider::textBoxTextColourId, textColor_);
    outputGainSlider_.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff0a0a0f));
    outputGainSlider_.addListener(this);
    addAndMakeVisible(outputGainSlider_);

    // ---- 状态标签 ----
    statusLabel_.setText("就绪", juce::dontSendNotification);
    statusLabel_.setFont(juce::Font(10.0f));
    statusLabel_.setColour(juce::Label::textColourId, accentColor_);
    addAndMakeVisible(statusLabel_);
}

EffectsChainUI::~EffectsChainUI() = default;

void EffectsChainUI::paint(juce::Graphics& g) {
    g.fillAll(bgColor_);
    g.setColour(panelColor_);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 4.0f, 1.0f);
}

void EffectsChainUI::resized() {
    auto area = getLocalBounds().reduced(8);

    // 顶部：预设管理行
    auto presetRow = area.removeFromTop(50);
    {
        auto left = presetRow.removeFromLeft(180);
        presetNameLabel_.setBounds(left.removeFromTop(16));
        presetNameInput_.setBounds(left.removeFromTop(22));

        presetRow.removeFromLeft(4);
        auto mid = presetRow.removeFromLeft(120);
        categoryLabel_.setBounds(mid.removeFromTop(16));
        categoryInput_.setBounds(mid.removeFromTop(22));

        presetRow.removeFromLeft(4);
        auto right = presetRow;
        presetListLabel_.setBounds(right.removeFromTop(16));
        auto btnRow = right.removeFromTop(22);
        int btnW = 42;
        saveButton_.setBounds(btnRow.removeFromLeft(btnW).reduced(1, 2));
        loadButton_.setBounds(btnRow.removeFromLeft(btnW).reduced(1, 2));
        deleteButton_.setBounds(btnRow.removeFromLeft(btnW).reduced(1, 2));
        exportButton_.setBounds(btnRow.removeFromLeft(btnW).reduced(1, 2));
        importButton_.setBounds(btnRow.removeFromLeft(btnW).reduced(1, 2));
    }

    area.removeFromTop(4);

    // 预设列表下拉 + 路由 + 增益
    auto controlRow = area.removeFromTop(24);
    presetListCombo_.setBounds(controlRow.removeFromLeft(140).reduced(2, 2));
    controlRow.removeFromLeft(8);
    routingModeLabel_.setBounds(controlRow.removeFromLeft(55));
    routingModeCombo_.setBounds(controlRow.removeFromLeft(60).reduced(2, 2));
    controlRow.removeFromLeft(8);
    outputGainLabel_.setBounds(controlRow.removeFromLeft(55));
    outputGainSlider_.setBounds(controlRow.removeFromLeft(100).reduced(2, 2));
    statusLabel_.setBounds(controlRow);

    area.removeFromTop(4);

    // 效果器列表 (6列 x 2行)
    int colWidth = area.getWidth() / 6;
    for (int row = 0; row < 2; ++row) {
        auto rowArea = area.removeFromTop(50);
        for (int col = 0; col < 6; ++col) {
            int idx = row * 6 + col;
            if (idx >= 12) break;

            auto cell = rowArea.removeFromLeft(colWidth).reduced(2, 2);
            auto& effect = *effectRows_[idx];

            effect.nameLabel.setBounds(cell.removeFromTop(16));
            auto toggleRow = cell.removeFromTop(16);
            effect.enabledToggle.setBounds(toggleRow.removeFromLeft(20));
            effect.wetDrySlider.setBounds(cell);
        }
    }
}

void EffectsChainUI::sliderValueChanged(juce::Slider* slider) {
    auto* preset = presetRef_ ? presetRef_() : nullptr;
    if (!preset) return;

    if (slider == &outputGainSlider_) {
        preset->outputGain = (float)slider->getValue();
        return;
    }

    for (int i = 0; i < 12; ++i) {
        if (slider == &effectRows_[i]->wetDrySlider) {
            if (i < (int)preset->wetDryMixes.size()) {
                preset->wetDryMixes[i] = (float)slider->getValue();
            }
            break;
        }
    }
}

void EffectsChainUI::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) {
    if (comboBoxThatHasChanged == &routingModeCombo_) {
        auto* preset = presetRef_ ? presetRef_() : nullptr;
        if (preset) {
            preset->routingMode = routingModeCombo_.getSelectedId() - 1;
        }
    }
}

void EffectsChainUI::onSavePreset() {
    auto* mgr = managerRef_ ? managerRef_() : nullptr;
    auto* preset = presetRef_ ? presetRef_() : nullptr;
    if (!mgr || !preset) return;

    // 从UI同步到预设
    preset->name = presetNameInput_.getText().toStdString();
    preset->category = categoryInput_.getText().toStdString();
    preset->routingMode = routingModeCombo_.getSelectedId() - 1;
    preset->outputGain = (float)outputGainSlider_.getValue();

    for (int i = 0; i < 12; ++i) {
        if (i < (int)preset->enabled.size()) {
            preset->enabled[i] = effectRows_[i]->enabledToggle.getToggleState();
        }
        if (i < (int)preset->wetDryMixes.size()) {
            preset->wetDryMixes[i] = (float)effectRows_[i]->wetDrySlider.getValue();
        }
    }

    auto presetDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                         .getChildFile("LianCore").getChildFile("EffectsChainPresets");
    presetDir.createDirectory();

    if (mgr->saveToPresetManager(*preset, presetDir)) {
        statusLabel_.setText("已保存: " + juce::String(preset->name), juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff2ed573));
        refreshPresetList();
    } else {
        statusLabel_.setText("保存失败", juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xffff6b6b));
    }
}

void EffectsChainUI::onLoadPreset() {
    auto* mgr = managerRef_ ? managerRef_() : nullptr;
    auto* preset = presetRef_ ? presetRef_() : nullptr;
    if (!mgr || !preset) return;

    auto selectedName = presetListCombo_.getText().toStdString();
    if (selectedName.empty()) return;

    auto presetDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                         .getChildFile("LianCore").getChildFile("EffectsChainPresets");

    if (mgr->loadFromPresetManager(*preset, selectedName, presetDir)) {
        if (setPresetFn_) setPresetFn_(*preset);
        loadPresetToUI();
        statusLabel_.setText("已加载: " + juce::String(selectedName), juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff2ed573));
    }
}

void EffectsChainUI::onDeletePreset() {
    auto* mgr = managerRef_ ? managerRef_() : nullptr;
    if (!mgr) return;

    auto selectedName = presetListCombo_.getText().toStdString();
    if (selectedName.empty()) return;

    auto presetDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                         .getChildFile("LianCore").getChildFile("EffectsChainPresets");

    if (mgr->deletePreset(selectedName, presetDir)) {
        statusLabel_.setText("已删除: " + juce::String(selectedName), juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, accentColor_);
        refreshPresetList();
    }
}

void EffectsChainUI::onExportPreset() {
    auto* mgr = managerRef_ ? managerRef_() : nullptr;
    auto* preset = presetRef_ ? presetRef_() : nullptr;
    if (!mgr || !preset) return;

    auto chooser = std::make_unique<juce::FileChooser>(
        "导出效果链预设...",
        juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile(preset->name + ".fxchain"),
        "*.fxchain");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this, preset](const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file != juce::File()) {
                auto* mgr = managerRef_ ? managerRef_() : nullptr;
                if (mgr && mgr->savePreset(*preset, file)) {
                    statusLabel_.setText("已导出: " + file.getFileName(), juce::dontSendNotification);
                    statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff2ed573));
                }
            }
        });
}

void EffectsChainUI::onImportPreset() {
    auto* mgr = managerRef_ ? managerRef_() : nullptr;
    auto* preset = presetRef_ ? presetRef_() : nullptr;
    if (!mgr || !preset) return;

    auto chooser = std::make_unique<juce::FileChooser>(
        "导入效果链预设...",
        juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
        "*.fxchain");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, preset](const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file != juce::File()) {
                auto* mgr = managerRef_ ? managerRef_() : nullptr;
                if (mgr && mgr->loadPreset(*preset, file)) {
                    if (setPresetFn_) setPresetFn_(*preset);
                    loadPresetToUI();
                    statusLabel_.setText("已导入: " + file.getFileName(), juce::dontSendNotification);
                    statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff2ed573));
                }
            }
        });
}

void EffectsChainUI::refreshPresetList() {
    auto* mgr = managerRef_ ? managerRef_() : nullptr;
    if (!mgr) return;

    auto presetDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                         .getChildFile("LianCore").getChildFile("EffectsChainPresets");

    auto names = mgr->listPresets(presetDir);
    presetListCombo_.clear();
    int id = 1;
    for (auto& name : names) {
        presetListCombo_.addItem(juce::String(name), id++);
    }
}

void EffectsChainUI::loadPresetToUI() {
    auto* preset = presetRef_ ? presetRef_() : nullptr;
    if (!preset) return;

    presetNameInput_.setText(juce::String(preset->name));
    categoryInput_.setText(juce::String(preset->category));
    routingModeCombo_.setSelectedId(preset->routingMode + 1, juce::dontSendNotification);
    outputGainSlider_.setValue(preset->outputGain, juce::dontSendNotification);

    for (int i = 0; i < 12; ++i) {
        if (i < (int)preset->enabled.size()) {
            effectRows_[i]->enabledToggle.setToggleState(preset->enabled[i], juce::dontSendNotification);
        }
        if (i < (int)preset->wetDryMixes.size()) {
            effectRows_[i]->wetDrySlider.setValue(preset->wetDryMixes[i], juce::dontSendNotification);
        }
    }
}

void EffectsChainUI::refreshUI() {
    refreshPresetList();
    loadPresetToUI();
}

} // namespace LianCore