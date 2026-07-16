// LianCore V3 - MPERecorder MPE录制与回放实现
// 支持完整的MPE事件录制、MIDI文件导出、JSON序列化、实时回放
#include "MPERecorder.h"
#include <algorithm>

namespace LianCore {

// ==================== MPERecorder ====================

void MPERecorder::processBlock(juce::MidiBuffer& midiBuffer) {
    if (!recording_) return;
    
    for (const auto& meta : midiBuffer) {
        auto msg = meta.getMessage();
        addEvent(msg);
    }
    totalSamples_ += 1.0; // 每处理一个block累加采样数
}

void MPERecorder::addEvent(const juce::MidiMessage& msg) {
    if (msg.isNoteOn()) {
        int note = msg.getNoteNumber();
        int channel = msg.getChannel() - 1; // 转换为0-based通道
        
        // MPE规范：每个音符独占一个通道
        activeNotes_[note] = channel;
        
        MPEEvent evt;
        evt.type = MPEEvent::NoteOn;
        evt.noteNumber = note;
        evt.channel = channel;
        evt.value = msg.getVelocity();
        evt.timestamp = totalSamples_ / sampleRate_;
        events_.push_back(evt);
    } 
    else if (msg.isNoteOff()) {
        int note = msg.getNoteNumber();
        activeNotes_.erase(note);
        
        MPEEvent evt;
        evt.type = MPEEvent::NoteOff;
        evt.noteNumber = note;
        evt.channel = msg.getChannel() - 1;
        evt.timestamp = totalSamples_ / sampleRate_;
        events_.push_back(evt);
    } 
    else if (msg.isPitchWheel()) {
        MPEEvent evt;
        evt.type = MPEEvent::PitchBend;
        evt.channel = msg.getChannel() - 1;
        evt.value = msg.getPitchWheelValue();
        evt.timestamp = totalSamples_ / sampleRate_;
        events_.push_back(evt);
    } 
    else if (msg.isChannelPressure()) {
        MPEEvent evt;
        evt.type = MPEEvent::ChannelPressure;
        evt.channel = msg.getChannel() - 1;
        evt.value = msg.getChannelPressureValue();
        evt.timestamp = totalSamples_ / sampleRate_;
        events_.push_back(evt);
    } 
    else if (msg.isAftertouch()) {
        MPEEvent evt;
        evt.type = MPEEvent::PolyPressure;
        evt.noteNumber = msg.getNoteNumber();
        evt.channel = msg.getChannel() - 1;
        evt.value = msg.getAfterTouchValue();
        evt.timestamp = totalSamples_ / sampleRate_;
        events_.push_back(evt);
    }
    else if (msg.isController() && msg.getControllerNumber() == 74) {
        // MPE Timbre = CC74
        MPEEvent evt;
        evt.type = MPEEvent::Timbre;
        evt.channel = msg.getChannel() - 1;
        evt.value = msg.getControllerValue();
        evt.timestamp = totalSamples_ / sampleRate_;
        events_.push_back(evt);
    }
}

bool MPERecorder::saveToMidiFile(const juce::File& file) {
    if (events_.empty()) return false;
    
    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(480);
    
    // 添加MPE配置元数据到第一个track
    juce::MidiMessageSequence mpeSetup;
    mpeSetup.addEvent(juce::MidiMessage::textMetaEvent(1, "LianCore V3 MPE Recording"));
    mpeSetup.addEvent(juce::MidiMessage::textMetaEvent(2, "MPE Enabled"));
    // 添加录制时间戳
    auto now = juce::Time::getCurrentTime();
    mpeSetup.addEvent(juce::MidiMessage::textMetaEvent(3, 
        "Recorded: " + now.formatted("%Y-%m-%d %H:%M:%S")));
    midiFile.addTrack(mpeSetup);
    
    // 主事件track
    juce::MidiMessageSequence seq;
    
    // 添加track名称
    seq.addEvent(juce::MidiMessage::textMetaEvent(1, "MPE Events"));
    
    // 设置初始弯音范围（MPE要求 ±48 半音）
    for (int ch = 1; ch <= 16; ++ch) {
        // RPN 0: Pitch Bend Sensitivity
        seq.addEvent(juce::MidiMessage::controllerEvent(ch, 101, 0), 0.0);  // RPN MSB
        seq.addEvent(juce::MidiMessage::controllerEvent(ch, 100, 0), 0.0);  // RPN LSB
        seq.addEvent(juce::MidiMessage::controllerEvent(ch, 6, 48), 0.0);   // Data Entry: 48 semitones
        seq.addEvent(juce::MidiMessage::controllerEvent(ch, 38, 0), 0.0);   // Data Entry LSB
        // RPN Null
        seq.addEvent(juce::MidiMessage::controllerEvent(ch, 101, 127), 0.0);
        seq.addEvent(juce::MidiMessage::controllerEvent(ch, 100, 127), 0.0);
    }
    
    // 转换事件
    for (const auto& evt : events_) {
        int ticks = static_cast<int>(evt.timestamp * 480.0);
        juce::MidiMessage msg = evt.toMidiMessage();
        seq.addEvent(msg, static_cast<double>(ticks));
    }
    
    // 确保NoteOn/NoteOff配对
    seq.updateMatchedPairs();
    
    midiFile.addTrack(seq);
    
    // 使用JUCE的标准写入方法
    juce::FileOutputStream stream(file);
    if (!stream.openedOk()) return false;
    
    return midiFile.writeTo(stream);
}

bool MPERecorder::saveToJsonFile(const juce::File& file) {
    if (events_.empty()) return false;
    
    juce::Array<juce::var> eventArray;
    for (const auto& evt : events_) {
        eventArray.add(evt.toJson());
    }
    
    auto root = new juce::DynamicObject();
    root->setProperty("version", "1.0");
    root->setProperty("format", "LianCore MPE Recording");
    root->setProperty("sampleRate", sampleRate_);
    root->setProperty("duration", getDuration());
    root->setProperty("eventCount", getEventCount());
    root->setProperty("noteCount", getNoteCount());
    root->setProperty("events", eventArray);
    
    juce::var json(root);
    juce::FileOutputStream stream(file);
    if (!stream.openedOk()) return false;
    
    stream.writeText(json.toString(), false, false, "\n");
    return true;
}

bool MPERecorder::loadFromJsonFile(const juce::File& file) {
    if (!file.existsAsFile()) return false;
    
    juce::String content = file.loadFileAsString();
    auto json = juce::JSON::parse(content);
    if (json.isVoid()) return false;
    
    events_.clear();
    if (auto* obj = json.getDynamicObject()) {
        sampleRate_ = obj->getProperty("sampleRate");
        
        if (auto* arr = obj->getProperty("events").getArray()) {
            for (auto& v : *arr) {
                events_.push_back(MPEEvent::fromJson(v));
            }
        }
    }
    return true;
}

int MPERecorder::getNoteCount() const {
    std::set<int> notes;
    for (const auto& evt : events_) {
        if (evt.type == MPEEvent::NoteOn) {
            notes.insert(evt.noteNumber);
        }
    }
    return (int)notes.size();
}

// ==================== MPEPlayer ====================

void MPEPlayer::processBlock(juce::MidiBuffer& midiBuffer, int numSamples) {
    if (!playing_ || events_.empty()) return;
    
    double blockDuration = numSamples / sampleRate_;
    double blockEnd = playbackPosition_ + blockDuration;
    
    while (nextEventIndex_ < events_.size() && 
           events_[nextEventIndex_].timestamp < blockEnd) {
        const auto& evt = events_[nextEventIndex_];
        
        int sampleOffset = static_cast<int>(
            (evt.timestamp - playbackPosition_) * sampleRate_);
        if (sampleOffset < 0) sampleOffset = 0;
        if (sampleOffset >= numSamples) sampleOffset = numSamples - 1;
        
        juce::MidiMessage msg = evt.toMidiMessage();
        midiBuffer.addEvent(msg, sampleOffset);
        nextEventIndex_++;
    }
    
    playbackPosition_ = blockEnd;
    
    // 播放结束处理
    if (nextEventIndex_ >= events_.size()) {
        if (looping_) {
            // 循环播放：回到开头
            nextEventIndex_ = 0;
            playbackPosition_ = 0.0;
        } else {
            playing_ = false;
        }
    }
}

bool MPEPlayer::loadFromMidiFile(const juce::File& file) {
    if (!file.existsAsFile()) return false;
    
    juce::FileInputStream stream(file);
    if (!stream.openedOk()) return false;
    
    juce::MidiFile midiFile;
    if (!midiFile.readFrom(stream)) return false;
    
    events_.clear();
    double ticksPerQuarter = midiFile.getTimeFormat();
    
    // 假设120 BPM作为默认速度
    double secondsPerTick = 0.5 / ticksPerQuarter; // 120 BPM = 0.5s per beat
    
    for (int t = 0; t < midiFile.getNumTracks(); ++t) {
        const auto* track = midiFile.getTrack(t);
        if (track == nullptr) continue;
        
        for (int i = 0; i < track->getNumEvents(); ++i) {
            auto* midiEvent = track->getEventPointer(i);
            auto msg = midiEvent->message;
            double timestamp = midiEvent->message.getTimeStamp() * secondsPerTick;
            
            if (msg.isNoteOn()) {
                MPEEvent evt;
                evt.type = MPEEvent::NoteOn;
                evt.noteNumber = msg.getNoteNumber();
                evt.channel = msg.getChannel() - 1;
                evt.value = msg.getVelocity();
                evt.timestamp = timestamp;
                events_.push_back(evt);
            } else if (msg.isNoteOff()) {
                MPEEvent evt;
                evt.type = MPEEvent::NoteOff;
                evt.noteNumber = msg.getNoteNumber();
                evt.channel = msg.getChannel() - 1;
                evt.timestamp = timestamp;
                events_.push_back(evt);
            } else if (msg.isPitchWheel()) {
                MPEEvent evt;
                evt.type = MPEEvent::PitchBend;
                evt.channel = msg.getChannel() - 1;
                evt.value = msg.getPitchWheelValue();
                evt.timestamp = timestamp;
                events_.push_back(evt);
            } else if (msg.isChannelPressure()) {
                MPEEvent evt;
                evt.type = MPEEvent::ChannelPressure;
                evt.channel = msg.getChannel() - 1;
                evt.value = msg.getChannelPressureValue();
                evt.timestamp = timestamp;
                events_.push_back(evt);
            } else if (msg.isAftertouch()) {
                MPEEvent evt;
                evt.type = MPEEvent::PolyPressure;
                evt.noteNumber = msg.getNoteNumber();
                evt.channel = msg.getChannel() - 1;
                evt.value = msg.getAfterTouchValue();
                evt.timestamp = timestamp;
                events_.push_back(evt);
            } else if (msg.isController() && msg.getControllerNumber() == 74) {
                MPEEvent evt;
                evt.type = MPEEvent::Timbre;
                evt.channel = msg.getChannel() - 1;
                evt.value = msg.getControllerValue();
                evt.timestamp = timestamp;
                events_.push_back(evt);
            }
        }
    }
    
    // 按时间戳排序
    std::sort(events_.begin(), events_.end(),
        [](const MPEEvent& a, const MPEEvent& b) {
            return a.timestamp < b.timestamp;
        });
    
    return !events_.empty();
}

} // namespace LianCore
