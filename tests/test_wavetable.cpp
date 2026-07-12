// =============================================================================
// LianCore - 波表引擎测试套件
// 验收标准: WT-001, WT-003, WT-004, WT-005
// =============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "WavetableBank.h"
#include "WavetableOscillator.h"
#include "VirtualAnalogOscillator.h"
#include "NoiseGenerator.h"
#include "FilterProcessor.h"

using namespace LianCore;

// =============================================================================
// WT-001: 波表生成测试
// =============================================================================
TEST_CASE("WavetableBank: 波形生成", "[wavetable][wt-001]") {
    WavetableBank bank;

    SECTION("生成正弦波波表") {
        bank.generateSineWave(1);
        REQUIRE(bank.getNumFrames() == 256);
        REQUIRE(bank.getFrameSize() == 2048);
        REQUIRE(!bank.isEmpty());

        auto* frame0 = bank.getFrameData(0);
        // 验证正弦波: 第一个采样应接近0
        REQUIRE(std::abs(frame0[0]) < 0.01f);
        // 验证周期: 第2048个采样也应接近0
        REQUIRE(std::abs(frame0[2047]) < 0.1f);
    }

    SECTION("生成锯齿波波表") {
        bank.generateSawWave(64);
        REQUIRE(bank.getNumFrames() == 256);
        auto* frame0 = bank.getFrameData(0);
        // 锯齿波: 开始应接近0
        REQUIRE(std::abs(frame0[0]) < 0.01f);
    }

    SECTION("生成方波波表") {
        bank.generateSquareWave(64);
        REQUIRE(bank.getNumFrames() == 256);
    }

    SECTION("生成三角波波表") {
        bank.generateTriangleWave(64);
        REQUIRE(bank.getNumFrames() == 256);
    }

    SECTION("内存使用") {
        bank.generateSineWave(1);
        size_t mem = bank.getMemoryUsage();
        REQUIRE(mem == 256 * 2048 * sizeof(float));
    }

    SECTION("清空波表") {
        bank.clear();
        REQUIRE(bank.isEmpty());
        REQUIRE(bank.getNumFrames() == 0);
    }
}

// =============================================================================
// WT-003: 波表帧插值测试
// =============================================================================
TEST_CASE("WavetableBank: 帧插值", "[wavetable][wt-003]") {
    WavetableBank bank;
    bank.generateSineWave(1);

    SECTION("帧内插值") {
        float output[256];
        bank.getInterpolatedFrame(0.0f, output, 256);
        // 验证输出非零
        float maxAbs = 0.0f;
        for (int i = 0; i < 256; ++i) {
            maxAbs = std::max(maxAbs, std::abs(output[i]));
        }
        REQUIRE(maxAbs > 0.0f);
    }

    SECTION("帧间插值") {
        float output[256];
        bank.getInterpolatedFrame(127.5f, output, 256);
        float maxAbs = 0.0f;
        for (int i = 0; i < 256; ++i) {
            maxAbs = std::max(maxAbs, std::abs(output[i]));
        }
        REQUIRE(maxAbs > 0.0f);
    }
}

// =============================================================================
// WT-004: 波表振荡器测试
// =============================================================================
TEST_CASE("WavetableOscillator: 基本功能", "[wavetable][wt-004]") {
    WavetableOscillator osc("TestOSC");

    osc.prepareToPlay(44100.0, 256);

    SECTION("默认状态") {
        REQUIRE(osc.getNumParameters() == 10);
    }

    SECTION("频率设置") {
        osc.setFrequency(440.0f);
        REQUIRE(osc.getParameter(0) == Catch::Approx(440.0f / 20000.0f).margin(0.001f));
    }

    SECTION("波表帧设置") {
        osc.setFrameIndex(64.0f);
        REQUIRE(osc.getParameter(1) == Catch::Approx(64.0f / 255.0f).margin(0.001f));
    }

    SECTION("Unison设置") {
        osc.setUnisonVoices(4);
        REQUIRE(osc.getParameter(4) == Catch::Approx(4.0f / 16.0f).margin(0.001f));
    }

    SECTION("音频处理") {
        juce::AudioBuffer<float> buffer(2, 256);
        juce::MidiBuffer midi;
        buffer.clear();
        osc.processBlock(buffer, midi);

        // 验证输出非零
        float maxAbs = 0.0f;
        for (int ch = 0; ch < 2; ++ch) {
            for (int i = 0; i < 256; ++i) {
                maxAbs = std::max(maxAbs, std::abs(buffer.getSample(ch, i)));
            }
        }
        REQUIRE(maxAbs > 0.0f);
    }
}

