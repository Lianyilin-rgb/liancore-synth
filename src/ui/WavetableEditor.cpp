// =============================================================================
// LianCore - WavetableEditor 可视化波表编辑器实现
// =============================================================================

#include "WavetableEditor.h"
#include <cmath>

namespace LianCore {

// =============================================================================
// 构造与析构
// =============================================================================
WavetableEditor::WavetableEditor() {
    // 初始化谐波数组
    harmonics_.resize(kMaxHarmonics, 0.0f);
    harmonics_[0] = 1.0f; // 基频全开

    // 波形缓冲区
    customWaveform_.resize(WavetableBank::kFrameSize, 0.0f);

    // 模式按钮
    modeButton_.setButtonText("模式: 谐波");
    modeButton_.onClick = [this]() {
        EditMode next = static_cast<EditMode>((static_cast<int>(editMode_) + 1) % 3);
        setEditMode(next);
    };
    addAndMakeVisible(modeButton_);

    // 预览按钮
    previewButton_.setButtonText("预览");
    previewButton_.onClick = [this]() {
        if (previewing_) {
            stopPreview();
        } else {
            startPreview();
        }
    };
    addAndMakeVisible(previewButton_);

    // 重置按钮
    resetButton_.setButtonText("重置");
    resetButton_.onClick = [this]() {
        resetHarmonics();
    };
    addAndMakeVisible(resetButton_);

    // 预设下拉框
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
        if (id > 0) {
            setPresetWaveform(presetCombo_.getText());
        }
    };
    addAndMakeVisible(presetCombo_);

    // 标签
    harmonicsLabel_.setText("谐波编辑", juce::dontSendNotification);
    harmonicsLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(harmonicsLabel_);

    drawLabel_.setText("请在波形区域拖拽绘制", juce::dontSendNotification);
    drawLabel_.setJustificationType(juce::Justification::centred);
    drawLabel_.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(drawLabel_);

    // 谐波滑块
    for (int i = 0; i < kVisibleHarmonics; ++i) {
        harmonicSliders_[i].setRange(0.0, 1.0, 0.01);
        harmonicSliders_[i].setValue(0.0f);
        harmonicSliders_[i].setSliderStyle(juce::Slider::LinearBarVertical);
        harmonicSliders_[i].setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        harmonicSliders_[i].addListener(this);
        addAndMakeVisible(harmonicSliders_[i]);

        harmonicLabels_[i].setText(juce::String(i + 1), juce::dontSendNotification);
        harmonicLabels_[i].setJustificationType(juce::Justification::centred);
        harmonicLabels_[i].setFont(juce::Font(10.0f));
        addAndMakeVisible(harmonicLabels_[i]);
    }

    harmonicSliders_[0].setValue(1.0f); // 基频

    // 滚动条
    harmonicScrollBar_ = std::make_unique<juce::ScrollBar>(true);
    addAndMakeVisible(*harmonicScrollBar_);
    harmonicScrollBar_->setRangeLimits(0, kMaxHarmonics - kVisibleHarmonics);
    harmonicScrollBar_->setSingleStepSize(1);
    harmonicScrollBar_->addListener(this);

    // 初始模式
    setEditMode(EditMode::Harmonic);
    setSize(600, 400);
}

WavetableEditor::~WavetableEditor() {
    stopPreview();
}

// =============================================================================
// 波表设置
// =============================================================================
void WavetableEditor::setWavetableBank(WavetableBank* bank) {
    bank_ = bank;
    if (bank_ && !bank_->isEmpty()) {
        // 从波表提取谐波
        const float* frameData = bank_->getFrameData(0);
        if (frameData) {
            WaveformAnalyzer::analyze(frameData, WavetableBank::kFrameSize,
                                       harmonics_.data(), kMaxHarmonics);
        }
        waveformDirty_ = true;
        repaint();
    }
}

// =============================================================================
// 模式切换
// =============================================================================
void WavetableEditor::setEditMode(EditMode mode) {
    editMode_ = mode;
    switch (mode) {
        case EditMode::Harmonic:
            modeButton_.setButtonText("模式: 谐波");
            break;
        case EditMode::FreeDraw:
            modeButton_.setButtonText("模式: 绘制");
            break;
        case EditMode::Preset:
            modeButton_.setButtonText("模式: 预设");
            break;
    }
    resized();
    repaint();
}

