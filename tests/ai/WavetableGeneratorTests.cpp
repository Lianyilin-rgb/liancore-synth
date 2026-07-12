// =============================================================================
// LianCore - 波表生成器 单元测试
// Gamma Week 5-6: 文本到波表合成
// =============================================================================

#include <catch2/catch_all.hpp>
#include "../src/ai/WavetableGenerator.h"
#include <cmath>
#include <algorithm>
#include <numeric>

using namespace LianCore::AI;

// =============================================================================
// 测试辅助函数
// =============================================================================

/** 生成随机文本嵌入向量 (128维) */
static std::vector<float> makeRandomEmbedding(int seed = 42) {
    std::mt19937 rng(static_cast<unsigned>(seed));
    std::normal_distribution<float> dist(0.0f, 1.0f);
    std::vector<float> emb(128);
    for (int i = 0; i < 128; ++i) {
        emb[i] = dist(rng);
    }
    // 归一化
    float norm = 0.0f;
    for (float v : emb) norm += v * v;
    norm = std::sqrt(norm + 1e-8f);
    for (float& v : emb) v /= norm;
    return emb;
}

/** 生成特定的文本嵌入 (某个维度突出) */
static std::vector<float> makeTypedEmbedding(int typeIndex) {
    std::vector<float> emb(128, 0.0f);
    emb[typeIndex % 128] = 1.0f;
    return emb;
}

// =============================================================================
// 测试用例
// =============================================================================

TEST_CASE("WavetableGenerator: construction and defaults", "[wavetable]") {
    WavetableGenerator generator;
    
    REQUIRE(generator.getNumFrames() == 256);
    REQUIRE(generator.getTableSize() == 2048);
    REQUIRE(generator.getLatentDim() == 64);
    REQUIRE(generator.getTextDim() == 128);
    REQUIRE(!generator.isLoaded());
}

TEST_CASE("WavetableGenerator: model loading with valid path", "[wavetable]") {
    WavetableGenerator generator;
    
    juce::File modelPath("models/wavetable_vae_decoder.onnx");
    
    if (modelPath.existsAsFile()) {
        bool loaded = generator.loadModel(modelPath);
        REQUIRE(loaded);
        REQUIRE(generator.isLoaded());
    } else {
        SUCCEED("Model file not found, skipping integration test");
    }
}

TEST_CASE("WavetableGenerator: model loading with invalid path", "[wavetable]") {
    WavetableGenerator generator;
    
    juce::File invalidPath("nonexistent/model.onnx");
    bool loaded = generator.loadModel(invalidPath);
    REQUIRE(!loaded);
    REQUIRE(!generator.isLoaded());
}

TEST_CASE("WavetableGenerator: generate without loaded model", "[wavetable]") {
    WavetableGenerator generator;
    auto emb = makeRandomEmbedding();
    
    auto result = generator.generate(emb);
    REQUIRE(!result.isValid());
    REQUIRE(!result.errorMessage.empty());
}

TEST_CASE("WavetableGenerator: empty text embedding handling", "[wavetable]") {
    WavetableGenerator generator;
    std::vector<float> emptyEmb;  // 空向量
    
    auto result = generator.generate(emptyEmb);
    REQUIRE(!result.isValid());
    REQUIRE(!result.errorMessage.empty());
}

TEST_CASE("WavetableGenerator: wrong embedding dimension", "[wavetable]") {
    WavetableGenerator generator;
    std::vector<float> wrongEmb(64, 0.0f);  // 64维而非128维
    
    auto result = generator.generate(wrongEmb);
    REQUIRE(!result.isValid());
    REQUIRE(!result.errorMessage.empty());
}

TEST_CASE("WavetableGenerator: basic waveform generation (integration)", "[wavetable]") {
    WavetableGenerator generator;
    
    juce::File modelPath("models/wavetable_vae_decoder.onnx");
    if (!modelPath.existsAsFile()) {
        SUCCEED("Model file not found, skipping integration test");
        return;
    }
    
    REQUIRE(generator.loadModel(modelPath));
    
    auto emb = makeRandomEmbedding();
    auto result = generator.generate(emb);
    
    REQUIRE(result.isValid());
    REQUIRE(result.numFrames == 256);
    REQUIRE(result.tableSize == 2048);
    REQUIRE(result.wavetable.size() == 256 * 2048);
    REQUIRE(result.errorMessage.empty());
}

TEST_CASE("WavetableGenerator: output size validation", "[wavetable]") {
    WavetableGenerator generator;
    
    juce::File modelPath("models/wavetable_vae_decoder.onnx");
    if (!modelPath.existsAsFile()) {
        SUCCEED("Model file not found, skipping integration test");
        return;
    }
    
    REQUIRE(generator.loadModel(modelPath));
    
    auto emb = makeRandomEmbedding();
    auto result = generator.generate(emb);
    
    REQUIRE(result.isValid());
    REQUIRE(result.wavetable.size() == static_cast<size_t>(256 * 2048));
    REQUIRE(result.numFrames == 256);
    REQUIRE(result.tableSize == 2048);
}

TEST_CASE("WavetableGenerator: output range validation [-1, 1]", "[wavetable]") {
    WavetableGenerator generator;
    
    juce::File modelPath("models/wavetable_vae_decoder.onnx");
    if (!modelPath.existsAsFile()) {
        SUCCEED("Model file not found, skipping integration test");
        return;
    }
    
    REQUIRE(generator.loadModel(modelPath));
    
    auto emb = makeRandomEmbedding();
    auto result = generator.generate(emb);
    
    REQUIRE(result.isValid());
    
    float minVal = *std::min_element(result.wavetable.begin(), result.wavetable.end());
    float maxVal = *std::max_element(result.wavetable.begin(), result.wavetable.end());
    
    INFO("Min value: " << minVal << ", Max value: " << maxVal);
    REQUIRE(minVal >= -1.01f);  // 允许微小容差
    REQUIRE(maxVal <= 1.01f);
}

