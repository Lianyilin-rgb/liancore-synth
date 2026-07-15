// =============================================================================
// LianCore - 效果器单元测试 (P1-2)
// 测试 Chorus, Flanger, Phaser, BitCrusher, RingMod, ConvolutionReverb
// =============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <JuceHeader.h>
#include "../../src/synthesis/Chorus.h"
#include "../../src/synthesis/Flanger.h"
#include "../../src/synthesis/Phaser.h"
#include "../../src/synthesis/BitCrusher.h"
#include "../../src/synthesis/RingMod.h"
#include "../../src/synthesis/ConvolutionReverb.h"

using namespace LianCore;
using Catch::Approx;

// =============================================================================
// 辅助函数: 为效果器设置音频端口
// =============================================================================
static void setupAudioPorts(AudioNode& node) {
    PortDescriptor desc{ "Audio", "", true, 0.0f, -1.0f, 1.0f };
    node.addInputPort("Audio In", desc);
    node.addOutputPort("Audio Out", desc);
}

// =============================================================================
// 辅助函数: 将外部buffer复制到效果器的输入端口，调用processBlock，然后验证输出
// =============================================================================
static void feedAudioAndProcess(AudioNode& node, juce::AudioBuffer<float>& buffer) {
    // 复制输入数据到节点的输入端口缓冲区
    auto& inputBuf = node.getInputBuffer(0);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        inputBuf.copyFrom(ch, 0, buffer, ch, 0, buffer.getNumSamples());
    }
    // 处理
    juce::MidiBuffer midi;
    node.processBlock(buffer, midi);
}

static juce::AudioBuffer<float> createTestBuffer(int numSamples = 256) {
    juce::AudioBuffer<float> buffer(2, numSamples);
    buffer.clear();
    // 填充测试信号: 双声道正弦波
    for (int i = 0; i < numSamples; ++i) {
        float sample = std::sin(2.0f * 3.14159f * 440.0f * i / 44100.0f) * 0.5f;
        buffer.setSample(0, i, sample);
        buffer.setSample(1, i, sample * 0.8f);
    }
    return buffer;
}

// =============================================================================
// Chorus 合声效果器测试
// =============================================================================

TEST_CASE("Chorus: default construction", "[chorus][unit]") {
    Chorus chorus;
    REQUIRE(chorus.getNodeType() == NodeType::Chorus);
    REQUIRE(chorus.getNumParameters() == 4);
    REQUIRE(chorus.getParameter(0) == Approx(0.3f).margin(0.01f)); // rate
    REQUIRE(chorus.getParameter(1) == Approx(0.5f).margin(0.01f)); // depth
    REQUIRE(chorus.getParameter(2) == Approx(0.4f).margin(0.01f)); // mix
}

TEST_CASE("Chorus: parameter setters/getters", "[chorus][unit]") {
    Chorus chorus;
    chorus.setRate(0.8f);
    REQUIRE(chorus.getParameter(0) == Approx(0.8f).margin(0.01f));
    chorus.setDepth(0.2f);
    REQUIRE(chorus.getParameter(1) == Approx(0.2f).margin(0.01f));
    chorus.setMix(0.7f);
    REQUIRE(chorus.getParameter(2) == Approx(0.7f).margin(0.01f));
    chorus.setVoices(3);
    REQUIRE(chorus.getParameter(3) == Approx(2.0f / 3.0f).margin(0.05f));
}

TEST_CASE("Chorus: parameter clamping", "[chorus][unit]") {
    Chorus chorus;
    chorus.setRate(1.5f);
    REQUIRE(chorus.getParameter(0) == Approx(1.0f).margin(0.01f));
    chorus.setRate(-0.5f);
    REQUIRE(chorus.getParameter(0) == Approx(0.0f).margin(0.01f));
    chorus.setVoices(10);
    REQUIRE(chorus.getParameter(3) == Approx(1.0f).margin(0.01f)); // clamped to 4
    chorus.setVoices(0);
    REQUIRE(chorus.getParameter(3) == Approx(0.0f).margin(0.01f)); // clamped to 1
}

