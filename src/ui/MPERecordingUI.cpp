// =============================================================================
// LianCore - MPERecordingUI 实现 (P6-1)
// MPE录制与回放UI组件，包含事件时间线可视化
// =============================================================================
#include "MPERecordingUI.h"

namespace LianCore {

// =============================================================================
// MPEEventTimeline 实现
// =============================================================================

MPEEventTimeline::MPEEventTimeline() {
    setInterceptsMouseClicks(false, false);
}

void MPEEventTimeline::setEvents(const std::vector<MPEEvent>* events) {
    events_ = events;
    repaint();
}

void MPEEventTimeline::setPlaybackPosition(double pos) {
    playbackPosition_ = juce::jlimit(0.0, 1.0, pos);
    repaint();
}

void MPEEventTimeline::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    g.fillAll(bgColor_);

    drawGrid(g, bounds);
    drawEvents(g, bounds);
    drawPlayhead(g, bounds);
}

void MPEEventTimeline::resized() {
    repaint();
}

void MPEEventTimeline::drawGrid(juce::Graphics& g, const juce::Rectangle<int>& bounds) {
    g.setColour(gridColor_);

    // 水平线 (MIDI音符分组)
    int noteRange = 128;
    float rowHeight = (float)bounds.getHeight() / 12.0f; // 按八度分组
    for (int octave = 0; octave < 11; ++octave) {
        float y = bounds.getY() + octave * rowHeight;
        if (octave == 5) {
            g.setColour(gridColor_.withAlpha(0.5f));
            g.drawLine((float)bounds.getX(), y, (float)bounds.getRight(), y, 1.0f);
            g.setColour(gridColor_);
        } else {
            g.drawLine((float)bounds.getX(), y, (float)bounds.getRight(), y, 0.5f);
        }
    }

    // 垂直线 (时间标记)
    int numVerticalLines = 10;
    for (int i = 0; i <= numVerticalLines; ++i) {
        float x = bounds.getX() + (float)bounds.getWidth() * i / (float)numVerticalLines;
        g.drawLine(x, (float)bounds.getY(), x, (float)bounds.getBottom(), 0.5f);
    }
}

void MPEEventTimeline::drawEvents(juce::Graphics& g, const juce::Rectangle<int>& bounds) {
    if (!events_ || events_->empty()) return;

    float totalDuration = events_->back().timestamp;
    if (totalDuration <= 0.0f) return;

    float noteRange = 128.0f;
    float left = (float)bounds.getX();
    float right = (float)bounds.getRight();
    float top = (float)bounds.getY();
    float bottom = (float)bounds.getBottom();

    for (const auto& evt : *events_) {
        float x = left + (float)(evt.timestamp / totalDuration) * (right - left);
        float y = bottom - (float)(evt.noteNumber / noteRange) * (bottom - top);

        juce::Colour color;
        float radius = 2.5f;

        switch (evt.type) {
            case MPEEvent::NoteOn:
                color = noteOnColor_;
                radius = 3.0f;
                break;
            case MPEEvent::NoteOff:
                color = noteOffColor_;
                radius = 2.0f;
                break;
            case MPEEvent::PitchBend:
                color = pitchBendColor_;
                radius = 1.5f;
                break;
            case MPEEvent::ChannelPressure:
            case MPEEvent::PolyPressure:
                color = pressureColor_;
                radius = 1.5f;
                break;
            default:
                color = juce::Colour(0xff888888);
                radius = 1.5f;
                break;
        }

        g.setColour(color);
        g.fillEllipse(x - radius, y - radius, radius * 2.0f, radius * 2.0f);
    }
}

void MPEEventTimeline::drawPlayhead(juce::Graphics& g, const juce::Rectangle<int>& bounds) {
    if (playbackPosition_ <= 0.0) return;

    float x = bounds.getX() + (float)playbackPosition_ * (float)bounds.getWidth();
    g.setColour(playheadColor_);
    g.drawLine(x, (float)bounds.getY(), x, (float)bounds.getBottom(), 2.0f);

    // 播放头三角形指示器
    juce::Path triangle;
    triangle.addTriangle(x, (float)bounds.getY(),
                         x - 5.0f, (float)bounds.getY() - 8.0f,
                         x + 5.0f, (float)bounds.getY() - 8.0f);
    g.fillPath(triangle);
}

void MPEEventTimeline::setColors(juce::Colour bg, juce::Colour noteOn, juce::Colour noteOff,
                                  juce::Colour pitchBend, juce::Colour pressure, juce::Colour playhead) {
    bgColor_ = bg;
    noteOnColor_ = noteOn;
    noteOffColor_ = noteOff;
    pitchBendColor_ = pitchBend;
    pressureColor_ = pressure;
    playheadColor_ = playhead;
    repaint();
}

