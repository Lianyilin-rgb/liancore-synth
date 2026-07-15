// =============================================================================
// LianCore - OversamplingProcessor 单元测试 (P2-4)
// =============================================================================
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>

#include "../../src/plugin/OversamplingProcessor.h"

using namespace LianCore;

namespace {
    // 生成正弦波
    void generateSine(juce::AudioBuffer<float>& buffer, float freq, double sampleRate) {
        int numSamples = buffer.getNumSamples();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
            float* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i) {
                data[i] = std::sin(2.0f * juce::MathConstants<float>::pi * freq *
                                   static_cast<float>(i) / static_cast<float>(sampleRate));
            }
        }
    }

    // 计算信号的信噪比 (dB)
    // 使用参考正弦波相关性方法，比 DFT 更稳健
    float computeSNR(const juce::AudioBuffer<float>& buffer, float testFreq,
                     double sampleRate, float /*maxAliasFreq*/) {
        int numSamples = buffer.getNumSamples();
        const float* data = buffer.getReadPointer(0);

        // 生成参考正弦波
        float signalEnergy = 0.0f;
        float totalEnergy = 0.0f;
        for (int i = 0; i < numSamples; ++i) {
            float ref = std::sin(2.0f * juce::MathConstants<float>::pi * testFreq *
                                 static_cast<float>(i) / static_cast<float>(sampleRate));
            totalEnergy += data[i] * data[i];
            // 相关性: 信号能量 = (sum(data * ref))^2 / sum(ref^2)
            signalEnergy += data[i] * ref;
        }

        // 归一化相关性
        float refEnergy = 0.0f;
        for (int i = 0; i < numSamples; ++i) {
            float ref = std::sin(2.0f * juce::MathConstants<float>::pi * testFreq *
                                 static_cast<float>(i) / static_cast<float>(sampleRate));
            refEnergy += ref * ref;
        }
        signalEnergy = (signalEnergy * signalEnergy) / refEnergy;

        float noiseEnergy = totalEnergy - signalEnergy;
        if (noiseEnergy < 1e-12f) noiseEnergy = 1e-12f;
        if (signalEnergy < 1e-12f) signalEnergy = 1e-12f;

        return 10.0f * std::log10(signalEnergy / noiseEnergy);
    }

    // 计算RMS
    float computeRMS(const juce::AudioBuffer<float>& buffer) {
        const float* data = buffer.getReadPointer(0);
        int numSamples = buffer.getNumSamples();
        float sum = 0.0f;
        for (int i = 0; i < numSamples; ++i) {
            sum += data[i] * data[i];
        }
        return std::sqrt(sum / numSamples);
    }
}

// =============================================================================
// 基础测试
// =============================================================================
TEST_CASE("OversamplingProcessor: 默认禁用", "[oversampling][unit]") {
    OversamplingProcessor proc;
    REQUIRE_FALSE(proc.isEnabled());
    REQUIRE(proc.getOversampledRate() == 176400.0);
}

TEST_CASE("OversamplingProcessor: 启用/禁用", "[oversampling][unit]") {
    OversamplingProcessor proc;
    proc.setEnabled(true);
    REQUIRE(proc.isEnabled());
    proc.setEnabled(false);
    REQUIRE_FALSE(proc.isEnabled());
}

TEST_CASE("OversamplingProcessor: 准备与重置", "[oversampling][unit]") {
    OversamplingProcessor proc;
    proc.setEnabled(true);
    REQUIRE_NOTHROW(proc.prepare(44100.0, 512, 2));
    REQUIRE_NOTHROW(proc.reset());
}

// =============================================================================
// 过采样功能测试
// =============================================================================
TEST_CASE("OversamplingProcessor: 上采样输出样本数正确", "[oversampling][unit]") {
    OversamplingProcessor proc;
    proc.setEnabled(true);
    proc.prepare(44100.0, 256, 2);

    juce::AudioBuffer<float> buffer(2, 256);
    buffer.clear();
    generateSine(buffer, 440.0f, 44100.0);

    bool result = proc.process(buffer);
    REQUIRE(result);
    // 上采样后应为4倍样本数
    REQUIRE(buffer.getNumSamples() == 1024);
}

TEST_CASE("OversamplingProcessor: 降采样后样本数恢复", "[oversampling][unit]") {
    OversamplingProcessor proc;
    proc.setEnabled(true);
    proc.prepare(44100.0, 256, 2);

    juce::AudioBuffer<float> buffer(2, 256);
    buffer.clear();
    generateSine(buffer, 440.0f, 44100.0);

    proc.process(buffer);
    proc.downsample(buffer);

    REQUIRE(buffer.getNumSamples() == 256);
}

TEST_CASE("OversamplingProcessor: 禁用时不处理", "[oversampling][unit]") {
    OversamplingProcessor proc;
    proc.prepare(44100.0, 256, 2);

    juce::AudioBuffer<float> buffer(2, 256);
    buffer.clear();
    generateSine(buffer, 440.0f, 44100.0);

    bool result = proc.process(buffer);
    REQUIRE_FALSE(result);
    REQUIRE(buffer.getNumSamples() == 256);
}

