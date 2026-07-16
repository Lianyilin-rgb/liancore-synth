// =============================================================================
// LianCore - GranularEngineUI 粒子合成引擎UI组件 (P6-2)
// 提供粒子参数控制、包络可视化、方向控制、AI密度控制界面
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include "../synthesis/GranularPlayer.h"
#include "../ui/GrainEnvelopeVisualizer.h"

namespace LianCore {

// =============================================================================
// GranularEngineUI - 粒子合成引擎主界面
// =============================================================================
class GranularEngineUI : public juce::Component,
                         public juce::Slider::Listener,
                         public juce::ComboBox::Listener {
public:
    using PlayerRef = std::function<GranularPlayer*()>;

    GranularEngineUI();
    ~GranularEngineUI() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Slider::Listener
    void sliderValueChanged(juce::Slider* slider) override;

    // ComboBox::Listener
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;

    // 设置粒子播放器引用
    void setPlayerRef(PlayerRef ref) { playerRef_ = std::move(ref); }

    // 刷新UI参数
    void refreshParameters();

private:
    // 加载源音频文件
    void onLoadSourceFile();

    // 获取当前粒子播放器
    GranularPlayer* getPlayer();

    PlayerRef playerRef_;

    // 包络可视化组件
    GrainEnvelopeVisualizer envelopeVisualizer_;

    // 粒子参数滑块
    juce::Slider grainSizeSlider_;
    juce::Slider grainDensitySlider_;
    juce::Slider grainPositionSlider_;
    juce::Slider pitchRandomSlider_;
    juce::Slider panRandomSlider_;
    juce::Slider scatterSlider_;
    juce::Slider volumeSlider_;
    juce::Slider pitchShiftSlider_;

    // 滑块标签
    juce::Label grainSizeLabel_;
    juce::Label grainDensityLabel_;
    juce::Label grainPositionLabel_;
    juce::Label pitchRandomLabel_;
    juce::Label panRandomLabel_;
    juce::Label scatterLabel_;
    juce::Label volumeLabel_;
    juce::Label pitchShiftLabel_;

    // 包络类型选择器
    juce::ComboBox envelopeTypeCombo_;
    juce::Label envelopeTypeLabel_;

    // 方向控制
    juce::ComboBox directionCombo_;
    juce::Label directionLabel_;

    // AI密度控制
    juce::ToggleButton aiDensityToggle_;
    juce::Label aiDensityLabel_;

    // 加载源文件按钮
    juce::TextButton loadSourceButton_;
    juce::Label sourceStatusLabel_;

    // 状态标签
    juce::Label activeGrainLabel_;
    juce::Label cpuLoadLabel_;

    // 颜色配置
    juce::Colour bgColor_     = juce::Colour(0xff0a0a0f);
    juce::Colour panelColor_  = juce::Colour(0xff1a1a2e);
    juce::Colour accentColor_ = juce::Colour(0xff00d4ff);
    juce::Colour textColor_   = juce::Colour(0xffe0e0e0);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GranularEngineUI)
};

} // namespace LianCore