TEST_CASE("Chorus: parameter names", "[chorus][unit]") {
    Chorus chorus;
    REQUIRE(chorus.getParameterName(0).isNotEmpty());
    REQUIRE(chorus.getParameterName(1).isNotEmpty());
    REQUIRE(chorus.getParameterName(2).isNotEmpty());
    REQUIRE(chorus.getParameterName(3).isNotEmpty());
}

TEST_CASE("Chorus: processBlock does not crash", "[chorus][unit]") {
    Chorus chorus;
    setupAudioPorts(chorus);
    chorus.prepareToPlay(44100.0, 256);
    auto buffer = createTestBuffer(256);
    juce::MidiBuffer midi;
    // 不应该崩溃
    REQUIRE_NOTHROW(feedAudioAndProcess(chorus, buffer));
}

TEST_CASE("Chorus: processBlock produces output", "[chorus][unit]") {
    Chorus chorus;
    setupAudioPorts(chorus);
    chorus.prepareToPlay(44100.0, 256);
    auto buffer = createTestBuffer(256);
    juce::MidiBuffer midi;
    feedAudioAndProcess(chorus, buffer);

    auto& output = chorus.getOutputBuffer(0);
    bool hasNonZero = false;
    for (int i = 0; i < 256; ++i) {
        if (std::abs(output.getSample(0, i)) > 1e-10f) {
            hasNonZero = true;
            break;
        }
    }
    REQUIRE(hasNonZero == true);
}

TEST_CASE("Chorus: voices parameter affects output", "[chorus][unit]") {
    Chorus chorus1, chorus2;
    setupAudioPorts(chorus1);
    setupAudioPorts(chorus2);
    chorus1.prepareToPlay(44100.0, 256);
    chorus2.prepareToPlay(44100.0, 256);
    chorus1.setVoices(1);
    chorus2.setVoices(4);

    auto buffer1 = createTestBuffer(256);
    auto buffer2 = createTestBuffer(256);
    juce::MidiBuffer midi;

    feedAudioAndProcess(chorus1, buffer1);
    feedAudioAndProcess(chorus2, buffer2);

    // 不同声部数应产生不同输出
    bool different = false;
    for (int i = 0; i < 256; ++i) {
        if (std::abs(chorus1.getOutputBuffer(0).getSample(0, i) -
                     chorus2.getOutputBuffer(0).getSample(0, i)) > 0.001f) {
            different = true;
            break;
        }
    }
    REQUIRE(different == true);
}

TEST_CASE("Chorus: serialization roundtrip", "[chorus][unit]") {
    Chorus chorus;
    chorus.setRate(0.7f);
    chorus.setDepth(0.3f);
    chorus.setMix(0.6f);
    chorus.setVoices(3);

    auto json = chorus.toJson();
    Chorus chorus2;
    chorus2.fromJson(json);

    REQUIRE(chorus2.getParameter(0) == Approx(0.7f).margin(0.01f));
    REQUIRE(chorus2.getParameter(1) == Approx(0.3f).margin(0.01f));
    REQUIRE(chorus2.getParameter(2) == Approx(0.6f).margin(0.01f));
}

// =============================================================================
// Flanger 镶边效果器测试
// =============================================================================

TEST_CASE("Flanger: default construction", "[flanger][unit]") {
    Flanger flanger;
    REQUIRE(flanger.getNodeType() == NodeType::Flanger);
    REQUIRE(flanger.getNumParameters() == 4);
    REQUIRE(flanger.getParameter(0) == Approx(0.3f).margin(0.01f)); // rate
    REQUIRE(flanger.getParameter(1) == Approx(0.5f).margin(0.01f)); // depth
    REQUIRE(flanger.getParameter(2) == Approx(0.5f / 0.95f).margin(0.01f)); // feedback
    REQUIRE(flanger.getParameter(3) == Approx(0.5f).margin(0.01f)); // mix
}

