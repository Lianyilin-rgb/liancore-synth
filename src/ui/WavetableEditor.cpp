// =============================================================================
// LianCore - WavetableEditor 可视化波表编辑器实现
// =============================================================================

#include "WavetableEditor.h"
#include "../ai/AITextToWavetable.h"
#include <cmath>

namespace LianCore {

// =============================================================================
// 构造与析构
// =============================================================================
WavetableEditor::WavetableEditor() {
    harmonics_.resize(kMaxHarmonics, 0.0f);
    harmonics_[0] = 1.0f;
    customWaveform_.resize(WavetableBank::kFrameSize, 0.0f);

    modeButton_.setButtonText("模式: 谐波");
    modeButton_.onClick = [this]() {
        EditMode next = static_cast<EditMode>((static_cast<int>(editMode_) + 1) % 4);
        setEditMode(next);
    };
    addAndMakeVisible(modeButton_);

    previewButton_.setButtonText("预览");
    previewButton_.onClick = [this]() {
        previewing_ ? stopPreview() : startPreview();
    };
    addAndMakeVisible(previewButton_);

    resetButton_.setButtonText("重置");
    resetButton_.onClick = [this]() { resetHarmonics(); };
    addAndMakeVisible(resetButton_);

    presetCombo_.addItem("正弦波", 1);
    presetCombo_.addItem("锯齿波", 2);
    presetCombo_.addItem("方波", 3);
    presetCombo_.addItem("三角波", 4);
    presetCombo_.addItem("脉冲波(25%)", 5);
    presetCombo_.addItem("脉冲波(10%)", 6);
    presetCombo_.addItem("管风琴", 7);
    presetCombo_.addItem("弦乐", 8);
    presetCombo_.addItem("木管", 9);
    presetCombo_.addItem("钟声", 10);
    presetCombo_.setText("预设波形");
    presetCombo_.onChange = [this]() {
        int id = presetCombo_.getSelectedId();
        if (id > 0) setPresetWaveform(presetCombo_.getText());
    };
    addAndMakeVisible(presetCombo_);

    harmonicsLabel_.setText("谐波编辑", juce::dontSendNotification);
    harmonicsLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(harmonicsLabel_);

    drawLabel_.setText("请在波形区域拖拽绘制", juce::dontSendNotification);
    drawLabel_.setJustificationType(juce::Justification::centred);
    drawLabel_.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(drawLabel_);

    // AI生成UI
    aiPromptInput_.setMultiLine(false);
    aiPromptInput_.setTextToShowWhenEmpty("描述波表声音... (例: bright saw)", juce::Colour(0xFF555568));
    aiPromptInput_.setFont(juce::Font(13.0f));
    addAndMakeVisible(aiPromptInput_);

    aiGenerateButton_.setButtonText("AI 生成");
    aiGenerateButton_.onClick = [this]() {
        juce::String text = aiPromptInput_.getText();
        if (text.isEmpty()) text = "bright saw wave";
        aiGenerateFromText(text);
    };
    addAndMakeVisible(aiGenerateButton_);

    aiStatusLabel_.setText("就绪", juce::dontSendNotification);
    aiStatusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF2ecc71));
    aiStatusLabel_.setFont(juce::Font(11.0f));
    addAndMakeVisible(aiStatusLabel_);

    for (int i = 0; i < kVisibleHarmonics; ++i) {
        harmonicSliders_[i].setRange(0.0, 1.0, 0.01);
        harmonicSliders_[i].setSliderStyle(juce::Slider::LinearBarVertical);
        harmonicSliders_[i].setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        harmonicSliders_[i].addListener(this);
        addAndMakeVisible(harmonicSliders_[i]);

        harmonicLabels_[i].setText(juce::String(i + 1), juce::dontSendNotification);
        harmonicLabels_[i].setJustificationType(juce::Justification::centred);
        harmonicLabels_[i].setFont(juce::Font(10.0f));
        addAndMakeVisible(harmonicLabels_[i]);
    }
    harmonicSliders_[0].setValue(1.0f);

    harmonicScrollBar_ = std::make_unique<juce::ScrollBar>(true);
    addAndMakeVisible(*harmonicScrollBar_);
    harmonicScrollBar_->setRangeLimits(0, kMaxHarmonics - kVisibleHarmonics);
    harmonicScrollBar_->setSingleStepSize(1);
    harmonicScrollBar_->addListener(this);

    setEditMode(EditMode::Harmonic);
    setSize(600, 400);
}