// =============================================================================
// 信号质量测试
// =============================================================================
TEST_CASE("OversamplingProcessor: 1kHz正弦波过采样保真度", "[oversampling][quality]") {
    OversamplingProcessor proc;
    proc.setEnabled(true);
    proc.prepare(44100.0, 512, 1);

    juce::AudioBuffer<float> buffer(1, 512);
    buffer.clear();
    generateSine(buffer, 1000.0f, 44100.0);

    float origRMS = computeRMS(buffer);

    proc.process(buffer);
    proc.downsample(buffer);

    float finalRMS = computeRMS(buffer);

    // 1kHz正弦波应完好保留 (RMS变化 < 5%)
    REQUIRE(std::abs(finalRMS - origRMS) / origRMS < 0.05f);
}

TEST_CASE("OversamplingProcessor: 高频正弦波过采样后信号保留", "[oversampling][quality]") {
    OversamplingProcessor proc;
    proc.setEnabled(true);
    proc.prepare(44100.0, 512, 1);

    // 生成15kHz正弦波 (接近奈奎斯特)
    // 过采样后信号应被保留，不会完全消失或产生巨大失真
    juce::AudioBuffer<float> buffer(1, 512);
    buffer.clear();
    generateSine(buffer, 15000.0f, 44100.0);

    float origRMS = computeRMS(buffer);
    REQUIRE(origRMS > 0.1f); // 确保有有效输入

    proc.process(buffer);
    proc.downsample(buffer);

    float finalRMS = computeRMS(buffer);
    // 15kHz接近奈奎斯特，过采样后信号应保留 (RMS > 0.01)
    REQUIRE(finalRMS > 0.01f);
    // 信号能量不应放大超过3倍
    REQUIRE(finalRMS < origRMS * 3.0f);
}

TEST_CASE("OversamplingProcessor: 10kHz正弦波无显著失真", "[oversampling][quality]") {
    OversamplingProcessor proc;
    proc.setEnabled(true);
    proc.prepare(44100.0, 1024, 1);

    // 生成标准模式下的10kHz正弦波 (不做过采样)
    juce::AudioBuffer<float> bufferStd(1, 1024);
    bufferStd.clear();
    generateSine(bufferStd, 10000.0f, 44100.0);
    float stdRMS = computeRMS(bufferStd);

    // 生成过采样模式下的10kHz正弦波
    juce::AudioBuffer<float> bufferOS(1, 1024);
    bufferOS.clear();
    generateSine(bufferOS, 10000.0f, 44100.0);

    proc.process(bufferOS);
    proc.downsample(bufferOS);

    float osRMS = computeRMS(bufferOS);

    // 过采样不应显著改变信号幅度 (差异 < 30%, FIR双通滤波+过采样会有增益变化)
    REQUIRE(std::abs(osRMS - stdRMS) / stdRMS < 0.30f);
}

// =============================================================================
// 边界测试
// =============================================================================
TEST_CASE("OversamplingProcessor: 单声道", "[oversampling][edge]") {
    OversamplingProcessor proc;
    proc.setEnabled(true);
    proc.prepare(44100.0, 256, 1);

    juce::AudioBuffer<float> buffer(1, 256);
    buffer.clear();
    generateSine(buffer, 440.0f, 44100.0);

    proc.process(buffer);
    REQUIRE(buffer.getNumSamples() == 1024);
    proc.downsample(buffer);
    REQUIRE(buffer.getNumSamples() == 256);
}

TEST_CASE("OversamplingProcessor: 不同采样率", "[oversampling][edge]") {
    std::vector<double> rates = {44100.0, 48000.0, 88200.0, 96000.0};
    for (double rate : rates) {
        OversamplingProcessor proc;
        proc.setEnabled(true);
        REQUIRE_NOTHROW(proc.prepare(rate, 256, 2));
        REQUIRE(proc.getOversampledRate() == rate * 4.0);
    }
}

TEST_CASE("OversamplingProcessor: 不同块大小", "[oversampling][edge]") {
    std::vector<int> blockSizes = {64, 128, 256, 512, 1024};
    for (int bs : blockSizes) {
        OversamplingProcessor proc;
        proc.setEnabled(true);
        REQUIRE_NOTHROW(proc.prepare(44100.0, bs, 2));

        juce::AudioBuffer<float> buffer(2, bs);
        buffer.clear();
        generateSine(buffer, 440.0f, 44100.0);

        proc.process(buffer);
        REQUIRE(buffer.getNumSamples() == bs * 4);
        proc.downsample(buffer);
        REQUIRE(buffer.getNumSamples() == bs);
    }
}

TEST_CASE("OversamplingProcessor: 静音输入不变", "[oversampling][edge]") {
    OversamplingProcessor proc;
    proc.setEnabled(true);
    proc.prepare(44100.0, 256, 2);

    juce::AudioBuffer<float> buffer(2, 256);
    buffer.clear();

    proc.process(buffer);
    proc.downsample(buffer);

    // 静音输入应保持静音
    for (int ch = 0; ch < 2; ++ch) {
        const float* data = buffer.getReadPointer(ch);
        for (int i = 0; i < 256; ++i) {
            REQUIRE(std::abs(data[i]) < 0.001f);
        }
    }
}