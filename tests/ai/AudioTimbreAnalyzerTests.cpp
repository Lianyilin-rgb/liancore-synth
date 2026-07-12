// =============================================================================
// LianCore - 音频音色分析器 单元测试
// Gamma Week 3-4: 音频参考音色复刻
// =============================================================================

#include <catch2/catch_all.hpp>
#include "../src/ai/AudioTimbreAnalyzer.h"
#include <cmath>
#include <algorithm>

using namespace LianCore::AI;

// =============================================================================
// 测试辅助函数
// =============================================================================

/** 生成纯正弦波 */
static juce::AudioBuffer<float> generateSineWave(double frequency, double duration,
                                                   double sampleRate) {
    int numSamples = static_cast<int>(duration * sampleRate);
    juce::AudioBuffer<float> buffer(1, numSamples);
    float* data = buffer.getWritePointer(0);
    for (int i = 0; i < numSamples; ++i) {
        data[i] = std::sin(2.0 * 3.14159265358979323846 * frequency * i / sampleRate);
    }
    return buffer;
}

/** 生成锯齿波 */
static juce::AudioBuffer<float> generateSawWave(double frequency, double duration,
                                                  double sampleRate) {
    int numSamples = static_cast<int>(duration * sampleRate);
    juce::AudioBuffer<float> buffer(1, numSamples);
    float* data = buffer.getWritePointer(0);
    double period = sampleRate / frequency;
    for (int i = 0; i < numSamples; ++i) {
        data[i] = 2.0f * static_cast<float>(std::fmod(i / period, 1.0)) - 1.0f;
    }
    return buffer;
}

/** 生成静音 */
static juce::AudioBuffer<float> generateSilence(int numSamples) {
    juce::AudioBuffer<float> buffer(1, numSamples);
    buffer.clear();
    return buffer;
}

/** 生成白噪声 */
static juce::AudioBuffer<float> generateWhiteNoise(int numSamples) {
    juce::AudioBuffer<float> buffer(1, numSamples);
    float* data = buffer.getWritePointer(0);
    for (int i = 0; i < numSamples; ++i) {
        data[i] = static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f;
    }
    return buffer;
}

// =============================================================================
// 测试用例
// =============================================================================

TEST_CASE("AudioTimbreAnalyzer: construction and defaults", "[audio]") {
    AudioTimbreAnalyzer analyzer;
    
    REQUIRE(analyzer.getEmbeddingDim() == 128);
    REQUIRE(analyzer.getTargetSampleRate() == 44100.0);
    REQUIRE(analyzer.getTargetSampleCount() == 16384);
    REQUIRE(analyzer.getNumParams() == 11);
    REQUIRE(!analyzer.isLoaded());
}

TEST_CASE("AudioTimbreAnalyzer: model loading with valid paths", "[audio]") {
    AudioTimbreAnalyzer analyzer;
    
    juce::File encoderPath("models/audio_encoder.onnx");
    juce::File regressorPath("models/param_regressor.onnx");
    
    if (encoderPath.existsAsFile() && regressorPath.existsAsFile()) {
        bool loaded = analyzer.loadModels(encoderPath, regressorPath);
        REQUIRE(loaded);
        REQUIRE(analyzer.isLoaded());
    } else {
        SUCCEED("Model files not found, skipping integration test");
    }
}

TEST_CASE("AudioTimbreAnalyzer: model loading with invalid paths", "[audio]") {
    AudioTimbreAnalyzer analyzer;
    
    juce::File invalidPath("nonexistent/model.onnx");
    bool loaded = analyzer.loadModels(invalidPath, invalidPath);
    REQUIRE(!loaded);
    REQUIRE(!analyzer.isLoaded());
}

TEST_CASE("AudioTimbreAnalyzer: analyze without loaded models", "[audio]") {
    AudioTimbreAnalyzer analyzer;
    auto audio = generateSineWave(440.0, 0.5, 44100.0);
    
    auto result = analyzer.analyze(audio, 44100.0);
    REQUIRE(!result.isValid());
    REQUIRE(!result.errorMessage.empty());
}

TEST_CASE("AudioTimbreAnalyzer: empty audio buffer handling", "[audio]") {
    AudioTimbreAnalyzer analyzer;
    juce::AudioBuffer<float> emptyBuffer(1, 0);
    
    auto result = analyzer.analyze(emptyBuffer, 44100.0);
    REQUIRE(!result.errorMessage.empty());
}