WavetableEditor::~WavetableEditor() { stopPreview(); }

void WavetableEditor::setWavetableBank(WavetableBank* bank) {
    bank_ = bank;
    if (bank_ && !bank_->isEmpty()) {
        const float* fd = bank_->getFrameData(0);
        if (fd) {
            WaveformAnalyzer::analyze(fd, WavetableBank::kFrameSize, harmonics_.data(), kMaxHarmonics);
        }
        waveformDirty_ = true;
        repaint();
    }
}

void WavetableEditor::setEditMode(EditMode mode) {
    editMode_ = mode;
    const char* labels[] = {"模式: 谐波", "模式: 绘制", "模式: 预设", "模式: AI生成"};
    modeButton_.setButtonText(labels[static_cast<int>(mode)]);
    resized();
    repaint();
}

// =============================================================================
// AI 波表生成
// =============================================================================
void WavetableEditor::aiGenerateFromText(const juce::String& text) {
    aiStatusLabel_.setText("生成中...", juce::dontSendNotification);
    aiStatusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFFf39c12));
    repaint();

    applyTextToHarmonics(text);
    generateMultiFrameWavetable();

    for (int i = 0; i < kVisibleHarmonics; ++i)
        harmonicSliders_[i].setValue(harmonics_[i], juce::dontSendNotification);

    waveformDirty_ = true;
    applyHarmonics();

    aiStatusLabel_.setText("OK: " + text.substring(0, 25), juce::dontSendNotification);
    aiStatusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF2ecc71));

    if (onAIGenerateComplete) onAIGenerateComplete();
    repaint();
}

void WavetableEditor::applyTextToHarmonics(const juce::String& text) {
    auto result = AI::AITextToWavetable::textToHarmonics(text.toStdString());
    for (int i = 0; i < std::min(kMaxHarmonics, static_cast<int>(result.size())); ++i)
        harmonics_[i] = result[i];
}

void WavetableEditor::generateMultiFrameWavetable() {
    if (!bank_) return;
    auto frames = AI::AITextToWavetable::harmonicsToFrames(harmonics_, 256, WavetableBank::kFrameSize);
    for (int f = 0; f < std::min(256, static_cast<int>(frames.size())); ++f)
        bank_->setFrameData(f, frames[f].data(),
                            std::min(static_cast<int>(frames[f].size()), WavetableBank::kFrameSize));
    bank_->setNumFrames(std::min(256, static_cast<int>(frames.size())));
}

// =============================================================================
// 谐波操作
// =============================================================================
void WavetableEditor::setHarmonic(int index, float amplitude) {
    if (index >= 0 && index < kMaxHarmonics) {
        harmonics_[index] = juce::jlimit(0.0f, 1.0f, amplitude);
        if (index < kVisibleHarmonics)
            harmonicSliders_[index].setValue(amplitude, juce::dontSendNotification);
        waveformDirty_ = true;
        applyHarmonics();
        repaint();
    }
}

float WavetableEditor::getHarmonic(int index) const {
    return (index >= 0 && index < kMaxHarmonics) ? harmonics_[index] : 0.0f;
}

void WavetableEditor::resetHarmonics() {
    for (int i = 0; i < kMaxHarmonics; ++i) harmonics_[i] = 0.0f;
    harmonics_[0] = 1.0f;
    for (int i = 0; i < kVisibleHarmonics; ++i)
        harmonicSliders_[i].setValue(0.0f, juce::dontSendNotification);
    harmonicSliders_[0].setValue(1.0f, juce::dontSendNotification);
    waveformDirty_ = true;
    applyHarmonics();
    repaint();
}

