// =============================================================================
// LianCore - GranularEngineUI 实现 (P6-2)
// 粒子合成引擎UI组件，包含参数控制、包络可视化、AI控制
// =============================================================================
#include "GranularEngineUI.h"

namespace LianCore {

// =============================================================================
// 辅助函数：创建带样式的滑块
// =============================================================================
static void setupSlider(juce::Slider& slider, double minVal, double maxVal, double defaultVal,
                        double step = 0.01) {
    slider.setRange(minVal, maxVal, step);
    slider.setValue(defaultVal);
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 55, 18);
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff00d4ff));
    slider.setColour(juce::Slider::trackColourId, juce::Colour(0xff2a2a3a));
    slider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff1a1a2e));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffe0e0e0));
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff0a0a0f));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff2a2a3a));
}

static void setupLabel(juce::Label& label, const juce::String& text) {
    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font(10.0f));
    label.setColour(juce::Label::textColourId, juce::Colour(0xff8888a0));
    label.setJustificationType(juce::Justification::centredLeft);
}

// =============================================================================
GranularEngineUI::GranularEngineUI() {
    // ---- 粒子大小滑块 ----
    setupSlider(grainSizeSlider_, 1.0, 500.0, 50.0);
    addAndMakeVisible(grainSizeSlider_);
    grainSizeSlider_.addListener(this);
    setupLabel(grainSizeLabel_, "粒子大小 (ms)");
    addAndMakeVisible(grainSizeLabel_);

    // ---- 粒子密度滑块 ----
    setupSlider(grainDensitySlider_, 1.0, 200.0, 30.0);
    addAndMakeVisible(grainDensitySlider_);
    grainDensitySlider_.addListener(this);
    setupLabel(grainDensityLabel_, "粒子密度 (/s)");
    addAndMakeVisible(grainDensityLabel_);

    // ---- 粒子位置滑块 ----
    setupSlider(grainPositionSlider_, 0.0, 1.0, 0.5);
    addAndMakeVisible(grainPositionSlider_);
    grainPositionSlider_.addListener(this);
    setupLabel(grainPositionLabel_, "源位置");
    addAndMakeVisible(grainPositionLabel_);

    // ---- 音高随机滑块 ----
    setupSlider(pitchRandomSlider_, 0.0, 24.0, 0.0);
    addAndMakeVisible(pitchRandomSlider_);
    pitchRandomSlider_.addListener(this);
    setupLabel(pitchRandomLabel_, "音高随机 (半音)");
    addAndMakeVisible(pitchRandomLabel_);

    // ---- 声像随机滑块 ----
    setupSlider(panRandomSlider_, 0.0, 1.0, 0.0);
    addAndMakeVisible(panRandomSlider_);
    panRandomSlider_.addListener(this);
    setupLabel(panRandomLabel_, "声像随机");
    addAndMakeVisible(panRandomLabel_);

    // ---- 散射滑块 ----
    setupSlider(scatterSlider_, 0.0, 1.0, 0.5);
    addAndMakeVisible(scatterSlider_);
    scatterSlider_.addListener(this);
    setupLabel(scatterLabel_, "散射量");
    addAndMakeVisible(scatterLabel_);

    // ---- 音量滑块 ----
    setupSlider(volumeSlider_, 0.0, 1.0, 1.0);
    addAndMakeVisible(volumeSlider_);
    volumeSlider_.addListener(this);
    setupLabel(volumeLabel_, "音量");
    addAndMakeVisible(volumeLabel_);

    // ---- 音高偏移滑块 ----
    setupSlider(pitchShiftSlider_, -24.0, 24.0, 0.0);
    addAndMakeVisible(pitchShiftSlider_);
    pitchShiftSlider_.addListener(this);
    setupLabel(pitchShiftLabel_, "音高偏移 (半音)");
    addAndMakeVisible(pitchShiftLabel_);

    // ---- 包络类型选择器 ----
    envelopeTypeCombo_.addItem("Hann", 1);
    envelopeTypeCombo_.addItem("Hamming", 2);
    envelopeTypeCombo_.addItem("Triangle", 3);
    envelopeTypeCombo_.addItem("Exponential", 4);
    envelopeTypeCombo_.addItem("Rectangular", 5);
    envelopeTypeCombo_.addItem("Blackman", 6);
    envelopeTypeCombo_.addItem("Gaussian", 7);
    envelopeTypeCombo_.addItem("Tukey", 8);
    envelopeTypeCombo_.addItem("Cosine", 9);
    envelopeTypeCombo_.addItem("Welch", 10);
    envelopeTypeCombo_.setSelectedId(1);
    envelopeTypeCombo_.addListener(this);
    addAndMakeVisible(envelopeTypeCombo_);
    setupLabel(envelopeTypeLabel_, "包络类型");
    addAndMakeVisible(envelopeTypeLabel_);

    // ---- 方向控制 ----
    directionCombo_.addItem("正向", 1);
    directionCombo_.addItem("反向", 2);
    directionCombo_.addItem("双向", 3);
    directionCombo_.setSelectedId(1);
    directionCombo_.addListener(this);
    addAndMakeVisible(directionCombo_);
    setupLabel(directionLabel_, "方向");
    addAndMakeVisible(directionLabel_);

    // ---- AI密度控制 ----
    aiDensityToggle_.setButtonText("AI密度优化");
    aiDensityToggle_.setColour(juce::ToggleButton::textColourId, textColor_);
    aiDensityToggle_.setColour(juce::ToggleButton::tickColourId, accentColor_);
    aiDensityToggle_.onClick = [this]() {
        auto* player = getPlayer();
        if (player) {
            player->enableAiDensityControl(aiDensityToggle_.getToggleState());
        }
    };
    addAndMakeVisible(aiDensityToggle_);
    setupLabel(aiDensityLabel_, "AI自动优化粒子密度");
    addAndMakeVisible(aiDensityLabel_);

    // ---- 加载源文件按钮 ----
    loadSourceButton_.setButtonText("加载音频文件");
    loadSourceButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a3a));
    loadSourceButton_.onClick = [this]() { onLoadSourceFile(); };
    addAndMakeVisible(loadSourceButton_);

    sourceStatusLabel_.setText("未加载", juce::dontSendNotification);
    sourceStatusLabel_.setFont(juce::Font(10.0f));
    sourceStatusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xffff6b6b));
    addAndMakeVisible(sourceStatusLabel_);

    // ---- 状态标签 ----
    activeGrainLabel_.setText("活跃粒子: 0", juce::dontSendNotification);
    activeGrainLabel_.setFont(juce::Font(10.0f));
    activeGrainLabel_.setColour(juce::Label::textColourId, textColor_);
    addAndMakeVisible(activeGrainLabel_);

    cpuLoadLabel_.setText("CPU: 0.0%", juce::dontSendNotification);
    cpuLoadLabel_.setFont(juce::Font(10.0f));
    cpuLoadLabel_.setColour(juce::Label::textColourId, textColor_);
    addAndMakeVisible(cpuLoadLabel_);

    // ---- 包络可视化 ----
    addAndMakeVisible(envelopeVisualizer_);
    envelopeVisualizer_.startAnimation(20);
}