TEST_CASE("AudioTimbreAnalyzer: analyze sine wave (integration)", "[audio]") {
    AudioTimbreAnalyzer analyzer;
    
    juce::File encoderPath("models/audio_encoder.onnx");
    juce::File regressorPath("models/param_regressor.onnx");
    
    if (!encoderPath.existsAsFile() || !regressorPath.existsAsFile()) {
        SUCCEED("Model files not found, skipping integration test");
        return;
    }
    
    REQUIRE(analyzer.loadModels(encoderPath, regressorPath));
    
    auto audio = generateSineWave(440.0, 0.37, 44100.0);
    auto result = analyzer.analyze(audio, 44100.0);
    
    REQUIRE(result.isValid());
    REQUIRE(result.parameters.size() == 11);
    REQUIRE(result.audioEmbedding.size() == 128);
    REQUIRE(result.confidence >= 0.0f);
    REQUIRE(result.confidence <= 1.0f);
}

TEST_CASE("AudioTimbreAnalyzer: parameter range validation", "[audio]") {
    AudioTimbreAnalyzer analyzer;
    
    juce::File encoderPath("models/audio_encoder.onnx");
    juce::File regressorPath("models/param_regressor.onnx");
    
    if (!encoderPath.existsAsFile() || !regressorPath.existsAsFile()) {
        SUCCEED("Model files not found, skipping integration test");
        return;
    }
    
    REQUIRE(analyzer.loadModels(encoderPath, regressorPath));
    
    // 测试多种输入类型
    auto sineAudio = generateSineWave(440.0, 0.37, 44100.0);
    auto sawAudio = generateSawWave(220.0, 0.37, 44100.0);
    
    for (const auto& audio : {std::ref(sineAudio), std::ref(sawAudio)}) {
        auto result = analyzer.analyze(audio, 44100.0);
        REQUIRE(result.isValid());
        
        for (int i = 0; i < 11; ++i) {
            INFO("Parameter " << i << " = " << result.parameters[i]);
            REQUIRE(result.parameters[i] >= 0.0f);
            REQUIRE(result.parameters[i] <= 1.0f);
        }
    }
}

TEST_CASE("AudioTimbreAnalyzer: zero input (silence) handling", "[audio]") {
    AudioTimbreAnalyzer analyzer;
    
    juce::File encoderPath("models/audio_encoder.onnx");
    juce::File regressorPath("models/param_regressor.onnx");
    
    if (!encoderPath.existsAsFile() || !regressorPath.existsAsFile()) {
        SUCCEED("Model files not found, skipping integration test");
        return;
    }
    
    REQUIRE(analyzer.loadModels(encoderPath, regressorPath));
    
    auto silence = generateSilence(16384);
    auto result = analyzer.analyze(silence, 44100.0);
    
    REQUIRE(result.isValid());
    REQUIRE(result.parameters.size() == 11);
    
    // 静音输入应返回合理参数 (不过度拟合到极端值)
    for (int i = 0; i < 11; ++i) {
        INFO("Silence param " << i << " = " << result.parameters[i]);
        REQUIRE(result.parameters[i] >= 0.0f);
        REQUIRE(result.parameters[i] <= 1.0f);
    }
}

TEST_CASE("AudioTimbreAnalyzer: sample rate conversion", "[audio]") {
    AudioTimbreAnalyzer analyzer;
    
    juce::File encoderPath("models/audio_encoder.onnx");
    juce::File regressorPath("models/param_regressor.onnx");
    
    if (!encoderPath.existsAsFile() || !regressorPath.existsAsFile()) {
        SUCCEED("Model files not found, skipping integration test");
        return;
    }
    
    REQUIRE(analyzer.loadModels(encoderPath, regressorPath));
    
    // 测试 48kHz 输入 (应自动重采样至 44.1kHz)
    auto audio48k = generateSineWave(440.0, 0.37, 48000.0);
    auto result = analyzer.analyze(audio48k, 48000.0);
    
    REQUIRE(result.isValid());
    REQUIRE(result.parameters.size() == 11);
}

TEST_CASE("AudioTimbreAnalyzer: noise input handling", "[audio]") {
    AudioTimbreAnalyzer analyzer;
    
    juce::File encoderPath("models/audio_encoder.onnx");
    juce::File regressorPath("models/param_regressor.onnx");
    
    if (!encoderPath.existsAsFile() || !regressorPath.existsAsFile()) {
        SUCCEED("Model files not found, skipping integration test");
        return;
    }
    
    REQUIRE(analyzer.loadModels(encoderPath, regressorPath));
    
    auto noise = generateWhiteNoise(16384);
    auto result = analyzer.analyze(noise, 44100.0);
    
    REQUIRE(result.isValid());
    REQUIRE(result.parameters.size() == 11);
    
    for (int i = 0; i < 11; ++i) {
        REQUIRE(result.parameters[i] >= 0.0f);
        REQUIRE(result.parameters[i] <= 1.0f);
    }
}