void WavetableEditor::applyHarmonics() {
    if (!bank_) return;
    generateWaveform();
    smoothWaveform();
    bank_->setFrameData(0, customWaveform_.data(), WavetableBank::kFrameSize);
    bank_->setNumFrames(1);
}

void WavetableEditor::setPresetWaveform(const juce::String& name) {
    for (int i = 0; i < kMaxHarmonics; ++i) harmonics_[i] = 0.0f;

    if (name.contains("正弦") || name.contains("Sine")) {
        harmonics_[0] = 1.0f;
    } else if (name.contains("锯齿") || name.contains("Saw")) {
        for (int i = 0; i < 64; ++i) harmonics_[i] = 1.0f / (i + 1);
    } else if (name.contains("方波") || name.contains("Square")) {
        for (int i = 0; i < 64; i += 2) harmonics_[i] = 1.0f / (i + 1);
    } else if (name.contains("三角") || name.contains("Triangle")) {
        for (int i = 0; i < 64; i += 2) {
            float n = static_cast<float>(i + 1);
            harmonics_[i] = 1.0f / (n * n);
        }
    } else if (name.contains("25%")) {
        for (int i = 0; i < 64; ++i) harmonics_[i] = 0.5f / (i + 1);
    } else if (name.contains("10%")) {
        for (int i = 0; i < 64; ++i) harmonics_[i] = 0.4f / (i + 1);
    } else if (name.contains("管风琴") || name.contains("Organ")) {
        harmonics_[0]=1.0f; harmonics_[1]=0.5f; harmonics_[2]=0.333f; harmonics_[3]=0.25f;
        harmonics_[4]=0.2f; harmonics_[5]=0.167f; harmonics_[6]=0.143f; harmonics_[7]=0.125f;
    } else if (name.contains("弦乐") || name.contains("String")) {
        for (int i = 0; i < 16; ++i) harmonics_[i] = 0.8f / (i + 1);
    } else if (name.contains("木管") || name.contains("Wood")) {
        harmonics_[0]=1.0f; harmonics_[1]=0.6f; harmonics_[2]=0.3f; harmonics_[3]=0.1f;
    } else if (name.contains("钟声") || name.contains("Bell")) {
        harmonics_[0]=1.0f; harmonics_[2]=0.7f; harmonics_[5]=0.5f; harmonics_[9]=0.3f;
        harmonics_[14]=0.2f; harmonics_[20]=0.1f;
    }

    for (int i = 0; i < kVisibleHarmonics; ++i)
        harmonicSliders_[i].setValue(harmonics_[i], juce::dontSendNotification);
    waveformDirty_ = true;
    applyHarmonics();
    repaint();
}

// =============================================================================
// 预览
// =============================================================================
void WavetableEditor::startPreview() {
    if (!bank_ || bank_->isEmpty()) return;
    previewing_ = true;
    previewAngle_ = 0.0;
    previewButton_.setButtonText("停止");
    startTimerHz(60);
}

void WavetableEditor::stopPreview() {
    previewing_ = false;
    stopTimer();
    previewButton_.setButtonText("预览");
}

void WavetableEditor::timerCallback() {
    if (!previewing_ || !bank_) return;
    previewAngle_ += 440.0 / sampleRate_ * juce::MathConstants<double>::twoPi;
    if (previewAngle_ > juce::MathConstants<double>::twoPi)
        previewAngle_ -= juce::MathConstants<double>::twoPi;
    repaint();
}

// =============================================================================
// 绘制
// =============================================================================
void WavetableEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(42, 42, 42));
    drawWaveform(g, waveformBounds_);
    if (editMode_ == EditMode::Harmonic) drawHarmonicBars(g, harmonicBounds_);
    else if (editMode_ == EditMode::AIGenerate) drawAIPanel(g, aiPanelBounds_);
}

