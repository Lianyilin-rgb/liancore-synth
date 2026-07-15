// =============================================================================
// LianCore - MPEProcessor 实现
// =============================================================================
#include "MPEProcessor.h"
#include <cmath>

namespace LianCore {

// =============================================================================
// 构造
// =============================================================================
MPEProcessor::MPEProcessor() {
    voiceStates_.reserve(16); // 最大 16 复音

    // 默认启用 MPE
    configureMPEZone();
    enabled_ = true;
}

// =============================================================================
// MPE 模式管理
// =============================================================================
void MPEProcessor::enable(bool enable) {
    if (enable == enabled_) return;
    enabled_ = enable;

    if (enable) {
        configureMPEZone();
    } else {
        instrument_.setZoneLayout(juce::MPEZoneLayout());
        voiceStates_.clear();
    }
}

bool MPEProcessor::isEnabled() const {
    return enabled_;
}

bool MPEProcessor::isMPEActive() const {
    return enabled_ && getNumActiveNotes() > 0;
}

// =============================================================================
// MPE Zone 配置
// =============================================================================
void MPEProcessor::configureMPEZone() {
    // MPE Zone 配置 (P1-1: 最大 32 复音)
    //   下区 (Lower Zone): 通道 2-16 (15 个成员通道), 弯音 ±48 半音
    //   上区 (Upper Zone): 通道 1-15 (15 个成员通道), 弯音 ±48 半音
    //   总计: 30 个 MPE 复音 + 通道 16 作为全局主通道
    //   非 MPE 模式: 无限制, 由 JUCE MPEInstrument 管理
    juce::MPEZoneLayout layout;
    layout.setLowerZone(kMPEMemberChannels, kMPEPitchBendRange, kMPEMasterChannel + 1);
    layout.setUpperZone(kMPEUpperMemberChannels, kMPEPitchBendRange, kMPEMasterChannel + 1);
    instrument_.setZoneLayout(layout);
    // setZoneLayout 自动禁用 legacy mode
}

// =============================================================================
// MIDI 处理
// =============================================================================
void MPEProcessor::processMidiBuffer(juce::MidiBuffer& midi) {
    if (!enabled_) return;

    // 遍历 MIDI 缓冲区, 将每个事件送进 MPEInstrument
    for (const auto metadata : midi) {
        auto msg = metadata.getMessage();
        instrument_.processNextMidiEvent(msg);
    }

    // 更新音符状态快照
    updateVoiceStates();
}

// =============================================================================
// 音符状态更新
// =============================================================================
void MPEProcessor::updateVoiceStates() {
    voiceStates_.clear();

    // 遍历当前活跃的 MPE 音符
    for (int i = 0; i < instrument_.getNumPlayingNotes(); ++i) {
        auto note = instrument_.getNote(i);

        MPEVoiceState state;
        state.noteNumber = note.initialNote;
        state.midiChannel = note.midiChannel;
        state.velocity = note.noteOnVelocity.asUnsignedFloat();
        state.pitchBendSemitones = static_cast<float>(note.totalPitchbendInSemitones);
        state.pressure = note.pressure.asUnsignedFloat();
        state.timbre = note.timbre.asUnsignedFloat();
        state.isActive = true;
        state.isNoteOn = (note.keyState != juce::MPENote::off);
        state.mpeNote = note;

        voiceStates_.push_back(state);
    }

    // 更新主通道状态 (来自通道 1 的全局 MPE 消息)
    if (!voiceStates_.empty()) {
        masterPitchBend_ = voiceStates_[0].pitchBendSemitones;
        masterPressure_ = voiceStates_[0].pressure;
        masterTimbre_ = voiceStates_[0].timbre;
    } else {
        masterPitchBend_ = 0.0f;
        masterPressure_ = 0.0f;
        masterTimbre_ = 0.0f;
    }
}

// =============================================================================
// 音符状态查询
// =============================================================================
int MPEProcessor::getNumActiveNotes() const {
    return static_cast<int>(voiceStates_.size());
}

const MPEVoiceState& MPEProcessor::getNoteState(int noteIndex) const {
    static MPEVoiceState emptyState;
    if (noteIndex >= 0 && noteIndex < static_cast<int>(voiceStates_.size())) {
        return voiceStates_[noteIndex];
    }
    return emptyState;
}

const std::vector<MPEVoiceState>& MPEProcessor::getAllNoteStates() const {
    return voiceStates_;
}

// =============================================================================
// MPE 参数提取
// =============================================================================
float MPEProcessor::getMasterPitchBend() const {
    return masterPitchBend_;
}

float MPEProcessor::getMasterPressure() const {
    return masterPressure_;
}

float MPEProcessor::getMasterTimbre() const {
    return masterTimbre_;
}

float MPEProcessor::getFirstNotePitchBend() const {
    if (voiceStates_.empty()) return 0.0f;
    return voiceStates_[0].pitchBendSemitones;
}

float MPEProcessor::getFirstNotePressure() const {
    if (voiceStates_.empty()) return 0.0f;
    return voiceStates_[0].pressure;
}

float MPEProcessor::getFirstNoteTimbre() const {
    if (voiceStates_.empty()) return 0.0f;
    return voiceStates_[0].timbre;
}

// =============================================================================
// 调制参数映射 (MPE → 合成参数)
// =============================================================================
float MPEProcessor::getPressureToFilterCutoff() const {
    // 压力增加 → 滤波器截止频率升高 (增加亮度)
    // 映射: pressure [0,1] → cutoff offset [0, 4000Hz]
    float pressure = getFirstNotePressure();
    return pressure * 4000.0f;
}

float MPEProcessor::getTimbreToResonance() const {
    // 音色/滑音 → 共振峰偏移
    // 映射: timbre [0,1] → resonance [0.1, 10.0]
    float timbre = getFirstNoteTimbre();
    return 0.1f + timbre * 9.9f;
}

float MPEProcessor::getPitchBendToFrequency() const {
    // 弯音 → 频率倍率
    // 映射: pitchBendSemitones → 频率倍率 (2^(semitones/12))
    float semitones = getFirstNotePitchBend();
    return std::pow(2.0f, semitones / 12.0f);
}

} // namespace LianCore