TEST_CASE("Flanger: parameter setters/getters", "[flanger][unit]") {
    Flanger flanger;
    flanger.setRate(0.6f);
    REQUIRE(flanger.getParameter(0) == Approx(0.6f).margin(0.01f));
    flanger.setDepth(0.3f);
    REQUIRE(flanger.getParameter(1) == Approx(0.3f).margin(0.01f));
    flanger.setFeedback(0.7f);
    REQUIRE(flanger.getParameter(2) == Approx(0.7f / 0.95f).margin(0.01f));
    flanger.setMix(0.8f);
    REQUIRE(flanger.getParameter(3) == Approx(0.8f).margin(0.01f));
}

TEST_CASE("Flanger: feedback clamped to 0.95", "[flanger][unit]") {
    Flanger flanger;
    flanger.setFeedback(1.5f);
    REQUIRE(flanger.getParameter(2) == Approx(1.0f).margin(0.01f));
    flanger.setFeedback(-0.5f);
    REQUIRE(flanger.getParameter(2) == Approx(0.0f).margin(0.01f));
}

TEST_CASE("Flanger: processBlock does not crash", "[flanger][unit]") {
    Flanger flanger;
    setupAudioPorts(flanger);
    flanger.prepareToPlay(44100.0, 256);
    auto buffer = createTestBuffer(256);
    juce::MidiBuffer midi;
    REQUIRE_NOTHROW(feedAudioAndProcess(flanger, buffer));
}

TEST_CASE("Flanger: processBlock produces output", "[flanger][unit]") {
    Flanger flanger;
    setupAudioPorts(flanger);
    flanger.prepareToPlay(44100.0, 256);
    auto buffer = createTestBuffer(256);
    juce::MidiBuffer midi;
    feedAudioAndProcess(flanger, buffer);

    auto& output = flanger.getOutputBuffer(0);
    bool hasNonZero = false;
    for (int i = 0; i < 256; ++i) {
        if (std::abs(output.getSample(0, i)) > 1e-10f) {
            hasNonZero = true;
            break;
        }
    }
    REQUIRE(hasNonZero == true);
}

TEST_CASE("Flanger: serialization roundtrip", "[flanger][unit]") {
    Flanger flanger;
    flanger.setRate(0.5f);
    flanger.setDepth(0.7f);
    flanger.setFeedback(0.4f);
    flanger.setMix(0.6f);

    auto json = flanger.toJson();
    Flanger flanger2;
    flanger2.fromJson(json);

    REQUIRE(flanger2.getParameter(0) == Approx(0.5f).margin(0.01f));
    REQUIRE(flanger2.getParameter(1) == Approx(0.7f).margin(0.01f));
    REQUIRE(flanger2.getParameter(2) == Approx(0.4f / 0.95f).margin(0.01f));
    REQUIRE(flanger2.getParameter(3) == Approx(0.6f).margin(0.01f));
}

// =============================================================================
// Phaser 移相器效果器测试
// =============================================================================

TEST_CASE("Phaser: default construction", "[phaser][unit]") {
    Phaser phaser;
    REQUIRE(phaser.getNodeType() == NodeType::Phaser);
    REQUIRE(phaser.getNumParameters() == 5);
    REQUIRE(phaser.getParameter(0) == Approx(0.3f).margin(0.01f)); // rate
    REQUIRE(phaser.getParameter(1) == Approx(0.5f).margin(0.01f)); // depth
    REQUIRE(phaser.getParameter(2) == Approx(0.3f / 0.95f).margin(0.01f)); // feedback
    REQUIRE(phaser.getParameter(3) == Approx(0.5f).margin(0.01f)); // mix
}

TEST_CASE("Phaser: parameter setters/getters", "[phaser][unit]") {
    Phaser phaser;
    phaser.setRate(0.7f);
    REQUIRE(phaser.getParameter(0) == Approx(0.7f).margin(0.01f));
    phaser.setDepth(0.4f);
    REQUIRE(phaser.getParameter(1) == Approx(0.4f).margin(0.01f));
    phaser.setFeedback(0.6f);
    REQUIRE(phaser.getParameter(2) == Approx(0.6f / 0.95f).margin(0.01f));
    phaser.setMix(0.4f);
    REQUIRE(phaser.getParameter(3) == Approx(0.4f).margin(0.01f));
    phaser.setStages(6);
    REQUIRE(phaser.getParameter(4) == Approx(4.0f / 10.0f).margin(0.05f));
}

