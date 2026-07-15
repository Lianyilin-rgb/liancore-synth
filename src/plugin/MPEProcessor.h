// =============================================================================
// LianCore - MPEProcessor MPE 数据处理器
// 包装 JUCE MPEInstrument，提取逐音符 MPE 数据并转换为合成参数
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <array>

namespace LianCore {

// =============================================================================
// MPEVoiceState - 单个 MPE 音符的状态快照
// =============================================================================
struct MPEVoiceState {
    int noteNumber = 60;          // MIDI 音符号 (0-127)
    int midiChannel = 1;          // MIDI 通道 (1-16)
    float velocity = 0.0f;        // 触发力度 (0.0-1.0)
    float pitchBendSemitones = 0.0f; // 弯音偏移 (半音, ±48)
    float pressure = 0.0f;        // 复音触后 (0.0-1.0)
    float timbre = 0.0f;          // 音色/滑音 (0.0-1.0, CC74)
    float noteOnTime = 0.0f;      // 音符触发时间
    bool isActive = false;        // 是否正在发音
    bool isNoteOn = false;        // 是否处于 Note On 状态
    juce::MPENote mpeNote;        // 原始 JUCE MPENote (用于高级操作)
};

// =============================================================================
// MPEProcessor - MPE 数据提取与参数转换
// =============================================================================
class MPEProcessor {
public:
    // 配置常量 (公开, 供外部使用)
    static constexpr int kMPEMasterChannel = 1;         // 主通道
    static constexpr int kMPEMemberChannels = 15;       // 下区成员通道 (通道 2-16)
    static constexpr int kMPEUpperMemberChannels = 15;  // 上区成员通道 (通道 1-15)
    static constexpr int kMPEPitchBendRange = 48;       // 弯音范围 (半音)
    static constexpr int kMPEMaxPolyphony = 32;         // 最大复音数 (P1-1)

    MPEProcessor();
    ~MPEProcessor() = default;

    // =========================================================================
    // MPE 模式管理
    // =========================================================================
    void enable(bool enable);
    bool isEnabled() const;
    bool isMPEActive() const;  // 是否有活跃的 MPE 连接

    // =========================================================================
    // MIDI 处理
    // =========================================================================
    void processMidiBuffer(juce::MidiBuffer& midi);

    // =========================================================================
    // 音符状态查询
    // =========================================================================
    int getNumActiveNotes() const;
    const MPEVoiceState& getNoteState(int noteIndex) const;
    const std::vector<MPEVoiceState>& getAllNoteStates() const;

    // =========================================================================
    // MPE 参数提取 (用于调制路由)
    // =========================================================================
    float getMasterPitchBend() const;  // 主通道弯音 (半音)
    float getMasterPressure() const;   // 主通道压力 (0.0-1.0)
    float getMasterTimbre() const;     // 主通道音色 (0.0-1.0)

    // 获取第一个活跃音符的 MPE 参数 (用于单音合成)
    float getFirstNotePitchBend() const;
    float getFirstNotePressure() const;
    float getFirstNoteTimbre() const;

    // =========================================================================
    // 调制参数映射 (MPE → 合成参数)
    // =========================================================================
    float getPressureToFilterCutoff() const;   // 压力 → 滤波器截止频率偏移
    float getTimbreToResonance() const;        // 音色 → 共振峰偏移
    float getPitchBendToFrequency() const;     // 弯音 → 频率倍率

    // =========================================================================
    // 底层 JUCE MPEInstrument 访问
    // =========================================================================
    juce::MPEInstrument& getInstrument() { return instrument_; }
    const juce::MPEInstrument& getInstrument() const { return instrument_; }

private:
    // =========================================================================
    // 内部方法
    // =========================================================================
    void updateVoiceStates();
    void configureMPEZone();

    // =========================================================================
    // 成员变量
    // =========================================================================
    juce::MPEInstrument instrument_;
    bool enabled_ = false;

    // 音符状态缓存 (最大 32 复音)
    std::vector<MPEVoiceState> voiceStates_;

    // 主通道 MPE 状态
    float masterPitchBend_ = 0.0f;
    float masterPressure_ = 0.0f;
    float masterTimbre_ = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MPEProcessor)
};

} // namespace LianCore