// =============================================================================
// 谐波操作
// =============================================================================
void WavetableEditor::setHarmonic(int index, float amplitude) {
    if (index >= 0 && index < kMaxHarmonics) {
        harmonics_[index] = juce::jlimit(0.0f, 1.0f, amplitude);
        if (index < kVisibleHarmonics) {
            harmonicSliders_[index].setValue(amplitude, juce::dontSendNotification);
        }
        waveformDirty_ = true;
        applyHarmonics();
        repaint();
    }
}

float WavetableEditor::getHarmonic(int index) const {
    if (index >= 0 && index < kMaxHarmonics) {
        return harmonics_[index];
    }
    return 0.0f;
}

void WavetableEditor::resetHarmonics() {
    // 仅保留基频
    for (int i = 0; i < kMaxHarmonics; ++i) {
        harmonics_[i] = 0.0f;
    }
    harmonics_[0] = 1.0f;

    // 更新滑块
    for (int i = 0; i < kVisibleHarmonics; ++i) {
        harmonicSliders_[i].setValue(0.0f, juce::dontSendNotification);
    }
    harmonicSliders_[0].setValue(1.0f, juce::dontSendNotification);

    waveformDirty_ = true;
    applyHarmonics();
    repaint();
}

void WavetableEditor::applyHarmonics() {
    if (!bank_) return;

    // 生成波形
    generateWaveform();
    smoothWaveform();

    // 写入波表
    bank_->setFrameData(0, customWaveform_.data(), WavetableBank::kFrameSize);
    bank_->setNumFrames(1);
}

// =============================================================================
// 预设波形
// =============================================================================
void WavetableEditor::setPresetWaveform(const juce::String& name) {
    for (int i = 0; i < kMaxHarmonics; ++i) {
        harmonics_[i] = 0.0f;
    }

    if (name.contains("正弦") || name.contains("Sine")) {
        harmonics_[0] = 1.0f;
    } else if (name.contains("锯齿") || name.contains("Saw")) {
        for (int i = 0; i < 64; ++i) {
            harmonics_[i] = 1.0f / (i + 1);
        }
    } else if (name.contains("方波") || name.contains("Square")) {
        for (int i = 0; i < 64; i += 2) {
            harmonics_[i] = 1.0f / (i + 1);
        }
    } else if (name.contains("三角") || name.contains("Triangle")) {
        for (int i = 0; i < 64; i += 2) {
            float n = i + 1;
            harmonics_[i] = 1.0f / (n * n);
        }
    } else if (name.contains("25%")) {
        // 脉冲波 25% 占空比
        for (int i = 0; i < 64; ++i) {
            harmonics_[i] = 0.5f / (i + 1);
        }
    } else if (name.contains("10%")) {
        for (int i = 0; i < 64; ++i) {
            harmonics_[i] = 0.4f / (i + 1);
        }
    } else if (name.contains("管风琴") || name.contains("Organ")) {
        harmonics_[0] = 1.0f;
        harmonics_[1] = 0.5f;
        harmonics_[2] = 0.333f;
        harmonics_[3] = 0.25f;
        harmonics_[4] = 0.2f;
        harmonics_[5] = 0.167f;
        harmonics_[6] = 0.143f;
        harmonics_[7] = 0.125f;
    } else if (name.contains("弦乐") || name.contains("String")) {
        for (int i = 0; i < 16; ++i) {
            harmonics_[i] = 0.8f / (i + 1);
        }
    } else if (name.contains("木管") || name.contains("Wood")) {
        harmonics_[0] = 1.0f;
        harmonics_[1] = 0.6f;
        harmonics_[2] = 0.3f;
        harmonics_[3] = 0.1f;
    } else if (name.contains("钟声") || name.contains("Bell")) {
        // 非谐波泛音结构
        harmonics_[0] = 1.0f;
        harmonics_[2] = 0.7f;
        harmonics_[5] = 0.5f;
        harmonics_[9] = 0.3f;
        harmonics_[14] = 0.2f;
        harmonics_[20] = 0.1f;
    }

    // 更新滑块
    for (int i = 0; i < kVisibleHarmonics; ++i) {
        harmonicSliders_[i].setValue(harmonics_[i], juce::dontSendNotification);
    }

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

    // 高亮显示当前播放位置
    previewAngle_ += 440.0 / sampleRate_ * juce::MathConstants<double>::twoPi;
    if (previewAngle_ > juce::MathConstants<double>::twoPi) {
        previewAngle_ -= juce::MathConstants<double>::twoPi;
    }
    repaint();
}