GranularEngineUI::~GranularEngineUI() {
    envelopeVisualizer_.stopAnimation();
}

void GranularEngineUI::paint(juce::Graphics& g) {
    g.fillAll(bgColor_);
    g.setColour(panelColor_);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 4.0f, 1.0f);

    // 更新状态显示
    auto* player = getPlayer();
    if (player) {
        activeGrainLabel_.setText("活跃粒子: " + juce::String(player->getActiveGrainCount()),
                                  juce::dontSendNotification);
        cpuLoadLabel_.setText("CPU: " + juce::String(player->getCurrentCpuLoad() * 100.0f, 1) + "%",
                              juce::dontSendNotification);
        if (player->isSourceLoaded()) {
            sourceStatusLabel_.setText("已加载", juce::dontSendNotification);
            sourceStatusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff2ed573));
        }
    }
}

void GranularEngineUI::resized() {
    auto area = getLocalBounds().reduced(8);

    // 顶部：包络可视化 (120px)
    envelopeVisualizer_.setBounds(area.removeFromTop(120));
    area.removeFromTop(4);

    // 第二行：源文件加载 + 状态
    auto sourceRow = area.removeFromTop(26);
    loadSourceButton_.setBounds(sourceRow.removeFromLeft(110).reduced(2, 2));
    sourceRow.removeFromLeft(4);
    sourceStatusLabel_.setBounds(sourceRow.removeFromLeft(100));
    activeGrainLabel_.setBounds(sourceRow.removeFromLeft(100));
    cpuLoadLabel_.setBounds(sourceRow);

    area.removeFromTop(4);

    // 参数滑块区域 (两列布局)
    auto sliderArea = area;
    int colWidth = sliderArea.getWidth() / 2;
    auto leftCol = sliderArea.removeFromLeft(colWidth).reduced(0, 0);
    auto rightCol = sliderArea.reduced(0, 0);

    auto addSliderRow = [](juce::Rectangle<int>& col, juce::Label& label, juce::Slider& slider) {
        auto row = col.removeFromTop(38);
        label.setBounds(row.removeFromTop(16));
        slider.setBounds(row);
    };

    addSliderRow(leftCol, grainSizeLabel_, grainSizeSlider_);
    addSliderRow(leftCol, grainDensityLabel_, grainDensitySlider_);
    addSliderRow(leftCol, grainPositionLabel_, grainPositionSlider_);
    addSliderRow(leftCol, pitchRandomLabel_, pitchRandomSlider_);

    addSliderRow(rightCol, panRandomLabel_, panRandomSlider_);
    addSliderRow(rightCol, scatterLabel_, scatterSlider_);
    addSliderRow(rightCol, volumeLabel_, volumeSlider_);
    addSliderRow(rightCol, pitchShiftLabel_, pitchShiftSlider_);

    // 底部：包络类型 + 方向 + AI
    auto bottomRow = leftCol;
    auto comboRow = bottomRow.removeFromTop(38);
    envelopeTypeLabel_.setBounds(comboRow.removeFromLeft(60));
    envelopeTypeCombo_.setBounds(comboRow.removeFromLeft(110).reduced(2, 2));
    comboRow.removeFromLeft(8);
    directionLabel_.setBounds(comboRow.removeFromLeft(40));
    directionCombo_.setBounds(comboRow.reduced(2, 2));

    auto aiRow = bottomRow.removeFromTop(38);
    aiDensityLabel_.setBounds(aiRow.removeFromTop(16));
    aiDensityToggle_.setBounds(aiRow);
}

