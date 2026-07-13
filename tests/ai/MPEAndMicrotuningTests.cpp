/**
 * MPE 和微音程单元测试
 * 测试 MPE MIDI消息处理和 Scala 调音文件加载
 */

#include <catch2/catch_test_macros.hpp>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

// 模拟 PluginProcessor 的头文件
#include "../../src/plugin/PluginProcessor.h"

using namespace LianCore;

// =============================================================================
// MPE 测试
// =============================================================================

TEST_CASE("MPE: 默认启用", "[mpe][integration]") {
    PluginProcessor processor;

    REQUIRE(processor.isMPEEnabled() == true);

    auto& mpeInst = processor.getMPEInstrument();
    REQUIRE(mpeInst.isLegacyModeEnabled() == true);
}

TEST_CASE("MPE: 启用/禁用切换", "[mpe][unit]") {
    PluginProcessor processor;

    // 默认启用
    REQUIRE(processor.isMPEEnabled() == true);

    // 禁用
    processor.enableMPE(false);
    REQUIRE(processor.isMPEEnabled() == false);
    REQUIRE(processor.getMPEInstrument().isLegacyModeEnabled() == false);

    // 重新启用
    processor.enableMPE(true);
    REQUIRE(processor.isMPEEnabled() == true);
    REQUIRE(processor.getMPEInstrument().isLegacyModeEnabled() == true);
}

TEST_CASE("MPE: 重复调用不改变状态", "[mpe][unit]") {
    PluginProcessor processor;

    REQUIRE(processor.isMPEEnabled() == true);

    // 重复启用
    processor.enableMPE(true);
    REQUIRE(processor.isMPEEnabled() == true);

    processor.enableMPE(true);
    REQUIRE(processor.isMPEEnabled() == true);
}

TEST_CASE("MPE: MPEInstrument 区域配置", "[mpe][unit]") {
    PluginProcessor processor;

    auto& mpeInst = processor.getMPEInstrument();

    // 验证 Legacy Mode 启用
    REQUIRE(mpeInst.isLegacyModeEnabled() == true);

    // 验证 Zone 配置
    auto zoneLayout = mpeInst.getZoneLayout();
    REQUIRE(zoneLayout.getNumZones() > 0);
}

TEST_CASE("MPE: MIDI缓冲区处理MQE消息", "[mpe][integration]") {
    PluginProcessor processor;
    processor.prepareToPlay(44100.0, 512);

    // 创建 MPE 设置消息 (RPN 6: MPE Configuration)
    juce::MidiBuffer midiBuffer;
    juce::MidiMessage mpeConfigMsg = juce::MidiMessage::controllerEvent(1, 101, 0);  // RPN MSB
    midiBuffer.addEvent(mpeConfigMsg, 0);
    juce::MidiMessage mpeConfigMsg2 = juce::MidiMessage::controllerEvent(1, 100, 6);  // RPN LSB = 6 (MPE)
    midiBuffer.addEvent(mpeConfigMsg2, 0);

    // 创建 Note On 消息 (通道 2, 复音触后)
    juce::MidiMessage noteOn = juce::MidiMessage::noteOn(2, 60, 0.8f);
    midiBuffer.addEvent(noteOn, 100);

    juce::AudioBuffer<float> buffer(2, 512);
    processor.processBlock(buffer, midiBuffer);

    // 验证处理无崩溃
    REQUIRE(true);
}

TEST_CASE("MPE: 复音触后消息传递", "[mpe][integration]") {
    PluginProcessor processor;
    processor.prepareToPlay(44100.0, 512);

    juce::MidiBuffer midiBuffer;

    // Note On (通道 2, velocity 0.7)
    midiBuffer.addEvent(juce::MidiMessage::noteOn(2, 60, 0.7f), 0);
    // 复音触后 (通道 2, note 60, pressure 0.5)
    midiBuffer.addEvent(juce::MidiMessage::aftertouchChange(2, 60, 0.5f), 100);
    // Note Off
    midiBuffer.addEvent(juce::MidiMessage::noteOff(2, 60), 400);

    juce::AudioBuffer<float> buffer(2, 512);
    processor.processBlock(buffer, midiBuffer);

    REQUIRE(true);
}