// =============================================================================
// 绘制
// =============================================================================
void WavetableEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(42, 42, 42));

    // 绘制波形区域
    drawWaveform(g, waveformBounds_);

    // 绘制谐波区域
    if (editMode_ == EditMode::Harmonic) {
        drawHarmonicBars(g, harmonicBounds_);
    }
}

void WavetableEditor::drawWaveform(juce::Graphics& g, const juce::Rectangle<int>& bounds) {
    // 背景
    g.setColour(juce::Colour(30, 30, 30));
    g.fillRect(bounds);

    // 网格
    drawGrid(g, bounds);

    // 生成波形路径
    if (waveformDirty_ || bank_ == nullptr) {
        generateWaveform();
    }

    int width = bounds.getWidth();
    int height = bounds.getHeight();
    int centerY = bounds.getCentreY();
    float halfHeight = height * 0.45f;

    juce::Path waveformPath;
    waveformPath.startNewSubPath(0.0f, static_cast<float>(centerY));

    for (int x = 0; x < width; ++x) {
        float phase = static_cast<float>(x) / static_cast<float>(width - 1);
        int idx = static_cast<int>(phase * WavetableBank::kFrameSize);
        float value = (idx < static_cast<int>(customWaveform_.size()))
                          ? customWaveform_[idx]
                          : 0.0f;
        float y = static_cast<float>(centerY) - value * halfHeight;
        waveformPath.lineTo(static_cast<float>(x), y);
    }

    // 绘制波形
    g.setColour(juce::Colour(0, 200, 255));
    g.strokePath(waveformPath, juce::PathStrokeType(2.0f));

    // 预览位置指示器
    if (previewing_) {
        float previewX = static_cast<float>(previewAngle_ / juce::MathConstants<double>::twoPi * width);
        g.setColour(juce::Colours::red);
        g.drawLine(previewX, static_cast<float>(bounds.getY()),
                   previewX, static_cast<float>(bounds.getBottom()), 1.0f);
    }

    // 边界
    g.setColour(juce::Colours::grey);
    g.drawRect(bounds, 1);
}

void WavetableEditor::drawGrid(juce::Graphics& g, const juce::Rectangle<int>& bounds) {
    g.setColour(juce::Colour(50, 50, 50));

    int centerY = bounds.getCentreY();
    // 水平中线
    g.drawLine(0.0f, static_cast<float>(centerY),
               static_cast<float>(bounds.getWidth()), static_cast<float>(centerY), 1.0f);

    // 垂直网格线
    for (int x = 0; x < bounds.getWidth(); x += bounds.getWidth() / 4) {
        g.drawLine(static_cast<float>(x), static_cast<float>(bounds.getY()),
                   static_cast<float>(x), static_cast<float>(bounds.getBottom()), 0.5f);
    }
}

void WavetableEditor::drawHarmonicBars(juce::Graphics& g, const juce::Rectangle<int>& bounds) {
    g.setColour(juce::Colour(30, 30, 30));
    g.fillRect(bounds);

    int startHarmonic = static_cast<int>(harmonicScrollBar_->getCurrentRangeStart());
    float barWidth = static_cast<float>(bounds.getWidth()) / static_cast<float>(kVisibleHarmonics);
    float maxBarHeight = static_cast<float>(bounds.getHeight()) * 0.9f;

    for (int i = 0; i < kVisibleHarmonics; ++i) {
        int h = startHarmonic + i;
        if (h >= kMaxHarmonics) break;

        float amplitude = harmonics_[h];
        float barHeight = amplitude * maxBarHeight;

        // 谐波条
        g.setColour((h == 0) ? juce::Colour(255, 180, 50)
                    : (h % 2 == 0) ? juce::Colour(100, 180, 255)
                    : juce::Colour(80, 140, 200));
        float x = static_cast<float>(bounds.getX()) + i * barWidth + 2;
        float y = static_cast<float>(bounds.getBottom()) - barHeight;
        g.fillRect(x, y, barWidth - 4, barHeight);

        // 编号
        g.setColour(juce::Colours::grey);
        g.setFont(9.0f);
        g.drawText(juce::String(h + 1),
                   static_cast<int>(x), bounds.getBottom() - 12,
                   static_cast<int>(barWidth - 4), 12,
                   juce::Justification::centred, false);
    }

    // 边界
    g.setColour(juce::Colours::grey);
    g.drawRect(bounds, 1);
}