// =============================================================================
// MPERecordingUI 实现
// =============================================================================

MPERecordingUI::MPERecordingUI() {
    // ---- 录制按钮 ----
    recordButton_.setButtonText("录制");
    recordButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a3a));
    recordButton_.setColour(juce::TextButton::buttonOnColourId, recordColor_);
    recordButton_.onClick = [this]() { onRecordToggle(); };
    addAndMakeVisible(recordButton_);

    // ---- 播放按钮 ----
    playButton_.setButtonText("播放");
    playButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a3a));
    playButton_.setColour(juce::TextButton::buttonOnColourId, playColor_);
    playButton_.onClick = [this]() { onPlayToggle(); };
    addAndMakeVisible(playButton_);

    // ---- 停止按钮 ----
    stopButton_.setButtonText("停止");
    stopButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a3a));
    stopButton_.onClick = [this]() { onStop(); };
    addAndMakeVisible(stopButton_);

    // ---- 保存/加载按钮 ----
    saveMidiButton_.setButtonText("保存 MIDI");
    saveMidiButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a3a));
    saveMidiButton_.onClick = [this]() { onSaveMidi(); };
    addAndMakeVisible(saveMidiButton_);

    saveJsonButton_.setButtonText("保存 JSON");
    saveJsonButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a3a));
    saveJsonButton_.onClick = [this]() { onSaveJson(); };
    addAndMakeVisible(saveJsonButton_);

    loadMidiButton_.setButtonText("加载 MIDI");
    loadMidiButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a3a));
    loadMidiButton_.onClick = [this]() { onLoadMidi(); };
    addAndMakeVisible(loadMidiButton_);

    loadJsonButton_.setButtonText("加载 JSON");
    loadJsonButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a3a));
    loadJsonButton_.onClick = [this]() { onLoadJson(); };
    addAndMakeVisible(loadJsonButton_);

    // ---- 循环按钮 ----
    loopButton_.setButtonText("循环: 关");
    loopButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a3a));
    loopButton_.onClick = [this]() { onLoopToggle(); };
    addAndMakeVisible(loopButton_);

    // ---- 清除按钮 ----
    clearButton_.setButtonText("清除");
    clearButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a3a));
    clearButton_.onClick = [this]() { onClear(); };
    addAndMakeVisible(clearButton_);

    // ---- 状态标签 ----
    statusLabel_.setText("就绪", juce::dontSendNotification);
    statusLabel_.setFont(juce::Font(12.0f, juce::Font::bold));
    statusLabel_.setColour(juce::Label::textColourId, accentColor_);
    statusLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLabel_);

    eventCountLabel_.setText("事件: 0", juce::dontSendNotification);
    eventCountLabel_.setFont(juce::Font(10.0f));
    eventCountLabel_.setColour(juce::Label::textColourId, textColor_);
    addAndMakeVisible(eventCountLabel_);

    durationLabel_.setText("时长: 0.0s", juce::dontSendNotification);
    durationLabel_.setFont(juce::Font(10.0f));
    durationLabel_.setColour(juce::Label::textColourId, textColor_);
    addAndMakeVisible(durationLabel_);

    noteCountLabel_.setText("音符: 0", juce::dontSendNotification);
    noteCountLabel_.setFont(juce::Font(10.0f));
    noteCountLabel_.setColour(juce::Label::textColourId, textColor_);
    addAndMakeVisible(noteCountLabel_);

    // ---- 事件时间线 ----
    addAndMakeVisible(timeline_);
}

MPERecordingUI::~MPERecordingUI() {
    stopUIUpdates();
}

void MPERecordingUI::paint(juce::Graphics& g) {
    g.fillAll(bgColor_);

    // 面板边框
    g.setColour(panelColor_);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 4.0f, 1.0f);
}

