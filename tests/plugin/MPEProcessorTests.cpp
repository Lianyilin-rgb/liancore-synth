// =============================================================================
// LianCore - MPEProcessor 单元测试
// 测试 MPE 数据提取、参数转换、MIDI 消息处理
// =============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <JuceHeader.h>
#include "../../src/plugin/MPEProcessor.h"

using namespace LianCore;
using Catch::Approx;

// =============================================================================
// MPEProcessor 初始化与配置
// =============================================================================

TEST_CASE("MPEProcessor: default initialization", "[mpe][unit]") {
    MPEProcessor processor;

    // 默认启用
    REQUIRE(processor.isEnabled() == true);

    // 初始无活跃音符
    REQUIRE(processor.getNumActiveNotes() == 0);

    // 默认 MPE 参数为零
    REQUIRE(processor.getMasterPitchBend() == Approx(0.0f).margin(0.001f));
    REQUIRE(processor.getMasterPressure() == Approx(0.0f).margin(0.001f));
    REQUIRE(processor.getMasterTimbre() == Approx(0.0f).margin(0.001f));
}

TEST_CASE("MPEProcessor: enable/disable toggle", "[mpe][unit]") {
    MPEProcessor processor;

    // 默认启用
    REQUIRE(processor.isEnabled() == true);

    // 禁用
    processor.enable(false);
    REQUIRE(processor.isEnabled() == false);

    // 重新启用
    processor.enable(true);
    REQUIRE(processor.isEnabled() == true);
}

TEST_CASE("MPEProcessor: repeated enable/disable is idempotent", "[mpe][unit]") {
    MPEProcessor processor;

    // 重复启用
    processor.enable(true);
    REQUIRE(processor.isEnabled() == true);
    processor.enable(true);
    REQUIRE(processor.isEnabled() == true);

    // 重复禁用
    processor.enable(false);
    REQUIRE(processor.isEnabled() == false);
    processor.enable(false);
    REQUIRE(processor.isEnabled() == false);
}

TEST_CASE("MPEProcessor: isMPEActive without notes", "[mpe][unit]") {
    MPEProcessor processor;

    // 启用但无活跃音符 → 不活跃
    REQUIRE(processor.isMPEActive() == false);

    // 禁用时更不活跃
    processor.enable(false);
    REQUIRE(processor.isMPEActive() == false);
}

// =============================================================================
// MIDI 消息处理
// =============================================================================

TEST_CASE("MPEProcessor: process empty MIDI buffer", "[mpe][integration]") {
    MPEProcessor processor;
    juce::MidiBuffer midi;

    processor.processMidiBuffer(midi);

    REQUIRE(processor.getNumActiveNotes() == 0);
}

TEST_CASE("MPEProcessor: process MIDI buffer when disabled", "[mpe][integration]") {
    MPEProcessor processor;
    processor.enable(false);

    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(2, 60, 0.8f), 0);

    processor.processMidiBuffer(midi);

    // 禁用时不处理
    REQUIRE(processor.getNumActiveNotes() == 0);
}

TEST_CASE("MPEProcessor: note on/off via MPE channels", "[mpe][integration]") {
    MPEProcessor processor;

    juce::MidiBuffer midi;
    // Note On 在 MPE 成员通道 2
    midi.addEvent(juce::MidiMessage::noteOn(2, 60, 0.8f), 0);

    processor.processMidiBuffer(midi);

    // 应追踪到此音符
    REQUIRE(processor.getNumActiveNotes() == 1);
    REQUIRE(processor.isMPEActive() == true);

    auto& state = processor.getNoteState(0);
    REQUIRE(state.noteNumber == 60);
    REQUIRE(state.midiChannel == 2);
    REQUIRE(state.velocity == Approx(0.8f).margin(0.01f));
    REQUIRE(state.isNoteOn == true);

    // Note Off
    juce::MidiBuffer midiOff;
    midiOff.addEvent(juce::MidiMessage::noteOff(2, 60), 0);
    processor.processMidiBuffer(midiOff);

    REQUIRE(processor.getNumActiveNotes() == 0);
}

TEST_CASE("MPEProcessor: multiple notes on different channels", "[mpe][integration]") {
    MPEProcessor processor;

    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(2, 60, 0.8f), 0);
    midi.addEvent(juce::MidiMessage::noteOn(3, 64, 0.7f), 0);
    midi.addEvent(juce::MidiMessage::noteOn(4, 67, 0.6f), 0);

    processor.processMidiBuffer(midi);

    REQUIRE(processor.getNumActiveNotes() == 3);

    // 验证不同通道
    REQUIRE(processor.getNoteState(0).midiChannel == 2);
    REQUIRE(processor.getNoteState(1).midiChannel == 3);
    REQUIRE(processor.getNoteState(2).midiChannel == 4);
}

// =============================================================================
// 复音触后 (Polyphonic Aftertouch)
// =============================================================================