// =============================================================================
// 布局
// =============================================================================
void WavetableEditor::resized() {
    auto area = getLocalBounds().reduced(4);
    auto topBar = area.removeFromTop(28);

    // 顶部按钮栏
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

    // 波形区域(上半部分)
    waveformBounds_ = area.removeFromTop(area.getHeight() * 2 / 3);

    area.removeFromTop(4);

    // 谐波区域(下半部分)
    harmonicBounds_ = area;

    // 谐波滑块布局
    if (editMode_ == EditMode::Harmonic) {
        float sliderWidth = static_cast<float>(harmonicBounds_.getWidth()) / static_cast<float>(kVisibleHarmonics);
        for (int i = 0; i < kVisibleHarmonics; ++i) {
            int x = harmonicBounds_.getX() + static_cast<int>(i * sliderWidth);
            harmonicSliders_[i].setBounds(x + 2, harmonicBounds_.getY() + 14,
                                          static_cast<int>(sliderWidth - 4),
                                          harmonicBounds_.getHeight() - 28);
            harmonicLabels_[i].setBounds(x + 2, harmonicBounds_.getBottom() - 14,
                                         static_cast<int>(sliderWidth - 4), 12);
        }
    }
}

// =============================================================================
// 鼠标交互 - 自由绘制
// =============================================================================
void WavetableEditor::mouseDown(const juce::MouseEvent& e) {
    if (editMode_ == EditMode::FreeDraw && waveformBounds_.contains(e.getPosition())) {
        isDrawing_ = true;
        handleFreeDraw(e);
    } else if (editMode_ == EditMode::Harmonic && harmonicBounds_.contains(e.getPosition())) {
        // 在谐波条区域拖拽
        handleHarmonicDrag(e);
    }
}

void WavetableEditor::mouseDrag(const juce::MouseEvent& e) {
    if (isDrawing_ && editMode_ == EditMode::FreeDraw) {
        handleFreeDraw(e);
    } else if (editMode_ == EditMode::Harmonic && harmonicBounds_.contains(e.getPosition())) {
        handleHarmonicDrag(e);
    }
}

void WavetableEditor::mouseUp(const juce::MouseEvent& e) {
    if (isDrawing_) {
        isDrawing_ = false;
        finishDrawing();
    }
    (void)e;
}

void WavetableEditor::handleFreeDraw(const juce::MouseEvent& e) {
    if (!waveformBounds_.contains(e.getPosition())) return;

    float relativeX = static_cast<float>(e.getPosition().x - waveformBounds_.getX()) /
                      static_cast<float>(waveformBounds_.getWidth());
    float relativeY = 1.0f - static_cast<float>(e.getPosition().y - waveformBounds_.getY()) /
                              static_cast<float>(waveformBounds_.getHeight());

    // 映射到波形数组
    int idx = static_cast<int>(relativeX * WavetableBank::kFrameSize);
    float value = (relativeY - 0.5f) * 2.0f; // 映射到 [-1, 1]

    if (idx >= 0 && idx < static_cast<int>(customWaveform_.size())) {
        // 平滑绘制: 影响周围几个采样点
        int radius = 8;
        for (int i = std::max(0, idx - radius);
             i < std::min(static_cast<int>(customWaveform_.size()), idx + radius); ++i) {
            float weight = 1.0f - std::abs(static_cast<float>(i - idx)) / static_cast<float>(radius);
            customWaveform_[i] = customWaveform_[i] * (1.0f - weight * 0.5f) + value * weight * 0.5f;
        }
    }

    waveformDirty_ = false;
    repaint();
}

void WavetableEditor::handleHarmonicDrag(const juce::MouseEvent& e) {
    int startHarmonic = static_cast<int>(harmonicScrollBar_->getCurrentRangeStart());
    float barWidth = static_cast<float>(harmonicBounds_.getWidth()) / static_cast<float>(kVisibleHarmonics);
    int barIndex = static_cast<int>((e.getPosition().x - harmonicBounds_.getX()) / barWidth);
    int harmonicIndex = startHarmonic + barIndex;

    if (harmonicIndex >= 0 && harmonicIndex < kMaxHarmonics) {
        float relativeY = 1.0f - static_cast<float>(e.getPosition().y - harmonicBounds_.getY()) /
                                  static_cast<float>(harmonicBounds_.getHeight());
        float amplitude = juce::jlimit(0.0f, 1.0f, relativeY);
        setHarmonic(harmonicIndex, amplitude);
    }
}

