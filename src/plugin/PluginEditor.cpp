// =============================================================================
// LianCore - PluginEditor 实现
// =============================================================================
#include "PluginEditor.h"

namespace LianCore {

PluginEditor::PluginEditor(PluginProcessor& processor)
    : AudioProcessorEditor(&processor)
    , processor_(processor)
{
    // 设置窗口大小
    setSize(600, 400);

    // 标题
    titleLabel_.setText("LianCore V3 Alpha - 商业AI合成器软音源", juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(16.0f, juce::Font::bold));
    titleLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel_);

    // 状态
    statusLabel_.setText("节点: 波表振荡器 → 滤波器 → 输出", juce::dontSendNotification);
    statusLabel_.setFont(juce::Font(12.0f));
    statusLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel_);

    // CPU使用率
    cpuLabel_.setText("CPU: 0.0ms", juce::dontSendNotification);
    cpuLabel_.setFont(juce::Font(10.0f));
    addAndMakeVisible(cpuLabel_);

    // 测试按钮
    testButton_.setButtonText("测试音频");
    testButton_.onClick = [this]() {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "LianCore Alpha",
            "音频图引擎正常运行中\n节点数: " + juce::String(processor_.getAudioGraph().getNodeCount()),
            "确定"
        );
    };
    addAndMakeVisible(testButton_);

    // 启动定时器更新CPU显示
    startTimerHz(10);
}

PluginEditor::~PluginEditor() {
    stopTimer();
}

void PluginEditor::paint(juce::Graphics& g) {
    // 背景
    g.fillAll(juce::Colour(0xFF1a1a2e));

    // 分隔线
    g.setColour(juce::Colour(0xFFe94560));
    g.drawLine(50, 80, getWidth() - 50, 80, 2.0f);

    // 更新CPU显示
    cpuLabel_.setText(
        juce::String::formatted("CPU: %.2fms | 内存: %dKB",
            processor_.getCpuUsage(),
            static_cast<int>(processor_.getMemoryUsage() / 1024)),
        juce::dontSendNotification
    );
}

void PluginEditor::resized() {
    auto area = getLocalBounds().reduced(20);

    titleLabel_.setBounds(area.removeFromTop(40));
    area.removeFromTop(20);
    statusLabel_.setBounds(area.removeFromTop(25));
    area.removeFromTop(20);

    testButton_.setBounds(area.removeFromTop(40).withSizeKeepingCentre(120, 40));

    cpuLabel_.setBounds(10, getHeight() - 30, 200, 20);
}

} // namespace LianCore