TEST_CASE("Phaser: stages clamping", "[phaser][unit]") {
    Phaser phaser;
    phaser.setStages(20);
    REQUIRE(phaser.getParameter(4) == Approx(1.0f).margin(0.01f)); // clamped to 12
    phaser.setStages(1);
    REQUIRE(phaser.getParameter(4) == Approx(0.0f).margin(0.01f)); // clamped to 2
}

TEST_CASE("Phaser: processBlock does not crash", "[phaser][unit]") {
    Phaser phaser;
    setupAudioPorts(phaser);
    phaser.prepareToPlay(44100.0, 256);
    auto buffer = createTestBuffer(256);
    juce::MidiBuffer midi;
    REQUIRE_NOTHROW(feedAudioAndProcess(phaser, buffer));
}

TEST_CASE("Phaser: processBlock produces output", "[phaser][unit]") {
    Phaser phaser;
    setupAudioPorts(phaser);
    phaser.prepareToPlay(44100.0, 256);
    auto buffer = createTestBuffer(256);
    juce::MidiBuffer midi;
    feedAudioAndProcess(phaser, buffer);

    auto& output = phaser.getOutputBuffer(0);
    bool hasNonZero = false;
    for (int i = 0; i < 256; ++i) {
        if (std::abs(output.getSample(0, i)) > 1e-10f) {
            hasNonZero = true;
            break;
        }
    }
    REQUIRE(hasNonZero == true);
}

TEST_CASE("Phaser: serialization roundtrip", "[phaser][unit]") {
    Phaser phaser;
    phaser.setRate(0.6f);
    phaser.setDepth(0.5f);
    phaser.setFeedback(0.3f);
    phaser.setMix(0.7f);
    phaser.setStages(6);

    auto json = phaser.toJson();
    Phaser phaser2;
    phaser2.fromJson(json);

    REQUIRE(phaser2.getParameter(0) == Approx(0.6f).margin(0.01f));
    REQUIRE(phaser2.getParameter(1) == Approx(0.5f).margin(0.01f));
    REQUIRE(phaser2.getParameter(2) == Approx(0.3f / 0.95f).margin(0.01f));
    REQUIRE(phaser2.getParameter(3) == Approx(0.7f).margin(0.01f));
}

// =============================================================================
// BitCrusher 比特粉碎效果器测试
// =============================================================================

TEST_CASE("BitCrusher: default construction", "[bitcrusher][unit]") {
    BitCrusher bc;
    REQUIRE(bc.getNodeType() == NodeType::BitCrusher);
    REQUIRE(bc.getNumParameters() == 3);
    REQUIRE(bc.getParameter(0) == Approx(0.5f).margin(0.01f)); // bitDepth
    REQUIRE(bc.getParameter(1) == Approx(0.0f).margin(0.01f)); // sampleRateReduction
    REQUIRE(bc.getParameter(2) == Approx(0.5f).margin(0.01f)); // mix
}

TEST_CASE("BitCrusher: parameter setters/getters", "[bitcrusher][unit]") {
    BitCrusher bc;
    bc.setBitDepth(0.2f);
    REQUIRE(bc.getParameter(0) == Approx(0.2f).margin(0.01f));
    bc.setSampleRateReduction(0.8f);
    REQUIRE(bc.getParameter(1) == Approx(0.8f).margin(0.01f));
    bc.setMix(0.3f);
    REQUIRE(bc.getParameter(2) == Approx(0.3f).margin(0.01f));
}

