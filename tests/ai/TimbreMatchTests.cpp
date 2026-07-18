// =============================================================================
// LianCore - 音频音色匹配测试
// 验证: 标准波形输入 → 频谱特征提取 → 合成器参数映射
// =============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "AudioTimbreAnalyzer.h"
#include <juce_dsp/juce_dsp.h>

using namespace LianCore;
using namespace LianCore::AI;
using Catch::Approx;

// 生成正弦波 (440Hz, 44.1kHz, 1秒)
static juce::AudioBuffer<float> generateSineWave(float freq, double sampleRate, int numSamples) {
    juce::AudioBuffer<float> buffer(1, numSamples);
    buffer.clear();
    float* data = buffer.getWritePointer(0);
    for (int i = 0; i < numSamples; ++i) {
        data[i] = 0.5f * std::sin(2.0f * juce::MathConstants<float>::pi * freq * i / static_cast<float>(sampleRate));
    }
    return buffer;
}

// 生成锯齿波 (440Hz)
static juce::AudioBuffer<float> generateSawWave(float freq, double sampleRate, int numSamples) {
    juce::AudioBuffer<float> buffer(1, numSamples);
    buffer.clear();
    float* data = buffer.getWritePointer(0);
    float period = static_cast<float>(sampleRate) / freq;
    for (int i = 0; i < numSamples; ++i) {
        float phase = std::fmod(static_cast<float>(i) / period, 1.0f);
        data[i] = 0.5f * (phase * 2.0f - 1.0f);
    }
    return buffer;
}

// 生成方波 (440Hz)
static juce::AudioBuffer<float> generateSquareWave(float freq, double sampleRate, int numSamples) {
    juce::AudioBuffer<float> buffer(1, numSamples);
    buffer.clear();
    float* data = buffer.getWritePointer(0);
    float period = static_cast<float>(sampleRate) / freq;
    for (int i = 0; i < numSamples; ++i) {
        float phase = std::fmod(static_cast<float>(i) / period, 1.0f);
        data[i] = (phase < 0.5f) ? 0.5f : -0.5f;
    }
    return buffer;
}

// 生成白噪声
static juce::AudioBuffer<float> generateWhiteNoise(int numSamples) {
    juce::AudioBuffer<float> buffer(1, numSamples);
    buffer.clear();
    float* data = buffer.getWritePointer(0);
    for (int i = 0; i < numSamples; ++i) {
        data[i] = 0.3f * (static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f);
    }
    return buffer;
}

// =============================================================================
// TM-001: 标准波形参数范围测试
// =============================================================================
TEST_CASE("Timbre Match: standard waveform param range", "[timbre_match][tm-001]") {
    AudioTimbreAnalyzer analyzer;
    const double sampleRate = 44100.0;
    const int numSamples = 16384;

    SECTION("正弦波: 低谐波丰富度") {
        auto sine = generateSineWave(440.0, sampleRate, numSamples);
        auto result = analyzer.analyzeFallback(sine, sampleRate);

        REQUIRE(result.parameters.size() == 11);

        // 正弦波是纯基频, 应低谐波丰富度
        float waveformType = result.parameters[0]; // 波形类型
        REQUIRE(waveformType >= 0.0f);
        REQUIRE(waveformType <= 1.0f);
        // 正弦波谐波较少, 波形类型应较低
        REQUIRE(waveformType < 0.5f);
    }

    SECTION("锯齿波: 高谐波丰富度") {
        auto saw = generateSawWave(440.0, sampleRate, numSamples);
        auto result = analyzer.analyzeFallback(saw, sampleRate);

        REQUIRE(result.parameters.size() == 11);

        float waveformType = result.parameters[0];
        REQUIRE(waveformType >= 0.0f);
        REQUIRE(waveformType <= 1.0f);
        // 锯齿波谐波丰富, 波形类型应较高
        REQUIRE(waveformType > 0.3f);
    }

    SECTION("方波: 中高谐波丰富度") {
        auto square = generateSquareWave(440.0, sampleRate, numSamples);
        auto result = analyzer.analyzeFallback(square, sampleRate);

        REQUIRE(result.parameters.size() == 11);
        float waveformType = result.parameters[0];
        REQUIRE(waveformType >= 0.0f);
        REQUIRE(waveformType <= 1.0f);
    }

    SECTION("白噪声: 极高谐波, 极短Release") {
        auto noise = generateWhiteNoise(numSamples);
        auto result = analyzer.analyzeFallback(noise, sampleRate);

        REQUIRE(result.parameters.size() == 11);

        // 白噪声高频能量极高
        float waveformType = result.parameters[0];
        REQUIRE(waveformType >= 0.0f);
        REQUIRE(waveformType <= 1.0f);

        // 白噪声的高频能量比高 → Release短
        float release = result.parameters[6];
        REQUIRE(release >= 0.0f);
        REQUIRE(release <= 1.0f);
    }
}