// =============================================================================
// 虚拟模拟振荡器测试
// =============================================================================
TEST_CASE("VirtualAnalogOscillator: 波形输出", "[synth][va_osc]") {
    VirtualAnalogOscillator osc("VA_OSC");
    osc.prepareToPlay(44100.0, 256);

    SECTION("正弦波") {
        osc.setWaveform(VAWaveform::Sine);
        osc.setFrequency(440.0f);
        juce::AudioBuffer<float> buffer(2, 256);
        juce::MidiBuffer midi;
        buffer.clear();
        osc.processBlock(buffer, midi);

        float maxAbs = 0.0f;
        for (int i = 0; i < 256; ++i) {
            maxAbs = std::max(maxAbs, std::abs(buffer.getSample(0, i)));
        }
        REQUIRE(maxAbs > 0.0f);
    }

    SECTION("锯齿波") {
        osc.setWaveform(VAWaveform::Saw);
        osc.setFrequency(440.0f);
        juce::AudioBuffer<float> buffer(2, 256);
        juce::MidiBuffer midi;
        osc.processBlock(buffer, midi);

        float maxAbs = 0.0f;
        for (int i = 0; i < 256; ++i) {
            maxAbs = std::max(maxAbs, std::abs(buffer.getSample(0, i)));
        }
        REQUIRE(maxAbs > 0.0f);
    }
}

// =============================================================================
// 噪声发生器测试
// =============================================================================
TEST_CASE("NoiseGenerator: 噪声类型", "[synth][noise]") {
    NoiseGenerator noise("Noise");
    noise.prepareToPlay(44100.0, 256);

    SECTION("白噪声") {
        noise.setNoiseType(NoiseType::White);
        juce::AudioBuffer<float> buffer(2, 256);
        juce::MidiBuffer midi;
        noise.processBlock(buffer, midi);

        // 白噪声应包含正负值
        bool hasPositive = false, hasNegative = false;
        for (int i = 0; i < 256; ++i) {
            float v = buffer.getSample(0, i);
            if (v > 0.0f) hasPositive = true;
            if (v < 0.0f) hasNegative = true;
        }
        REQUIRE(hasPositive);
        REQUIRE(hasNegative);
    }

    SECTION("粉红噪声") {
        noise.setNoiseType(NoiseType::Pink);
        juce::AudioBuffer<float> buffer(2, 256);
        juce::MidiBuffer midi;
        noise.processBlock(buffer, midi);

        float maxAbs = 0.0f;
        for (int i = 0; i < 256; ++i) {
            maxAbs = std::max(maxAbs, std::abs(buffer.getSample(0, i)));
        }
        REQUIRE(maxAbs > 0.0f);
    }
}

// =============================================================================
// 滤波器测试
// =============================================================================
TEST_CASE("FilterProcessor: 滤波模式", "[synth][filter]") {
    FilterProcessor filter("Filter");
    filter.prepareToPlay(44100.0, 256);

    SECTION("低通滤波") {
        filter.setFilterMode(FilterMode::LowPass);
        filter.setCutoff(1000.0f);
        filter.setResonance(0.5f);

        juce::AudioBuffer<float> buffer(2, 256);
        juce::MidiBuffer midi;

        // 输入白噪声
        for (int i = 0; i < 256; ++i) {
            buffer.setSample(0, i, (rand() % 2000 - 1000) / 1000.0f);
        }

        filter.processBlock(buffer, midi);

        float maxAbs = 0.0f;
        for (int i = 0; i < 256; ++i) {
            maxAbs = std::max(maxAbs, std::abs(buffer.getSample(0, i)));
        }
        REQUIRE(maxAbs > 0.0f);
    }

    SECTION("高通滤波") {
        filter.setFilterMode(FilterMode::HighPass);
        filter.setCutoff(5000.0f);
    }

    SECTION("参数切换") {
        filter.setFilterMode(FilterMode::BandPass);
        REQUIRE(filter.getParameter(0) == Catch::Approx(2.0f / 4.0f).margin(0.01f));
    }
}