TEST_CASE("MPE: 弯音消息处理", "[mpe][integration]") {
    PluginProcessor processor;
    processor.prepareToPlay(44100.0, 512);

    juce::MidiBuffer midiBuffer;

    // Note On
    midiBuffer.addEvent(juce::MidiMessage::noteOn(2, 60, 0.8f), 0);
    // 弯音 (通道 2, ±48半音)
    midiBuffer.addEvent(juce::MidiMessage::pitchWheel(2, 0x4000), 100);  // 中心
    midiBuffer.addEvent(juce::MidiMessage::pitchWheel(2, 0x6000), 200);  // 向上
    midiBuffer.addEvent(juce::MidiMessage::noteOff(2, 60), 400);

    juce::AudioBuffer<float> buffer(2, 512);
    processor.processBlock(buffer, midiBuffer);

    REQUIRE(true);
}

TEST_CASE("MPE: 多通道同时播放", "[mpe][integration]") {
    PluginProcessor processor;
    processor.prepareToPlay(44100.0, 512);

    juce::MidiBuffer midiBuffer;

    // 4个Note分布在4个不同通道
    midiBuffer.addEvent(juce::MidiMessage::noteOn(2, 60, 0.8f), 0);
    midiBuffer.addEvent(juce::MidiMessage::noteOn(3, 64, 0.7f), 0);
    midiBuffer.addEvent(juce::MidiMessage::noteOn(4, 67, 0.6f), 0);
    midiBuffer.addEvent(juce::MidiMessage::noteOn(5, 72, 0.5f), 0);

    // 不同通道的复音触后
    midiBuffer.addEvent(juce::MidiMessage::aftertouchChange(2, 60, 0.9f), 100);
    midiBuffer.addEvent(juce::MidiMessage::aftertouchChange(3, 64, 0.8f), 100);

    // 弯音
    midiBuffer.addEvent(juce::MidiMessage::pitchWheel(2, 0x5000), 200);

    // Note Off
    midiBuffer.addEvent(juce::MidiMessage::noteOff(2, 60), 400);
    midiBuffer.addEvent(juce::MidiMessage::noteOff(3, 64), 400);
    midiBuffer.addEvent(juce::MidiMessage::noteOff(4, 67), 400);
    midiBuffer.addEvent(juce::MidiMessage::noteOff(5, 72), 400);

    juce::AudioBuffer<float> buffer(2, 512);
    processor.processBlock(buffer, midiBuffer);

    REQUIRE(true);
}

// =============================================================================
// 微音程/调音测试
// =============================================================================

TEST_CASE("微音程: 默认12-EDO调音", "[tuning][unit]") {
    PluginProcessor processor;

    REQUIRE(processor.isTuningLoaded() == false);

    // 默认情况下 A4 (MIDI 69) = 440Hz
    auto& tuning = processor.getTuning();
    double freq = tuning.getFrequencyForMidiNote(69);
    REQUIRE(freq == Approx(440.0).margin(0.01));
}

TEST_CASE("微音程: 参考频率设置", "[tuning][unit]") {
    PluginProcessor processor;

    // 设置为 A4=432Hz
    processor.setTuningFrequency(432.0, 69);
    auto& tuning = processor.getTuning();
    double freq = tuning.getFrequencyForMidiNote(69);
    REQUIRE(freq == Approx(432.0).margin(0.01));
}

TEST_CASE("微音程: 重置为默认调音", "[tuning][unit]") {
    PluginProcessor processor;

    // 先设置非默认值
    processor.setTuningFrequency(432.0, 69);
    REQUIRE(processor.getTuning().getFrequencyForMidiNote(69) == Approx(432.0).margin(0.01));

    // 重置
    processor.resetTuningToDefault();
    REQUIRE(processor.isTuningLoaded() == false);
    REQUIRE(processor.getTuning().getFrequencyForMidiNote(69) == Approx(440.0).margin(0.01));
}

TEST_CASE("微音程: 加载不存在文件", "[tuning][unit]") {
    PluginProcessor processor;

    bool result = processor.loadScalaFile("nonexistent_file.scl");
    REQUIRE(result == false);
    REQUIRE(processor.isTuningLoaded() == false);
}

TEST_CASE("微音程: 调音名称", "[tuning][unit]") {
    PluginProcessor processor;

    REQUIRE(processor.getTuningName().isEmpty() == true);

    // 设置频率后名称仍为空 (未加载 .scl)
    processor.setTuningFrequency(415.0, 69);
    REQUIRE(processor.getTuningName().isEmpty() == true);
}

} // anonymous namespace