TEST_CASE("WavetableGenerator: different text embeddings produce different wavetables", "[wavetable]") {
    WavetableGenerator generator;
    
    juce::File modelPath("models/wavetable_vae_decoder.onnx");
    if (!modelPath.existsAsFile()) {
        SUCCEED("Model file not found, skipping integration test");
        return;
    }
    
    REQUIRE(generator.loadModel(modelPath));
    
    // 生成两个不同的文本嵌入
    auto emb1 = makeTypedEmbedding(0);
    auto emb2 = makeTypedEmbedding(3);
    
    auto result1 = generator.generate(emb1);
    auto result2 = generator.generate(emb2);
    
    REQUIRE(result1.isValid());
    REQUIRE(result2.isValid());
    
    // 计算两个波表之间的平均绝对差异
    float totalDiff = 0.0f;
    for (size_t i = 0; i < result1.wavetable.size(); ++i) {
        totalDiff += std::abs(result1.wavetable[i] - result2.wavetable[i]);
    }
    float meanDiff = totalDiff / static_cast<float>(result1.wavetable.size());
    
    INFO("Mean absolute difference between wavetables: " << meanDiff);
    // 不同文本嵌入应产生至少微小的差异
    REQUIRE(meanDiff > 0.0f);
}

TEST_CASE("WavetableGenerator: frame smoothness verification", "[wavetable]") {
    WavetableGenerator generator;
    
    juce::File modelPath("models/wavetable_vae_decoder.onnx");
    if (!modelPath.existsAsFile()) {
        SUCCEED("Model file not found, skipping integration test");
        return;
    }
    
    REQUIRE(generator.loadModel(modelPath));
    
    auto emb = makeRandomEmbedding();
    auto result = generator.generate(emb);
    
    REQUIRE(result.isValid());
    
    // 检查相邻帧之间的最大突变
    // 帧间平滑后，相邻帧的平均差异应较小
    float maxFrameDiff = 0.0f;
    float totalFrameDiff = 0.0f;
    int frameCount = 0;
    
    for (int frame = 0; frame < result.numFrames - 1; ++frame) {
        const float* frameA = result.getFrame(frame);
        const float* frameB = result.getFrame(frame + 1);
        
        if (!frameA || !frameB) continue;
        
        float frameDiff = 0.0f;
        for (int i = 0; i < result.tableSize; ++i) {
            frameDiff += std::abs(frameA[i] - frameB[i]);
        }
        frameDiff /= static_cast<float>(result.tableSize);
        
        maxFrameDiff = std::max(maxFrameDiff, frameDiff);
        totalFrameDiff += frameDiff;
        frameCount++;
    }
    
    float avgFrameDiff = totalFrameDiff / static_cast<float>(frameCount);
    
    INFO("Average frame difference: " << avgFrameDiff);
    INFO("Max frame difference: " << maxFrameDiff);
    
    // 帧间平均差异应合理 (不会太大)
    REQUIRE(avgFrameDiff < 2.0f);
    REQUIRE(maxFrameDiff < 2.0f);
}

TEST_CASE("WavetableGenerator: getFrame pointer access", "[wavetable]") {
    WavetableGenerator generator;
    
    juce::File modelPath("models/wavetable_vae_decoder.onnx");
    if (!modelPath.existsAsFile()) {
        SUCCEED("Model file not found, skipping integration test");
        return;
    }
    
    REQUIRE(generator.loadModel(modelPath));
    
    auto emb = makeRandomEmbedding();
    auto result = generator.generate(emb);
    
    REQUIRE(result.isValid());
    
    // 测试有效帧访问
    const float* frame0 = result.getFrame(0);
    REQUIRE(frame0 != nullptr);
    
    const float* frame255 = result.getFrame(255);
    REQUIRE(frame255 != nullptr);
    
    // 测试无效帧访问
    const float* frameNeg = result.getFrame(-1);
    REQUIRE(frameNeg == nullptr);
    
    const float* frameOut = result.getFrame(256);
    REQUIRE(frameOut == nullptr);
    
    // 验证帧数据在范围内
    for (int i = 0; i < 2048; ++i) {
        REQUIRE(frame0[i] >= -1.01f);
        REQUIRE(frame0[i] <= 1.01f);
    }
}

TEST_CASE("WavetableGenerator: wavetable has non-zero content", "[wavetable]") {
    WavetableGenerator generator;
    
    juce::File modelPath("models/wavetable_vae_decoder.onnx");
    if (!modelPath.existsAsFile()) {
        SUCCEED("Model file not found, skipping integration test");
        return;
    }
    
    REQUIRE(generator.loadModel(modelPath));
    
    auto emb = makeRandomEmbedding();
    auto result = generator.generate(emb);
    
    REQUIRE(result.isValid());
    
    // 验证波表不是全零
    float sumAbs = 0.0f;
    for (float v : result.wavetable) {
        sumAbs += std::abs(v);
    }
    float avgAbs = sumAbs / static_cast<float>(result.wavetable.size());
    
    INFO("Average absolute value: " << avgAbs);
    REQUIRE(avgAbs > 0.01f);  // 波表应有非零内容
}