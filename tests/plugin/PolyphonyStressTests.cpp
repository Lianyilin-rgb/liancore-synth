// =============================================================================
// LianCore - 复音数压力测试 (P1-1)
// 测试 32 复音极限: MPE 模式 30 音符 (15+15), 非 MPE 模式 32 音符
// =============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <JuceHeader.h>
#include "../../src/plugin/MPEProcessor.h"

using namespace LianCore;
using Catch::Approx;

// =============================================================================
// MPE 复音压力测试: 30 音符 (MPE 双 Zone 最大)
// =============================================================================

TEST_CASE("MPEProcessor: 30-note MPE polyphony (both zones)", "[polyphony][stress]") {
    MPEProcessor processor;

    // 下区: 通道 2-16, 15 个音符
    // 上区: 通道 1-15, 15 个音符
    // 总计: 30 个 MPE 复音
    juce::MidiBuffer midi;
    for (int i = 0; i < 30; ++i) {
        // 交替使用下区和上区通道
        int channel = (i < 15) ? (2 + i) : (1 + (i - 15));
        int note = 36 + i;
        float velocity = 0.5f + (i * 0.01f);
        midi.addEvent(juce::MidiMessage::noteOn(channel, note, velocity), 0);
    }

    processor.processMidiBuffer(midi);

    // 验证 30 个音符全部被追踪
    REQUIRE(processor.getNumActiveNotes() >= 30);
    REQUIRE(processor.isMPEActive() == true);

    // 验证所有音符状态
    auto& allStates = processor.getAllNoteStates();
    REQUIRE(allStates.size() >= 30);

    int activeCount = 0;
    for (size_t i = 0; i < allStates.size(); ++i) {
        if (allStates[i].isActive) activeCount++;
    }
    REQUIRE(activeCount >= 30);
}

TEST_CASE("MPEProcessor: 30-note release stress test", "[polyphony][stress]") {
    MPEProcessor processor;

    // 触发 30 个音符
    juce::MidiBuffer midiOn;
    for (int i = 0; i < 30; ++i) {
        int channel = (i < 15) ? (2 + i) : (1 + (i - 15));
        int note = 36 + i;
        midiOn.addEvent(juce::MidiMessage::noteOn(channel, note, 0.5f), 0);
    }
    processor.processMidiBuffer(midiOn);
    REQUIRE(processor.getNumActiveNotes() >= 30);

    // 释放所有音符
    juce::MidiBuffer midiOff;
    for (int i = 0; i < 30; ++i) {
        int channel = (i < 15) ? (2 + i) : (1 + (i - 15));
        int note = 36 + i;
        midiOff.addEvent(juce::MidiMessage::noteOff(channel, note), 0);
    }
    processor.processMidiBuffer(midiOff);

    // 全部释放后应为 0
    REQUIRE(processor.getNumActiveNotes() == 0);
}

TEST_CASE("MPEProcessor: 16 notes in a single zone", "[polyphony][stress]") {
    MPEProcessor processor;

    // 下区: 15 个音符 + 1 个 (可能被拒绝)
    juce::MidiBuffer midi;
    for (int i = 0; i < 16; ++i) {
        int channel = 2 + (i % 15);  // 通道 2-16
        int note = 36 + i;
        midi.addEvent(juce::MidiMessage::noteOn(channel, note, 0.5f), 0);
    }

    processor.processMidiBuffer(midi);

    // 单区最多 15 个音符, 但可能因 MPE 分配而不同
    // 至少应有 15 个音符被追踪
    REQUIRE(processor.getNumActiveNotes() >= 15);
}

TEST_CASE("MPEProcessor: mixed on/off with 30 notes", "[polyphony][stress]") {
    MPEProcessor processor;

    // 第一批: 触发 15 个音符 (下区)
    juce::MidiBuffer midi1;
    for (int i = 0; i < 15; ++i) {
        int channel = 2 + i;  // 通道 2-16
        int note = 36 + i;
        midi1.addEvent(juce::MidiMessage::noteOn(channel, note, 0.5f), 0);
    }
    processor.processMidiBuffer(midi1);
    REQUIRE(processor.getNumActiveNotes() >= 15);

    // 第二批: 触发 15 个新音符 (上区)
    juce::MidiBuffer midi2;
    for (int i = 0; i < 15; ++i) {
        int channel = 1 + i;  // 通道 1-15
        int note = 52 + i;
        midi2.addEvent(juce::MidiMessage::noteOn(channel, note, 0.5f), 0);
    }
    processor.processMidiBuffer(midi2);
    REQUIRE(processor.getNumActiveNotes() >= 30);

    // 释放第一批 15 个音符
    juce::MidiBuffer midiOff1;
    for (int i = 0; i < 15; ++i) {
        int channel = 2 + i;
        int note = 36 + i;
        midiOff1.addEvent(juce::MidiMessage::noteOff(channel, note), 0);
    }
    processor.processMidiBuffer(midiOff1);
    REQUIRE(processor.getNumActiveNotes() >= 15);

    // 释放第二批 15 个音符
    juce::MidiBuffer midiOff2;
    for (int i = 0; i < 15; ++i) {
        int channel = 1 + i;
        int note = 52 + i;
        midiOff2.addEvent(juce::MidiMessage::noteOff(channel, note), 0);
    }
    processor.processMidiBuffer(midiOff2);
    REQUIRE(processor.getNumActiveNotes() == 0);
}

TEST_CASE("MPEProcessor: rapid note cycling (30 notes)", "[polyphony][stress]") {
    MPEProcessor processor;

    // 快速交替触发/释放音符 (模拟密集演奏)
    for (int round = 0; round < 3; ++round) {
        juce::MidiBuffer midiOn;
        for (int i = 0; i < 10; ++i) {
            int channel = (round % 2 == 0) ? (2 + i) : (1 + i);
            int note = 48 + i + round * 10;
            midiOn.addEvent(juce::MidiMessage::noteOn(channel, note, 0.5f), 0);
        }
        processor.processMidiBuffer(midiOn);
    }

    // 3 轮 × 10 音符 = 30 个活跃音符
    REQUIRE(processor.getNumActiveNotes() >= 30);
}

// =============================================================================
// 编译时验证
// =============================================================================

TEST_CASE("Polyphony: LIANCORE_MAX_POLYPHONY compile-time check", "[polyphony][unit]") {
#ifdef LIANCORE_MAX_POLYPHONY
    REQUIRE(LIANCORE_MAX_POLYPHONY == 32);
#else
    FAIL("LIANCORE_MAX_POLYPHONY is not defined");
#endif
}

TEST_CASE("Polyphony: MPE processor max polyphony constant", "[polyphony][unit]") {
    REQUIRE(MPEProcessor::kMPEMaxPolyphony == 32);
}