TEST_CASE("MPEProcessor: polyphonic aftertouch routing", "[mpe][integration]") {
    MPEProcessor processor;

    // Note On + 通道压力在同一buffer中发送
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(2, 60, 0.7f), 0);
    midi.addEvent(juce::MidiMessage::channelPressureChange(2, 64), 1); // 压力 ≈ 0.5

    processor.processMidiBuffer(midi);
    REQUIRE(processor.getNumActiveNotes() == 1);

    // 压力应被 MPEInstrument 接收并更新
    float pressure = processor.getFirstNotePressure();
    // 注意: 压力值取决于 MPEInstrument 的实现, 范围应为 [0, 1]
    REQUIRE(pressure >= 0.0f);
    REQUIRE(pressure <= 1.0f);
}

// =============================================================================
// 弯音 (Pitch Bend)
// =============================================================================

TEST_CASE("MPEProcessor: pitch bend processing", "[mpe][integration]") {
    MPEProcessor processor;

    // Note On + Pitch Bend 在同一buffer中发送
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(2, 60, 0.8f), 0);
    midi.addEvent(juce::MidiMessage::pitchWheel(2, 0x2FFF), 1); // 向上弯音

    processor.processMidiBuffer(midi);
    REQUIRE(processor.getNumActiveNotes() == 1);

    // 弯音应非零
    float bend = processor.getFirstNotePitchBend();
    REQUIRE(bend != Approx(0.0f).margin(0.01f));
}

TEST_CASE("MPEProcessor: pitch bend to frequency ratio", "[mpe][unit]") {
    MPEProcessor processor;

    // Note On with center pitch bend (14-bit 中心=8192=0x2000)
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(2, 60, 0.8f), 0);
    midi.addEvent(juce::MidiMessage::pitchWheel(2, 0x2000), 0); // 中心 (8192)

    processor.processMidiBuffer(midi);

    // 中心弯音 → 频率倍率 ≈ 1.0
    float ratio = processor.getPitchBendToFrequency();
    REQUIRE(ratio == Approx(1.0f).margin(0.05f));
}

// =============================================================================
// 滑音 / 音色 (Timbre, CC74)
// =============================================================================

TEST_CASE("MPEProcessor: timbre CC74 processing", "[mpe][integration]") {
    MPEProcessor processor;

    // Note On
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(2, 60, 0.8f), 0);
    // CC74 = 音色/滑音
    midi.addEvent(juce::MidiMessage::controllerEvent(2, 74, 64), 0); // 中值

    processor.processMidiBuffer(midi);

    REQUIRE(processor.getNumActiveNotes() == 1);

    // 音色值应在 0-1 范围内
    float timbre = processor.getFirstNoteTimbre();
    REQUIRE(timbre >= 0.0f);
    REQUIRE(timbre <= 1.0f);
}

// =============================================================================
// 参数映射 (MPE → 合成参数)
// =============================================================================

TEST_CASE("MPEProcessor: pressure to filter cutoff", "[mpe][unit]") {
    MPEProcessor processor;

    // 没有活跃音符时，cutoff 应为 0
    float cutoff = processor.getPressureToFilterCutoff();
    REQUIRE(cutoff == Approx(0.0f).margin(0.01f));
}

TEST_CASE("MPEProcessor: timbre to resonance", "[mpe][unit]") {
    MPEProcessor processor;

    // 没有活跃音符时，resonance 应为最小值
    float resonance = processor.getTimbreToResonance();
    REQUIRE(resonance == Approx(0.1f).margin(0.01f));
}

TEST_CASE("MPEProcessor: pitch bend to frequency ratio (no notes)", "[mpe][unit]") {
    MPEProcessor processor;

    // 没有活跃音符时，弯音倍率应为 1.0
    float ratio = processor.getPitchBendToFrequency();
    REQUIRE(ratio == Approx(1.0f).margin(0.01f));
}

// =============================================================================
// 边界条件
// =============================================================================

TEST_CASE("MPEProcessor: getNoteState out of bounds", "[mpe][unit]") {
    MPEProcessor processor;

    // 越界访问应返回空状态
    auto& state = processor.getNoteState(-1);
    REQUIRE(state.isActive == false);

    auto& state2 = processor.getNoteState(100);
    REQUIRE(state2.isActive == false);
}

TEST_CASE("MPEProcessor: getAllNoteStates empty", "[mpe][unit]") {
    MPEProcessor processor;

    auto& states = processor.getAllNoteStates();
    REQUIRE(states.empty());
}

TEST_CASE("MPEProcessor: getInstrument access", "[mpe][unit]") {
    MPEProcessor processor;

    auto& instrument = processor.getInstrument();
    // 应返回有效的 MPEInstrument 引用
    // 验证 Zone 配置存在
    auto layout = instrument.getZoneLayout();
    REQUIRE(layout.isActive());
}