TEST_CASE("BitCrusher: extreme bit depth produces quantization", "[bitcrusher][unit]") {
    BitCrusher bc;
    setupAudioPorts(bc);
    bc.prepareToPlay(44100.0, 256);
    // 极低位深度 (1 bit)
    bc.setBitDepth(0.0f);
    bc.setMix(1.0f);

    auto buffer = createTestBuffer(256);
    juce::MidiBuffer midi;
    feedAudioAndProcess(bc, buffer);

    // 1-bit量化应该产生阶梯状输出
    auto& output = bc.getOutputBuffer(0);
    int valueChanges = 0;
    float prev = output.getSample(0, 0);
    for (int i = 1; i < 256; ++i) {
        float current = output.getSample(0, i);
        if (std::abs(current - prev) > 0.01f) {
            valueChanges++;
        }
        prev = current;
    }
    // 1-bit信号应该有很少的值变化
    REQUIRE(valueChanges < 100);
}

TEST_CASE("BitCrusher: no reduction passes through clean", "[bitcrusher][unit]") {
    BitCrusher bc;
    setupAudioPorts(bc);
    bc.prepareToPlay(44100.0, 256);
    bc.setBitDepth(1.0f); // 16 bit, 几乎无损
    bc.setSampleRateReduction(0.0f); // 无降采样

    auto buffer = createTestBuffer(256);
    juce::MidiBuffer midi;
    feedAudioAndProcess(bc, buffer);

    auto& output = bc.getOutputBuffer(0);
    // 干信号应该接近原始信号 (0 mix → 0.5 mix → 部分干信号)
    // 检查没有NaN/Inf
    for (int i = 0; i < 256; ++i) {
        float val = output.getSample(0, i);
        REQUIRE(std::isfinite(val));
    }
}

TEST_CASE("BitCrusher: processBlock produces output", "[bitcrusher][unit]") {
    BitCrusher bc;
    setupAudioPorts(bc);
    bc.prepareToPlay(44100.0, 256);
    auto buffer = createTestBuffer(256);
    juce::MidiBuffer midi;
    feedAudioAndProcess(bc, buffer);

    auto& output = bc.getOutputBuffer(0);
    bool hasNonZero = false;
    for (int i = 0; i < 256; ++i) {
        if (std::abs(output.getSample(0, i)) > 1e-10f) {
            hasNonZero = true;
            break;
        }
    }
    REQUIRE(hasNonZero == true);
}

TEST_CASE("BitCrusher: serialization roundtrip", "[bitcrusher][unit]") {
    BitCrusher bc;
    bc.setBitDepth(0.3f);
    bc.setSampleRateReduction(0.6f);
    bc.setMix(0.7f);

    auto json = bc.toJson();
    BitCrusher bc2;
    bc2.fromJson(json);

    REQUIRE(bc2.getParameter(0) == Approx(0.3f).margin(0.01f));
    REQUIRE(bc2.getParameter(1) == Approx(0.6f).margin(0.01f));
    REQUIRE(bc2.getParameter(2) == Approx(0.7f).margin(0.01f));
}

// =============================================================================
// RingMod 环形调制效果器测试
// =============================================================================

TEST_CASE("RingMod: default construction", "[ringmod][unit]") {
    RingMod rm;
    REQUIRE(rm.getNodeType() == NodeType::RingMod);
    REQUIRE(rm.getNumParameters() == 2);
    REQUIRE(rm.getParameter(0) == Approx(0.1f).margin(0.01f)); // frequency
    REQUIRE(rm.getParameter(1) == Approx(0.5f).margin(0.01f)); // mix
}

TEST_CASE("RingMod: parameter setters/getters", "[ringmod][unit]") {
    RingMod rm;
    rm.setFrequency(0.5f);
    REQUIRE(rm.getParameter(0) == Approx(0.5f).margin(0.01f));
    rm.setMix(0.8f);
    REQUIRE(rm.getParameter(1) == Approx(0.8f).margin(0.01f));
}

TEST_CASE("RingMod: processBlock does not crash", "[ringmod][unit]") {
    RingMod rm;
    setupAudioPorts(rm);
    rm.prepareToPlay(44100.0, 256);
    auto buffer = createTestBuffer(256);
    juce::MidiBuffer midi;
    REQUIRE_NOTHROW(feedAudioAndProcess(rm, buffer));
}