void WavetableEditor::finishDrawing() {
    smoothWaveform();
    if (bank_) {
        bank_->setFrameData(0, customWaveform_.data(), WavetableBank::kFrameSize);
        bank_->setNumFrames(1);
    }

    // 从绘制的波形提取谐波
    WaveformAnalyzer::analyze(customWaveform_.data(), WavetableBank::kFrameSize,
                               harmonics_.data(), kMaxHarmonics);

    // 更新滑块
    for (int i = 0; i < kVisibleHarmonics; ++i) {
        harmonicSliders_[i].setValue(harmonics_[i], juce::dontSendNotification);
    }

    waveformDirty_ = true;
    repaint();
}

// =============================================================================
// 波形生成
// =============================================================================
void WavetableEditor::generateWaveform() {
    int size = WavetableBank::kFrameSize;
    customWaveform_.resize(size, 0.0f);

    for (int i = 0; i < size; ++i) {
        float phase = static_cast<float>(i) / static_cast<float>(size);
        float value = 0.0f;

        for (int h = 0; h < kMaxHarmonics; ++h) {
            if (harmonics_[h] > 0.001f) {
                value += harmonics_[h] * std::sin(phase * (h + 1) * juce::MathConstants<float>::twoPi);
            }
        }

        // 归一化
        float norm = 0.0f;
        for (int h = 0; h < kMaxHarmonics; ++h) {
            norm += harmonics_[h];
        }
        if (norm > 0.001f) {
            value /= norm;
        }

        customWaveform_[i] = value;
    }

    waveformDirty_ = false;
}

void WavetableEditor::smoothWaveform() {
    // 简单移动平均平滑
    std::vector<float> smoothed(customWaveform_.size());
    for (size_t i = 0; i < customWaveform_.size(); ++i) {
        float sum = 0.0f;
        int count = 0;
        for (int j = -2; j <= 2; ++j) {
            int idx = static_cast<int>(i) + j;
            if (idx >= 0 && idx < static_cast<int>(customWaveform_.size())) {
                sum += customWaveform_[idx];
                count++;
            }
        }
        smoothed[i] = sum / count;
    }
    customWaveform_ = std::move(smoothed);
}

// =============================================================================
// Slider::Listener
// =============================================================================
void WavetableEditor::sliderValueChanged(juce::Slider* slider) {
    for (int i = 0; i < kVisibleHarmonics; ++i) {
        if (slider == &harmonicSliders_[i]) {
            int startHarmonic = static_cast<int>(harmonicScrollBar_->getCurrentRangeStart());
            int harmonicIndex = startHarmonic + i;
            if (harmonicIndex < kMaxHarmonics) {
                harmonics_[harmonicIndex] = static_cast<float>(slider->getValue());
                waveformDirty_ = true;
                applyHarmonics();
                repaint();
            }
            break;
        }
    }
}

// =============================================================================
// ScrollBar::Listener
// =============================================================================
void WavetableEditor::scrollBarMoved(juce::ScrollBar*, double) {
    resized();
    repaint();
}

// =============================================================================
// WaveformAnalyzer - 从波形提取谐波
// =============================================================================
namespace WaveformAnalyzer {
    void analyze(const float* waveform, int size, float* harmonics, int numHarmonics) {
        // 使用简单的DFT提取谐波振幅
        // 仅计算谐波频率处的幅度(基频 = samplerate / size)
        for (int h = 0; h < numHarmonics; ++h) {
            float real = 0.0f;
            float imag = 0.0f;

            for (int i = 0; i < size; ++i) {
                float phase = static_cast<float>(i) * (h + 1) * juce::MathConstants<float>::twoPi / static_cast<float>(size);
                real += waveform[i] * std::cos(phase);
                imag -= waveform[i] * std::sin(phase);
            }

            real /= static_cast<float>(size);
            imag /= static_cast<float>(size);
            harmonics[h] = std::sqrt(real * real + imag * imag) * 2.0f;
        }
    }
}

} // namespace LianCore