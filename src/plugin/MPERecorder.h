// LianCore V3 - MPE录制与回放系统
// 将MPE演奏录制为标准MIDI文件，回放时保留MPE数据
// 基于MIDI Polyphonic Expression (MPE) 规范
// 每个音符使用独立MIDI通道，保留弯音/触后等维度数据
#pragma once
#include <JuceHeader.h>
#include <vector>
#include <map>
#include <set>
#include <string>

namespace LianCore {

// MPE事件结构 - 封装所有MPE相关的MIDI事件
struct MPEEvent {
    enum Type {
        NoteOn = 0,
        NoteOff,
        PitchBend,
        ChannelPressure,
        PolyPressure,
        Timbre,
        CC74
    };

    Type type = NoteOn;
    int noteNumber = 60;
    int channel = 0;           // 0-15, 每个音符独占一个通道
    int value = 0;             // 弯音值/触后值/力度值
    double timestamp = 0.0;    // 秒为单位的时间戳

    juce::MidiMessage toMidiMessage() const {
        juce::MidiMessage msg;
        switch (type) {
            case NoteOn:
                msg = juce::MidiMessage::noteOn(channel + 1, noteNumber, (uint8_t)value);
                break;
            case NoteOff:
                msg = juce::MidiMessage::noteOff(channel + 1, noteNumber);
                break;
            case PitchBend:
                msg = juce::MidiMessage::pitchWheel(channel + 1, value);
                break;
            case ChannelPressure:
                msg = juce::MidiMessage::channelPressureChange(channel + 1, value);
                break;
            case PolyPressure:
                msg = juce::MidiMessage::aftertouchChange(channel + 1, noteNumber, value);
                break;
            case Timbre:
                // MPE Timbre = CC74
                msg = juce::MidiMessage::controllerEvent(channel + 1, 74, value);
                break;
            case CC74:
                msg = juce::MidiMessage::controllerEvent(channel + 1, 74, value);
                break;
        }
        return msg;
    }

    juce::var toJson() const {
        auto obj = new juce::DynamicObject();
        obj->setProperty("type", (int)type);
        obj->setProperty("noteNumber", noteNumber);
        obj->setProperty("channel", channel);
        obj->setProperty("value", value);
        obj->setProperty("timestamp", timestamp);
        return juce::var(obj);
    }

    static MPEEvent fromJson(const juce::var& json) {
        MPEEvent evt;
        if (auto* obj = json.getDynamicObject()) {
            evt.type = (Type)(int)obj->getProperty("type");
            evt.noteNumber = obj->getProperty("noteNumber");
            evt.channel = obj->getProperty("channel");
            evt.value = obj->getProperty("value");
            evt.timestamp = (double)obj->getProperty("timestamp");
        }
        return evt;
    }
};

// MPE录制器 - 从MIDI缓冲区捕获MPE事件并存储
class MPERecorder {
public:
    MPERecorder() : sampleRate_(44100.0) {}

    // 处理音频块，从MIDI缓冲区提取事件
    void processBlock(juce::MidiBuffer& midiBuffer);

    // 开始/停止录制
    void startRecording() { recording_ = true; events_.clear(); totalSamples_ = 0.0; activeNotes_.clear(); }
    void stopRecording() { recording_ = false; }
    bool isRecording() const { return recording_; }

    // 保存为标准MIDI文件（.mid），包含MPE元数据
    bool saveToMidiFile(const juce::File& file);

    // 保存为JSON文件（可读格式，便于调试）
    bool saveToJsonFile(const juce::File& file);

    // 从JSON文件加载
    bool loadFromJsonFile(const juce::File& file);

    // 获取录制的事件列表
    const std::vector<MPEEvent>& getEvents() const { return events_; }
    std::vector<MPEEvent>& getEvents() { return events_; }

    // 事件数量
    int getEventCount() const { return (int)events_.size(); }

    // 不同音符数
    int getNoteCount() const;

    // 录制时长（秒）
    double getDuration() const { return events_.empty() ? 0.0 : events_.back().timestamp; }

    // 设置采样率
    void setSampleRate(double sr) { sampleRate_ = sr; }

    // 清除所有事件
    void clear() { events_.clear(); activeNotes_.clear(); totalSamples_ = 0.0; }

    // 添加单个事件（用于手动编程MPE序列）
    void addEvent(const MPEEvent& evt) { events_.push_back(evt); }

private:
    // 从MIDI消息添加事件
    void addEvent(const juce::MidiMessage& msg);

    std::vector<MPEEvent> events_;
    std::map<int, int> activeNotes_;  // noteNumber -> channel
    double sampleRate_;
    double totalSamples_ = 0.0;
    bool recording_ = false;
};

// MPE播放器 - 从录制的事件序列回放MPE MIDI
class MPEPlayer {
public:
    MPEPlayer() : sampleRate_(44100.0) {}

    // 处理音频块，将事件注入MIDI缓冲区
    void processBlock(juce::MidiBuffer& midiBuffer, int numSamples);

    // 加载事件
    void loadEvents(const std::vector<MPEEvent>& events) {
        events_ = events;
        // 按时间戳排序
        std::sort(events_.begin(), events_.end(),
            [](const MPEEvent& a, const MPEEvent& b) { return a.timestamp < b.timestamp; });
    }

    // 开始/停止播放
    void startPlayback() { playing_ = true; nextEventIndex_ = 0; playbackPosition_ = 0.0; }
    void stopPlayback() { playing_ = false; }
    bool isPlaying() const { return playing_; }

    // 设置播放位置（秒）
    void seekTo(double position) {
        playbackPosition_ = position;
        nextEventIndex_ = 0;
        while (nextEventIndex_ < events_.size() &&
               events_[nextEventIndex_].timestamp < playbackPosition_) {
            nextEventIndex_++;
        }
    }

    // 获取进度
    double getPlaybackPosition() const { return playbackPosition_; }
    double getTotalDuration() const { return events_.empty() ? 0.0 : events_.back().timestamp; }

    // 设置采样率
    void setSampleRate(double sr) { sampleRate_ = sr; }

    // 事件数量
    int getEventCount() const { return (int)events_.size(); }

    // 加载MIDI文件
    bool loadFromMidiFile(const juce::File& file);

    // 循环播放
    void setLooping(bool loop) { looping_ = loop; }
    bool isLooping() const { return looping_; }

private:
    std::vector<MPEEvent> events_;
    double sampleRate_;
    double playbackPosition_ = 0.0;
    size_t nextEventIndex_ = 0;
    bool playing_ = false;
    bool looping_ = false;
};

} // namespace LianCore
