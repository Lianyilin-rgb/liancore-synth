// =============================================================================
// LianCore - MPERecordingUI MPE录制与回放UI组件 (P6-1)
// 提供录音/播放/保存/加载 MIDI 文件的完整界面
// 包含钢琴卷帘事件时间线可视化
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include "../plugin/MPERecorder.h"

namespace LianCore {

// =============================================================================
// MPE事件时间线可视化组件
// =============================================================================
class MPEEventTimeline : public juce::Component {
public:
    MPEEventTimeline();
    ~MPEEventTimeline() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // 设置要显示的事件数据
    void setEvents(const std::vector<MPEEvent>* events);
    // 设置播放位置 (0.0 ~ 1.0)
    void setPlaybackPosition(double pos);

    // 颜色配置
    void setColors(juce::Colour bg, juce::Colour noteOn, juce::Colour noteOff,
                   juce::Colour pitchBend, juce::Colour pressure, juce::Colour playhead);

private:
    void drawGrid(juce::Graphics& g, const juce::Rectangle<int>& bounds);
    void drawEvents(juce::Graphics& g, const juce::Rectangle<int>& bounds);
    void drawPlayhead(juce::Graphics& g, const juce::Rectangle<int>& bounds);

    const std::vector<MPEEvent>* events_ = nullptr;
    double playbackPosition_ = 0.0;

    // 颜色
    juce::Colour bgColor_         = juce::Colour(0xff1a1a2e);
    juce::Colour noteOnColor_     = juce::Colour(0xff00d4ff);
    juce::Colour noteOffColor_    = juce::Colour(0xff6c5ce7);
    juce::Colour pitchBendColor_  = juce::Colour(0xff00cec9);
    juce::Colour pressureColor_   = juce::Colour(0xffff6b6b);
    juce::Colour playheadColor_   = juce::Colour(0xffffffff);
    juce::Colour gridColor_       = juce::Colour(0x20ffffff);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MPEEventTimeline)
};

// =============================================================================
// MPERecordingUI - MPE录制与回放主界面
// =============================================================================
class MPERecordingUI : public juce::Component,
                       public juce::Timer {
public:
    // 回调类型
    using RecorderRef = std::function<MPERecorder*()>;
    using PlayerRef = std::function<MPEPlayer*()>;

    MPERecordingUI();
    ~MPERecordingUI() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    // 设置录制器和播放器引用
    void setRecorderRef(RecorderRef ref) { recorderRef_ = std::move(ref); }
    void setPlayerRef(PlayerRef ref) { playerRef_ = std::move(ref); }

    // 启动/停止 UI 定时刷新
    void startUIUpdates(int fps = 15);
    void stopUIUpdates();

private:
    // 按钮回调
    void onRecordToggle();
    void onPlayToggle();
    void onStop();
    void onSaveMidi();
    void onSaveJson();
    void onLoadMidi();
    void onLoadJson();
    void onLoopToggle();
    void onClear();

    // 更新状态显示
    void updateStatusDisplay();

    // 获取当前录制器/播放器
    MPERecorder* getRecorder();
    MPEPlayer* getPlayer();

    // 引用回调
    RecorderRef recorderRef_;
    PlayerRef playerRef_;

    // 按钮
    juce::TextButton recordButton_;
    juce::TextButton playButton_;
    juce::TextButton stopButton_;
    juce::TextButton saveMidiButton_;
    juce::TextButton saveJsonButton_;
    juce::TextButton loadMidiButton_;
    juce::TextButton loadJsonButton_;
    juce::TextButton loopButton_;
    juce::TextButton clearButton_;

    // 状态标签
    juce::Label statusLabel_;
    juce::Label eventCountLabel_;
    juce::Label durationLabel_;
    juce::Label noteCountLabel_;

    // 事件时间线
    MPEEventTimeline timeline_;

    // 循环状态
    bool looping_ = false;

    // 颜色配置
    juce::Colour bgColor_       = juce::Colour(0xff0a0a0f);
    juce::Colour panelColor_    = juce::Colour(0xff1a1a2e);
    juce::Colour accentColor_   = juce::Colour(0xff00d4ff);
    juce::Colour textColor_     = juce::Colour(0xffe0e0e0);
    juce::Colour recordColor_   = juce::Colour(0xffff4757);
    juce::Colour playColor_     = juce::Colour(0xff2ed573);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MPERecordingUI)
};

} // namespace LianCore