void MPERecordingUI::resized() {
    auto area = getLocalBounds().reduced(8);

    // 第一行：控制按钮
    auto controlRow = area.removeFromTop(28);
    int btnWidth = 60;
    recordButton_.setBounds(controlRow.removeFromLeft(btnWidth).reduced(2, 2));
    controlRow.removeFromLeft(4);
    playButton_.setBounds(controlRow.removeFromLeft(btnWidth).reduced(2, 2));
    controlRow.removeFromLeft(4);
    stopButton_.setBounds(controlRow.removeFromLeft(btnWidth).reduced(2, 2));
    controlRow.removeFromLeft(8);
    loopButton_.setBounds(controlRow.removeFromLeft(70).reduced(2, 2));
    controlRow.removeFromLeft(8);
    clearButton_.setBounds(controlRow.removeFromLeft(50).reduced(2, 2));

    area.removeFromTop(4);

    // 第二行：保存/加载按钮
    auto ioRow = area.removeFromTop(26);
    int ioBtnWidth = 72;
    saveMidiButton_.setBounds(ioRow.removeFromLeft(ioBtnWidth).reduced(2, 2));
    ioRow.removeFromLeft(2);
    saveJsonButton_.setBounds(ioRow.removeFromLeft(ioBtnWidth).reduced(2, 2));
    ioRow.removeFromLeft(2);
    loadMidiButton_.setBounds(ioRow.removeFromLeft(ioBtnWidth).reduced(2, 2));
    ioRow.removeFromLeft(2);
    loadJsonButton_.setBounds(ioRow.removeFromLeft(ioBtnWidth).reduced(2, 2));

    area.removeFromTop(4);

    // 第三行：状态标签
    auto statusRow = area.removeFromTop(18);
    statusLabel_.setBounds(statusRow.removeFromLeft(80));
    eventCountLabel_.setBounds(statusRow.removeFromLeft(80));
    durationLabel_.setBounds(statusRow.removeFromLeft(80));
    noteCountLabel_.setBounds(statusRow);

    area.removeFromTop(4);

    // 第四行：事件时间线
    timeline_.setBounds(area);
}

void MPERecordingUI::timerCallback() {
    updateStatusDisplay();

    // 更新播放位置
    auto* player = getPlayer();
    if (player && player->isPlaying()) {
        double totalDuration = player->getTotalDuration();
        if (totalDuration > 0.0) {
            timeline_.setPlaybackPosition(player->getPlaybackPosition() / totalDuration);
        }
    }

    // 更新按钮状态
    auto* recorder = getRecorder();
    if (recorder) {
        if (recorder->isRecording()) {
            recordButton_.setButtonText("停止录制");
            recordButton_.setColour(juce::TextButton::buttonColourId, recordColor_);
        } else {
            recordButton_.setButtonText("录制");
            recordButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a3a));
        }
    }

    if (player) {
        if (player->isPlaying()) {
            playButton_.setButtonText("暂停");
            playButton_.setColour(juce::TextButton::buttonColourId, playColor_);
        } else {
            playButton_.setButtonText("播放");
            playButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a3a));
        }
    }
}

void MPERecordingUI::startUIUpdates(int fps) {
    startTimerHz(fps);
}

void MPERecordingUI::stopUIUpdates() {
    stopTimer();
}

MPERecorder* MPERecordingUI::getRecorder() {
    return recorderRef_ ? recorderRef_() : nullptr;
}

MPEPlayer* MPERecordingUI::getPlayer() {
    return playerRef_ ? playerRef_() : nullptr;
}

void MPERecordingUI::updateStatusDisplay() {
    auto* recorder = getRecorder();
    auto* player = getPlayer();

    // 优先显示录制器的事件（如果有录制数据）
    const std::vector<MPEEvent>* events = nullptr;
    double duration = 0.0;
    int eventCount = 0;
    int noteCount = 0;

    if (recorder && recorder->getEventCount() > 0) {
        events = &recorder->getEvents();
        duration = recorder->getDuration();
        eventCount = recorder->getEventCount();
        noteCount = recorder->getNoteCount();
        timeline_.setEvents(events);
    } else if (player && player->getEventCount() > 0) {
        // 从播放器获取已加载的事件
        events = nullptr; // 播放器没有直接暴露事件列表
        duration = player->getTotalDuration();
        eventCount = player->getEventCount();
        noteCount = 0;
    }

    if (recorder && recorder->isRecording()) {
        statusLabel_.setText("录制中...", juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, recordColor_);
    } else if (player && player->isPlaying()) {
        statusLabel_.setText("播放中...", juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, playColor_);
    } else {
        statusLabel_.setText("就绪", juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, accentColor_);
    }

    eventCountLabel_.setText("事件: " + juce::String(eventCount), juce::dontSendNotification);
    durationLabel_.setText("时长: " + juce::String(duration, 1) + "s", juce::dontSendNotification);
    noteCountLabel_.setText("音符: " + juce::String(noteCount), juce::dontSendNotification);
}

// =============================================================================
// 按钮回调
// =============================================================================

void MPERecordingUI::onRecordToggle() {
    auto* recorder = getRecorder();
    if (!recorder) return;

    if (recorder->isRecording()) {
        recorder->stopRecording();
        statusLabel_.setText("录制已停止", juce::dontSendNotification);
    } else {
        recorder->startRecording();
        statusLabel_.setText("录制中...", juce::dontSendNotification);
    }
    updateStatusDisplay();
}