void WavetableEditor::drawWaveform(juce::Graphics& g, const juce::Rectangle<int>& bounds) {
    g.setColour(juce::Colour(30, 30, 30));
    g.fillRect(bounds);
    drawGrid(g, bounds);

    if (waveformDirty_ || bank_ == nullptr) generateWaveform();

    int width = bounds.getWidth(), height = bounds.getHeight(), centerY = bounds.getCentreY();
    float halfH = height * 0.45f;
    juce::Path wp;
    wp.startNewSubPath(0.0f, static_cast<float>(centerY));
    for (int x = 0; x < width; ++x) {
        float phase = static_cast<float>(x) / static_cast<float>(width - 1);
        int idx = static_cast<int>(phase * WavetableBank::kFrameSize);
        float val = (idx < static_cast<int>(customWaveform_.size())) ? customWaveform_[idx] : 0.0f;
        wp.lineTo(static_cast<float>(x), static_cast<float>(centerY) - val * halfH);
    }
    g.setColour(juce::Colour(0, 200, 255));
    g.strokePath(wp, juce::PathStrokeType(2.0f));

    if (previewing_) {
        float px = static_cast<float>(previewAngle_ / juce::MathConstants<double>::twoPi * width);
        g.setColour(juce::Colours::red);
        g.drawLine(px, static_cast<float>(bounds.getY()), px, static_cast<float>(bounds.getBottom()), 1.0f);
    }
    g.setColour(juce::Colours::grey);
    g.drawRect(bounds, 1);
}

void WavetableEditor::drawGrid(juce::Graphics& g, const juce::Rectangle<int>& bounds) {
    g.setColour(juce::Colour(50, 50, 50));
    int cy = bounds.getCentreY();
    g.drawLine(0.0f, static_cast<float>(cy), static_cast<float>(bounds.getWidth()), static_cast<float>(cy));
    for (int x = 0; x < bounds.getWidth(); x += bounds.getWidth() / 4)
        g.drawLine(static_cast<float>(x), static_cast<float>(bounds.getY()),
                   static_cast<float>(x), static_cast<float>(bounds.getBottom()), 0.5f);
}

void WavetableEditor::drawHarmonicBars(juce::Graphics& g, const juce::Rectangle<int>& bounds) {
    g.setColour(juce::Colour(30, 30, 30));
    g.fillRect(bounds);
    int startH = static_cast<int>(harmonicScrollBar_->getCurrentRangeStart());
    float bw = static_cast<float>(bounds.getWidth()) / kVisibleHarmonics;
    float maxH = static_cast<float>(bounds.getHeight()) * 0.9f;

    for (int i = 0; i < kVisibleHarmonics; ++i) {
        int h = startH + i;
        if (h >= kMaxHarmonics) break;
        float amp = harmonics_[h], barH = amp * maxH;
        g.setColour((h == 0) ? juce::Colour(255, 180, 50)
                    : (h % 2 == 0) ? juce::Colour(100, 180, 255) : juce::Colour(80, 140, 200));
        float x = static_cast<float>(bounds.getX()) + i * bw + 2;
        g.fillRect(x, static_cast<float>(bounds.getBottom()) - barH, bw - 4, barH);
        g.setColour(juce::Colours::grey);
        g.setFont(9.0f);
        g.drawText(juce::String(h + 1), static_cast<int>(x), bounds.getBottom() - 12,
                   static_cast<int>(bw - 4), 12, juce::Justification::centred);
    }
    g.setColour(juce::Colours::grey);
    g.drawRect(bounds, 1);
}

void WavetableEditor::drawAIPanel(juce::Graphics& g, const juce::Rectangle<int>& bounds) {
    g.setColour(juce::Colour(28, 28, 38));
    g.fillRect(bounds);
    g.setColour(juce::Colour(0xFF6c5ce7));
    g.setFont(13.0f);
    g.drawText("AI 波表生成", bounds.getX(), bounds.getY() + 2, bounds.getWidth(), 20,
               juce::Justification::centred);
    g.setColour(juce::Colours::grey);
    g.drawRect(bounds, 1);
}