TEST_CASE("RingMod: full mix produces modulation", "[ringmod][unit]") {
    RingMod rm;
    setupAudioPorts(rm);
    rm.prepareToPlay(44100.0, 256);
    rm.setMix(1.0f); // 全湿
    rm.setFrequency(0.2f); // ~1000Hz 载波

    auto buffer = createTestBuffer(256);
    juce::MidiBuffer midi;
    feedAudioAndProcess(rm, buffer);

    auto& output = rm.getOutputBuffer(0);
    // 输出应该与输入不同 (调制效果)
    bool different = false;
    for (int i = 0; i < 256; ++i) {
        if (std::abs(output.getSample(0, i) - buffer.getSample(0, i)) > 0.001f) {
            different = true;
            break;
        }
    }
    REQUIRE(different == true);
}

TEST_CASE("RingMod: zero mix passes through dry", "[ringmod][unit]") {
    RingMod rm;
    setupAudioPorts(rm);
    rm.prepareToPlay(44100.0, 256);
    rm.setMix(0.0f); // 全干

    auto buffer = createTestBuffer(256);
    juce::MidiBuffer midi;
    feedAudioAndProcess(rm, buffer);

    auto& output = rm.getOutputBuffer(0);
    // 全干时输出应等于输入
    bool same = true;
    for (int i = 0; i < 256; ++i) {
        if (std::abs(output.getSample(0, i) - buffer.getSample(0, i)) > 0.001f) {
            same = false;
            break;
        }
    }
    REQUIRE(same == true);
}

TEST_CASE("RingMod: no NaN/Inf in output", "[ringmod][unit]") {
    RingMod rm;
    setupAudioPorts(rm);
    rm.prepareToPlay(44100.0, 256);
    rm.setMix(1.0f);

    auto buffer = createTestBuffer(256);
    juce::MidiBuffer midi;
    feedAudioAndProcess(rm, buffer);

    auto& output = rm.getOutputBuffer(0);
    for (int i = 0; i < 256; ++i) {
        REQUIRE(std::isfinite(output.getSample(0, i)));
    }
}

TEST_CASE("RingMod: serialization roundtrip", "[ringmod][unit]") {
    RingMod rm;
    rm.setFrequency(0.7f);
    rm.setMix(0.4f);

    auto json = rm.toJson();
    RingMod rm2;
    rm2.fromJson(json);

    REQUIRE(rm2.getParameter(0) == Approx(0.7f).margin(0.01f));
    REQUIRE(rm2.getParameter(1) == Approx(0.4f).margin(0.01f));
}

// =============================================================================
// ConvolutionReverb 卷积混响效果器测试
// =============================================================================

TEST_CASE("ConvolutionReverb: default construction", "[convreverb][unit]") {
    ConvolutionReverb reverb;
    REQUIRE(reverb.getNodeType() == NodeType::ConvolutionReverb);
    REQUIRE(reverb.getNumParameters() == 4);
    REQUIRE(reverb.getParameter(0) == Approx(0.5f).margin(0.01f)); // size
    REQUIRE(reverb.getParameter(1) == Approx(0.5f).margin(0.01f)); // decay
    REQUIRE(reverb.getParameter(2) == Approx(0.3f).margin(0.01f)); // damping
    REQUIRE(reverb.getParameter(3) == Approx(0.3f).margin(0.01f)); // mix
}

TEST_CASE("ConvolutionReverb: parameter setters/getters", "[convreverb][unit]") {
    ConvolutionReverb reverb;
    setupAudioPorts(reverb);
    reverb.prepareToPlay(44100.0, 256);
    reverb.setSize(0.2f);
    REQUIRE(reverb.getParameter(0) == Approx(0.2f).margin(0.01f));
    reverb.setDecay(0.8f);
    REQUIRE(reverb.getParameter(1) == Approx(0.8f).margin(0.01f));
    reverb.setDamping(0.6f);
    REQUIRE(reverb.getParameter(2) == Approx(0.6f).margin(0.01f));
    reverb.setMix(0.5f);
    REQUIRE(reverb.getParameter(3) == Approx(0.5f).margin(0.01f));
}