void GranularEngineUI::sliderValueChanged(juce::Slider* slider) {
    auto* player = getPlayer();
    if (!player) return;

    if (slider == &grainSizeSlider_) {
        player->setGrainSize((float)slider->getValue());
    } else if (slider == &grainDensitySlider_) {
        player->setGrainDensity((float)slider->getValue());
        envelopeVisualizer_.setGrainDensity((float)slider->getValue());
    } else if (slider == &grainPositionSlider_) {
        player->setGrainPosition((float)slider->getValue());
    } else if (slider == &pitchRandomSlider_) {
        player->setGrainPitchRandom((float)slider->getValue());
    } else if (slider == &panRandomSlider_) {
        player->setGrainPanRandom((float)slider->getValue());
    } else if (slider == &scatterSlider_) {
        player->setGrainScatter((float)slider->getValue());
    } else if (slider == &volumeSlider_) {
        player->setVolume((float)slider->getValue());
    } else if (slider == &pitchShiftSlider_) {
        player->setPitchShift((float)slider->getValue());
    }
}

void GranularEngineUI::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) {
    auto* player = getPlayer();
    if (!player) return;

    if (comboBoxThatHasChanged == &envelopeTypeCombo_) {
        auto type = static_cast<GrainEnvelopeType>(envelopeTypeCombo_.getSelectedId() - 1);
        player->setGrainEnvelopeType(type);
        envelopeVisualizer_.setEnvelopeType(type);
    } else if (comboBoxThatHasChanged == &directionCombo_) {
        auto dir = static_cast<GrainDirection>(directionCombo_.getSelectedId() - 1);
        player->setGrainDirection(dir);
    }
}

void GranularEngineUI::onLoadSourceFile() {
    auto chooser = std::make_unique<juce::FileChooser>(
        "加载音频文件作为粒子源...",
        juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
        "*.wav;*.mp3;*.aiff;*.flac;*.ogg");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file != juce::File()) {
                auto* player = getPlayer();
                if (player) {
                    player->loadSourceFile(file);
                    if (player->isSourceLoaded()) {
                        sourceStatusLabel_.setText("已加载: " + file.getFileName(),
                                                   juce::dontSendNotification);
                        sourceStatusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff2ed573));
                    } else {
                        sourceStatusLabel_.setText("加载失败", juce::dontSendNotification);
                        sourceStatusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xffff6b6b));
                    }
                }
            }
        });
}

void GranularEngineUI::refreshParameters() {
    auto* player = getPlayer();
    if (!player) return;

    grainSizeSlider_.setValue(player->getParameter(0), juce::dontSendNotification);
    grainDensitySlider_.setValue(player->getParameter(1), juce::dontSendNotification);
    grainPositionSlider_.setValue(player->getParameter(2), juce::dontSendNotification);
    pitchRandomSlider_.setValue(player->getParameter(3), juce::dontSendNotification);
    panRandomSlider_.setValue(player->getParameter(4), juce::dontSendNotification);
    scatterSlider_.setValue(player->getParameter(5), juce::dontSendNotification);
    volumeSlider_.setValue(player->getParameter(10), juce::dontSendNotification);
    pitchShiftSlider_.setValue(player->getParameter(11), juce::dontSendNotification);

    envelopeTypeCombo_.setSelectedId(static_cast<int>(player->getGrainEnvelopeType()) + 1,
                                     juce::dontSendNotification);
    envelopeVisualizer_.setEnvelopeType(player->getGrainEnvelopeType());

    directionCombo_.setSelectedId(static_cast<int>(player->getGrainDirection()) + 1,
                                  juce::dontSendNotification);

    aiDensityToggle_.setToggleState(player->isAiDensityControlEnabled(), juce::dontSendNotification);
}

GranularPlayer* GranularEngineUI::getPlayer() {
    return playerRef_ ? playerRef_() : nullptr;
}

} // namespace LianCore