// =============================================================================
// 布局
// =============================================================================
void WavetableEditor::resized() {
    auto area = getLocalBounds().reduced(4);
    auto topBar = area.removeFromTop(28);
    modeButton_.setBounds(topBar.removeFromLeft(100));
    topBar.removeFromLeft(4);
    previewButton_.setBounds(topBar.removeFromLeft(60));
    topBar.removeFromLeft(4);
    resetButton_.setBounds(topBar.removeFromLeft(50));
    topBar.removeFromLeft(4);
    presetCombo_.setBounds(topBar.removeFromLeft(120));
    topBar.removeFromLeft(4);
    drawLabel_.setBounds(topBar);
    area.removeFromTop(4);

    if (editMode_ == EditMode::AIGenerate) {
        auto aiRow = area.removeFromTop(30);
        aiPromptInput_.setBounds(aiRow.removeFromLeft(aiRow.getWidth() - 100));
        aiGenerateButton_.setBounds(aiRow.withWidth(100).reduced(4, 2));
        area.removeFromTop(2);
        aiStatusLabel_.setBounds(area.removeFromTop(18));
        area.removeFromTop(4);
        aiPanelBounds_ = juce::Rectangle<int>(getLocalBounds().getX(), aiRow.getY() - 4,
                                               getLocalBounds().getWidth(), 60);
    }

    waveformBounds_ = area.removeFromTop(area.getHeight() * 2 / 3);
    area.removeFromTop(4);
    harmonicBounds_ = (editMode_ != EditMode::AIGenerate) ? area : juce::Rectangle<int>(0,0,1,1);

    if (editMode_ == EditMode::Harmonic) {
        float sw = static_cast<float>(harmonicBounds_.getWidth()) / kVisibleHarmonics;
        for (int i = 0; i < kVisibleHarmonics; ++i) {
            int x = harmonicBounds_.getX() + static_cast<int>(i * sw);
            harmonicSliders_[i].setBounds(x + 2, harmonicBounds_.getY() + 14,
                                          static_cast<int>(sw - 4), harmonicBounds_.getHeight() - 28);
            harmonicLabels_[i].setBounds(x + 2, harmonicBounds_.getBottom() - 14,
                                         static_cast<int>(sw - 4), 12);
        }
    }
}

// =============================================================================
// 鼠标
// =============================================================================
void WavetableEditor::mouseDown(const juce::MouseEvent& e) {
    if (editMode_ == EditMode::FreeDraw && waveformBounds_.contains(e.getPosition())) {
        isDrawing_ = true; handleFreeDraw(e);
    } else if (editMode_ == EditMode::Harmonic && harmonicBounds_.contains(e.getPosition())) {
        handleHarmonicDrag(e);
    }
}
void WavetableEditor::mouseDrag(const juce::MouseEvent& e) {
    if (isDrawing_) handleFreeDraw(e);
    else if (editMode_ == EditMode::Harmonic && harmonicBounds_.contains(e.getPosition()))
        handleHarmonicDrag(e);
}
void WavetableEditor::mouseUp(const juce::MouseEvent&) {
    if (isDrawing_) { isDrawing_ = false; finishDrawing(); }
}

void WavetableEditor::handleFreeDraw(const juce::MouseEvent& e) {
    if (!waveformBounds_.contains(e.getPosition())) return;
    float rx = static_cast<float>(e.getPosition().x - waveformBounds_.getX()) / waveformBounds_.getWidth();
    float ry = 1.0f - static_cast<float>(e.getPosition().y - waveformBounds_.getY()) / waveformBounds_.getHeight();
    int idx = static_cast<int>(rx * WavetableBank::kFrameSize);
    float val = (ry - 0.5f) * 2.0f;
    if (idx >= 0 && idx < static_cast<int>(customWaveform_.size())) {
        int r = 8;
        for (int i = std::max(0, idx - r); i < std::min(static_cast<int>(customWaveform_.size()), idx + r); ++i) {
            float w = 1.0f - std::abs(static_cast<float>(i - idx)) / r;
            customWaveform_[i] = customWaveform_[i] * (1.0f - w * 0.5f) + val * w * 0.5f;
        }
    }
    waveformDirty_ = false;
    repaint();
}