TEST_CASE("ConvolutionReverb: processBlock does not crash", "[convreverb][unit]") {
    ConvolutionReverb reverb;
    setupAudioPorts(reverb);
    reverb.prepareToPlay(44100.0, 256);
    auto buffer = createTestBuffer(256);
    juce::MidiBuffer midi;
    REQUIRE_NOTHROW(feedAudioAndProcess(reverb, buffer));
}

TEST_CASE("ConvolutionReverb: processBlock produces output", "[convreverb][unit]") {
    ConvolutionReverb reverb;
    setupAudioPorts(reverb);
    reverb.prepareToPlay(44100.0, 256);
    reverb.setMix(1.0f);
    auto buffer = createTestBuffer(256);
    juce::MidiBuffer midi;
    feedAudioAndProcess(reverb, buffer);

    auto& output = reverb.getOutputBuffer(0);
    bool hasNonZero = false;
    for (int i = 0; i < 256; ++i) {
        if (std::abs(output.getSample(0, i)) > 1e-10f) {
            hasNonZero = true;
            break;
        }
    }
    REQUIRE(hasNonZero == true);
}

TEST_CASE("ConvolutionReverb: size affects IR length", "[convreverb][unit]") {
    ConvolutionReverb reverb1, reverb2;
    setupAudioPorts(reverb1);
    setupAudioPorts(reverb2);
    reverb1.prepareToPlay(44100.0, 256);
    reverb2.prepareToPlay(44100.0, 256);
    reverb1.setSize(0.1f); // 小房间
    reverb2.setSize(0.9f); // 大房间
    reverb1.setMix(1.0f);
    reverb2.setMix(1.0f);

    auto buffer1 = createTestBuffer(256);
    auto buffer2 = createTestBuffer(256);
    juce::MidiBuffer midi;

    feedAudioAndProcess(reverb1, buffer1);
    feedAudioAndProcess(reverb2, buffer2);

    // 不同房间大小应产生不同输出
    bool different = false;
    for (int i = 0; i < 256; ++i) {
        if (std::abs(reverb1.getOutputBuffer(0).getSample(0, i) -
                     reverb2.getOutputBuffer(0).getSample(0, i)) > 0.001f) {
            different = true;
            break;
        }
    }
    REQUIRE(different == true);
}

TEST_CASE("ConvolutionReverb: no NaN/Inf in output", "[convreverb][unit]") {
    ConvolutionReverb reverb;
    setupAudioPorts(reverb);
    reverb.prepareToPlay(44100.0, 256);
    reverb.setMix(1.0f);

    auto buffer = createTestBuffer(256);
    juce::MidiBuffer midi;
    feedAudioAndProcess(reverb, buffer);

    auto& output = reverb.getOutputBuffer(0);
    for (int i = 0; i < 256; ++i) {
        REQUIRE(std::isfinite(output.getSample(0, i)));
    }
}

TEST_CASE("ConvolutionReverb: serialization roundtrip", "[convreverb][unit]") {
    ConvolutionReverb reverb;
    setupAudioPorts(reverb);
    reverb.prepareToPlay(44100.0, 256);
    reverb.setSize(0.3f);
    reverb.setDecay(0.7f);
    reverb.setDamping(0.5f);
    reverb.setMix(0.4f);

    auto json = reverb.toJson();
    ConvolutionReverb reverb2;
    reverb2.fromJson(json);

    REQUIRE(reverb2.getParameter(0) == Approx(0.3f).margin(0.01f));
    REQUIRE(reverb2.getParameter(1) == Approx(0.7f).margin(0.01f));
    REQUIRE(reverb2.getParameter(2) == Approx(0.5f).margin(0.01f));
    REQUIRE(reverb2.getParameter(3) == Approx(0.4f).margin(0.01f));
}