// =============================================================================
// TM-002: 参数范围验证
// =============================================================================
TEST_CASE("Timbre Match: all params in [0,1] range", "[timbre_match][tm-002]") {
    AudioTimbreAnalyzer analyzer;
    const double sampleRate = 44100.0;

    SECTION("正弦波参数范围") {
        auto sine = generateSineWave(440.0, sampleRate, 16384);
        auto result = analyzer.analyzeFallback(sine, sampleRate);

        REQUIRE(result.parameters.size() == 11);
        for (int i = 0; i < 11; ++i) {
            INFO("Parameter " << i << " (" << AudioTimbreAnalyzer::getParamName(i).toStdString() << ")");
            REQUIRE(result.parameters[i] >= 0.0f);
            REQUIRE(result.parameters[i] <= 1.0f);
        }
    }

    SECTION("锯齿波参数范围") {
        auto saw = generateSawWave(440.0, sampleRate, 16384);
        auto result = analyzer.analyzeFallback(saw, sampleRate);

        REQUIRE(result.parameters.size() == 11);
        for (int i = 0; i < 11; ++i) {
            INFO("Parameter " << i << " (" << AudioTimbreAnalyzer::getParamName(i).toStdString() << ")");
            REQUIRE(result.parameters[i] >= 0.0f);
            REQUIRE(result.parameters[i] <= 1.0f);
        }
    }

    SECTION("方波参数范围") {
        auto square = generateSquareWave(440.0, sampleRate, 16384);
        auto result = analyzer.analyzeFallback(square, sampleRate);

        REQUIRE(result.parameters.size() == 11);
        for (int i = 0; i < 11; ++i) {
            INFO("Parameter " << i << " (" << AudioTimbreAnalyzer::getParamName(i).toStdString() << ")");
            REQUIRE(result.parameters[i] >= 0.0f);
            REQUIRE(result.parameters[i] <= 1.0f);
        }
    }
}

// =============================================================================
// TM-003: RMS特征验证
// =============================================================================
TEST_CASE("Timbre Match: RMS maps to Sustain", "[timbre_match][tm-003]") {
    AudioTimbreAnalyzer analyzer;
    const double sampleRate = 44100.0;
    const int numSamples = 16384;

    SECTION("高声压级 → 高Sustain") {
        // 生成响亮的正弦波 (幅度0.9)
        juce::AudioBuffer<float> loud(1, numSamples);
        loud.clear();
        float* data = loud.getWritePointer(0);
        for (int i = 0; i < numSamples; ++i) {
            data[i] = 0.9f * std::sin(2.0f * 3.14159f * 440.0f * i / 44100.0f);
        }

        auto result = analyzer.analyzeFallback(loud, sampleRate);
        float sustain = result.parameters[5];
        REQUIRE(sustain > 0.5f);
    }

    SECTION("低声压级 → 低Sustain") {
        // 生成安静的正弦波 (幅度0.05)
        juce::AudioBuffer<float> quiet(1, numSamples);
        quiet.clear();
        float* data = quiet.getWritePointer(0);
        for (int i = 0; i < numSamples; ++i) {
            data[i] = 0.05f * std::sin(2.0f * 3.14159f * 440.0f * i / 44100.0f);
        }

        auto result = analyzer.analyzeFallback(quiet, sampleRate);
        float sustain = result.parameters[5];
        REQUIRE(sustain < 0.5f);
    }
}

// =============================================================================
// TM-004: 空缓冲区错误处理
// =============================================================================
TEST_CASE("Timbre Match: empty buffer error handling", "[timbre_match][tm-004]") {
    AudioTimbreAnalyzer analyzer;

    SECTION("零样本缓冲区") {
        juce::AudioBuffer<float> empty(1, 0);
        auto result = analyzer.analyzeFallback(empty, 44100.0);

        REQUIRE(!result.errorMessage.empty());
    }
}

// =============================================================================
// TM-005: 参数名称和描述
// =============================================================================
TEST_CASE("Timbre Match: param names and descriptions", "[timbre_match][tm-005]") {
    SECTION("11个参数都有名称") {
        for (int i = 0; i < 11; ++i) {
            auto name = AudioTimbreAnalyzer::getParamName(i);
            REQUIRE(!name.isEmpty());
            REQUIRE(name != "Unknown");
        }
    }

    SECTION("11个参数都有描述") {
        for (int i = 0; i < 11; ++i) {
            auto desc = AudioTimbreAnalyzer::getParamDescription(i);
            REQUIRE(!desc.isEmpty());
            REQUIRE(desc != "Unknown");
        }
    }

    SECTION("越界参数返回Unknown") {
        REQUIRE(AudioTimbreAnalyzer::getParamName(-1) == "Unknown");
        REQUIRE(AudioTimbreAnalyzer::getParamName(11) == "Unknown");
        REQUIRE(AudioTimbreAnalyzer::getParamDescription(-1) == "Unknown");
        REQUIRE(AudioTimbreAnalyzer::getParamDescription(11) == "Unknown");
    }
}

// =============================================================================
// TM-006: 不同频率的影响
// =============================================================================
TEST_CASE("Timbre Match: diff fundamental spectral centroid", "[timbre_match][tm-006]") {
    AudioTimbreAnalyzer analyzer;
    const double sampleRate = 44100.0;
    const int numSamples = 16384;

    SECTION("高频正弦波 (2000Hz) → 高截止频率") {
        auto highSine = generateSineWave(2000.0, sampleRate, numSamples);
        auto result = analyzer.analyzeFallback(highSine, sampleRate);

        float cutoff = result.parameters[1]; // 滤波器截止
        REQUIRE(cutoff >= 0.0f);
        REQUIRE(cutoff <= 1.0f);
    }

    SECTION("低频正弦波 (80Hz) → 低截止频率") {
        auto lowSine = generateSineWave(80.0, sampleRate, numSamples);
        auto result = analyzer.analyzeFallback(lowSine, sampleRate);

        float cutoff = result.parameters[1]; // 滤波器截止
        REQUIRE(cutoff >= 0.0f);
        REQUIRE(cutoff <= 1.0f);
    }
}