void WavetableEditor::handleHarmonicDrag(const juce::MouseEvent& e) {
    int startH = static_cast<int>(harmonicScrollBar_->getCurrentRangeStart());
    float bw = static_cast<float>(harmonicBounds_.getWidth()) / kVisibleHarmonics;
    int bi = static_cast<int>((e.getPosition().x - harmonicBounds_.getX()) / bw);
    int hi = startH + bi;
    if (hi >= 0 && hi < kMaxHarmonics) {
        float ry = 1.0f - static_cast<float>(e.getPosition().y - harmonicBounds_.getY()) / harmonicBounds_.getHeight();
        setHarmonic(hi, juce::jlimit(0.0f, 1.0f, ry));
    }
}

void WavetableEditor::finishDrawing() {
    smoothWaveform();
    if (bank_) {
        bank_->setFrameData(0, customWaveform_.data(), WavetableBank::kFrameSize);
        bank_->setNumFrames(1);
    }
    WaveformAnalyzer::analyze(customWaveform_.data(), WavetableBank::kFrameSize, harmonics_.data(), kMaxHarmonics);
    for (int i = 0; i < kVisibleHarmonics; ++i)
        harmonicSliders_[i].setValue(harmonics_[i], juce::dontSendNotification);
    waveformDirty_ = true;
    repaint();
}

// =============================================================================
// 波形生成
// =============================================================================
void WavetableEditor::generateWaveform() {
    int sz = WavetableBank::kFrameSize;
    customWaveform_.resize(sz, 0.0f);
    for (int i = 0; i < sz; ++i) {
        float phase = static_cast<float>(i) / sz;
        float val = 0.0f;
        for (int h = 0; h < kMaxHarmonics; ++h)
            if (harmonics_[h] > 0.001f)
                val += harmonics_[h] * std::sin(phase * (h + 1) * juce::MathConstants<float>::twoPi);
        float norm = 0.0f;
        for (int h = 0; h < kMaxHarmonics; ++h) norm += harmonics_[h];
        if (norm > 0.001f) val /= norm;
        customWaveform_[i] = val;
    }
    waveformDirty_ = false;
}

void WavetableEditor::smoothWaveform() {
    std::vector<float> sm(customWaveform_.size());
    for (size_t i = 0; i < customWaveform_.size(); ++i) {
        float s = 0.0f; int c = 0;
        for (int j = -2; j <= 2; ++j) {
            int idx = static_cast<int>(i) + j;
            if (idx >= 0 && idx < static_cast<int>(customWaveform_.size())) { s += customWaveform_[idx]; c++; }
        }
        sm[i] = s / c;
    }
    customWaveform_ = std::move(sm);
}

void WavetableEditor::sliderValueChanged(juce::Slider* slider) {
    for (int i = 0; i < kVisibleHarmonics; ++i) {
        if (slider == &harmonicSliders_[i]) {
            int hi = static_cast<int>(harmonicScrollBar_->getCurrentRangeStart()) + i;
            if (hi < kMaxHarmonics) {
                harmonics_[hi] = static_cast<float>(slider->getValue());
                waveformDirty_ = true;
                applyHarmonics();
                repaint();
            }
            break;
        }
    }
}
void WavetableEditor::scrollBarMoved(juce::ScrollBar*, double) { resized(); repaint(); }

namespace WaveformAnalyzer {
    void analyze(const float* waveform, int size, float* harmonics, int numHarmonics) {
        for (int h = 0; h < numHarmonics; ++h) {
            float real = 0.0f, imag = 0.0f;
            for (int i = 0; i < size; ++i) {
                float ph = static_cast<float>(i) * (h + 1) * juce::MathConstants<float>::twoPi / size;
                real += waveform[i] * std::cos(ph);
                imag -= waveform[i] * std::sin(ph);
            }
            real /= size; imag /= size;
            harmonics[h] = std::sqrt(real * real + imag * imag) * 2.0f;
        }
    }
}

} // namespace LianCore