// =============================================================================
// LianCore - WavetableEditor 可视化波表编辑器
// 支持Canvas绘制、拖拽调整谐波、自定义波形绘制、实时预览、AI生成
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include "../synthesis/WavetableBank.h"
#include <functional>

namespace LianCore {

// =============================================================================
// WaveformAnalyzer - 从波形提取谐波 (前向声明)
// =============================================================================
namespace WaveformAnalyzer {
    void analyze(const float* waveform, int size, float* harmonics, int numHarmonics);
}

// =============================================================================
// WavetableEditor - 波表编辑器组件
// =============================================================================
class WavetableEditor : public juce::Component,
                         public juce::Slider::Listener,
                         public juce::ScrollBar::Listener,
                         public juce::Timer {
public:
    WavetableEditor();
    ~WavetableEditor() override;

    // 设置要编辑的波表
    void setWavetableBank(WavetableBank* bank);
    WavetableBank* getWavetableBank() const { return bank_; }

    // 预览音频
    void startPreview();
    void stopPreview();

    // =========================================================================
    // AI 波表生成
    // =========================================================================
    // 基于文本描述，使用规则引擎生成谐波结构
    void aiGenerateFromText(const juce::String& text);
    // AI生成完成回调
    std::function<void()> onAIGenerateComplete;

    // =========================================================================
    // Component
    // =========================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    // =========================================================================
    // Slider::Listener
    // =========================================================================
    void sliderValueChanged(juce::Slider* slider) override;

    // =========================================================================
    // ScrollBar::Listener
    // =========================================================================
    void scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart) override;

    // =========================================================================
    // Timer
    // =========================================================================
    void timerCallback() override;

    // =========================================================================
    // 模式
    // =========================================================================
    enum class EditMode {
        Harmonic,   // 谐波编辑模式 (滑块调整)
        FreeDraw,   // 自由绘制模式
        Preset,     // 预设波形模式
        AIGenerate, // AI生成模式
    };

    void setEditMode(EditMode mode);
    EditMode getEditMode() const { return editMode_; }

    // 谐波操作
    void setHarmonic(int index, float amplitude);
    float getHarmonic(int index) const;
    void resetHarmonics();
    void applyHarmonics();

    // 预设波形
    void setPresetWaveform(const juce::String& name);

private:
    // 绘制波形路径
    void drawWaveform(juce::Graphics& g, const juce::Rectangle<int>& bounds);
    void drawGrid(juce::Graphics& g, const juce::Rectangle<int>& bounds);
    void drawHarmonicBars(juce::Graphics& g, const juce::Rectangle<int>& bounds);
    void drawAIPanel(juce::Graphics& g, const juce::Rectangle<int>& bounds);

    // 输入处理
    void handleFreeDraw(const juce::MouseEvent& e);
    void handleHarmonicDrag(const juce::MouseEvent& e);
    void finishDrawing();

    // 生成波形
    void generateWaveform();
    void smoothWaveform();

    // AI生成辅助
    void applyTextToHarmonics(const juce::String& text);
    void generateMultiFrameWavetable();

    // 数据
    WavetableBank* bank_ = nullptr;
    std::vector<float> harmonics_;       // 64个谐波振幅
    std::vector<float> customWaveform_;  // 拖拽绘制的波形
    EditMode editMode_ = EditMode::Harmonic;

    // 波形显示区域
    juce::Rectangle<int> waveformBounds_;
    juce::Rectangle<int> harmonicBounds_;
    juce::Rectangle<int> aiPanelBounds_;

    // 绘制状态
    bool isDrawing_ = false;
    bool waveformDirty_ = true;

    // 预览
    bool previewing_ = false;
    double previewAngle_ = 0.0;
    double sampleRate_ = 44100.0;

    // UI控件
    juce::TextButton modeButton_;
    juce::TextButton previewButton_;
    juce::TextButton resetButton_;
    juce::ComboBox presetCombo_;
    juce::Label harmonicsLabel_;
    juce::Label drawLabel_;

    // AI生成UI
    juce::TextEditor aiPromptInput_;
    juce::TextButton aiGenerateButton_;
    juce::Label aiStatusLabel_;

    // 谐波滑块
    static constexpr int kMaxHarmonics = 64;
    static constexpr int kVisibleHarmonics = 16;
    std::array<juce::Slider, kVisibleHarmonics> harmonicSliders_;
    std::array<juce::Label, kVisibleHarmonics> harmonicLabels_;

    // 滚动条
    std::unique_ptr<juce::ScrollBar> harmonicScrollBar_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WavetableEditor)
};

} // namespace LianCore