void MPERecordingUI::onPlayToggle() {
    auto* player = getPlayer();
    auto* recorder = getRecorder();
    if (!player) return;

    if (player->isPlaying()) {
        player->stopPlayback();
        statusLabel_.setText("播放已暂停", juce::dontSendNotification);
        return;
    }

    // 如果录制器有数据，先加载到播放器
    if (recorder && recorder->getEventCount() > 0 && player->getEventCount() == 0) {
        player->loadEvents(recorder->getEvents());
    }

    if (player->getEventCount() == 0) {
        statusLabel_.setText("无事件可播放", juce::dontSendNotification);
        return;
    }

    player->startPlayback();
    player->setLooping(looping_);
    statusLabel_.setText("播放中...", juce::dontSendNotification);
}

void MPERecordingUI::onStop() {
    auto* recorder = getRecorder();
    auto* player = getPlayer();

    if (recorder && recorder->isRecording()) {
        recorder->stopRecording();
    }
    if (player && player->isPlaying()) {
        player->stopPlayback();
    }
    if (player) {
        player->seekTo(0.0);
    }
    timeline_.setPlaybackPosition(0.0);
    statusLabel_.setText("已停止", juce::dontSendNotification);
    updateStatusDisplay();
}

void MPERecordingUI::onSaveMidi() {
    auto* recorder = getRecorder();
    if (!recorder || recorder->getEventCount() == 0) {
        statusLabel_.setText("无事件可保存", juce::dontSendNotification);
        return;
    }

    auto chooser = std::make_unique<juce::FileChooser>(
        "保存 MPE MIDI 文件...",
        juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("MPE_Recording.mid"),
        "*.mid");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this, recorder](const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file != juce::File()) {
                if (recorder->saveToMidiFile(file)) {
                    statusLabel_.setText("已保存: " + file.getFileName(), juce::dontSendNotification);
                } else {
                    statusLabel_.setText("保存失败", juce::dontSendNotification);
                }
            }
        });
}

void MPERecordingUI::onSaveJson() {
    auto* recorder = getRecorder();
    if (!recorder || recorder->getEventCount() == 0) {
        statusLabel_.setText("无事件可保存", juce::dontSendNotification);
        return;
    }

    auto chooser = std::make_unique<juce::FileChooser>(
        "保存 MPE JSON 文件...",
        juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("MPE_Recording.json"),
        "*.json");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this, recorder](const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file != juce::File()) {
                if (recorder->saveToJsonFile(file)) {
                    statusLabel_.setText("已保存: " + file.getFileName(), juce::dontSendNotification);
                } else {
                    statusLabel_.setText("保存失败", juce::dontSendNotification);
                }
            }
        });
}

void MPERecordingUI::onLoadMidi() {
    auto* player = getPlayer();
    if (!player) return;

    auto chooser = std::make_unique<juce::FileChooser>(
        "加载 MPE MIDI 文件...",
        juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
        "*.mid");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, player](const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file != juce::File()) {
                if (player->loadFromMidiFile(file)) {
                    statusLabel_.setText("已加载: " + file.getFileName() + " (" +
                                         juce::String(player->getEventCount()) + " 事件)",
                                         juce::dontSendNotification);
                } else {
                    statusLabel_.setText("加载失败", juce::dontSendNotification);
                }
                updateStatusDisplay();
            }
        });
}

void MPERecordingUI::onLoadJson() {
    auto* recorder = getRecorder();
    if (!recorder) return;

    auto chooser = std::make_unique<juce::FileChooser>(
        "加载 MPE JSON 文件...",
        juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
        "*.json");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, recorder](const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file != juce::File()) {
                if (recorder->loadFromJsonFile(file)) {
                    auto* player = getPlayer();
                    if (player) {
                        player->loadEvents(recorder->getEvents());
                    }
                    statusLabel_.setText("已加载: " + file.getFileName() + " (" +
                                         juce::String(recorder->getEventCount()) + " 事件)",
                                         juce::dontSendNotification);
                } else {
                    statusLabel_.setText("加载失败", juce::dontSendNotification);
                }
                updateStatusDisplay();
            }
        });
}

void MPERecordingUI::onLoopToggle() {
    looping_ = !looping_;
    loopButton_.setButtonText(looping_ ? "循环: 开" : "循环: 关");

    auto* player = getPlayer();
    if (player) {
        player->setLooping(looping_);
    }
}

void MPERecordingUI::onClear() {
    auto* recorder = getRecorder();
    auto* player = getPlayer();

    if (recorder) recorder->clear();
    if (player) {
        player->stopPlayback();
        player->loadEvents({});
    }
    timeline_.setEvents(nullptr);
    timeline_.setPlaybackPosition(0.0);
    statusLabel_.setText("已清除", juce::dontSendNotification);
    updateStatusDisplay();
}

} // namespace LianCore