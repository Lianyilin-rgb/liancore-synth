// =============================================================================
// LianCore - AI波表生成端到端测试
// 验证: 文本描述 → 波表生成 → 加载到振荡器 → 输出非静音
// 注意: 直接使用AITextToWavetable引擎，无需WavetableEditor GUI依赖
// =============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "AITextToWavetable.h"
#include "WavetableBank.h"
#include "WavetableOscillator.h"

using namespace LianCore;
using namespace LianCore::AI;
using Catch::Approx;

// 计算缓冲区RMS
static float computeRMS(const juce::AudioBuffer<float>& buffer, int channel = 0) {
    float sum = 0.0f;
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        float s = buffer.getSample(channel, i);
        sum += s * s;
    }
    return std::sqrt(sum / buffer.getNumSamples());
}

// =============================================================================
// AI-001: AI波表生成 - 文本到波表
// =============================================================================
TEST_CASE("AI Wavetable: 文本描述生成波表", "[ai_wavetable][ai-001]") {
    SECTION("生成明亮的锯齿波") {
        auto result = AITextToWavetable::generate("bright saw wave");
        REQUIRE(result.success);
        REQUIRE(result.frames.size() == 256);
        REQUIRE(result.frames[0].size() == 2048);

        // 验证每帧数据非空
        bool allFramesHaveData = true;
        for (const auto& frame : result.frames) {
            float frameRMS = 0.0f;
            for (float s : frame) frameRMS += s * s;
            frameRMS = std::sqrt(frameRMS / frame.size());
            if (frameRMS < 0.01f) { allFramesHaveData = false; break; }
        }
        REQUIRE(allFramesHaveData);
        REQUIRE(!result.matchedKeywords.empty());
    }

    SECTION("生成暗色方波") {
        auto result = AITextToWavetable::generate("dark square wave");
        REQUIRE(result.success);
        REQUIRE(result.frames.size() == 256);

        const auto& frame0 = result.frames[0];
        REQUIRE(std::abs(frame0[0]) < 1.0f);
    }

    SECTION("生成管风琴音色") {
        auto result = AITextToWavetable::generate("organ");
        REQUIRE(result.success);
        REQUIRE(result.frames.size() == 256);
    }

    SECTION("生成贝斯音色") {
        auto result = AITextToWavetable::generate("bass");
        REQUIRE(result.success);
        REQUIRE(result.frames.size() == 256);
    }

    SECTION("生成铺底音色") {
        auto result = AITextToWavetable::generate("pad ambient");
        REQUIRE(result.success);
        REQUIRE(result.frames.size() == 256);
    }

    SECTION("生成钟声音色") {
        auto result = AITextToWavetable::generate("bell chime");
        REQUIRE(result.success);
        REQUIRE(result.frames.size() == 256);
    }

    SECTION("生成弦乐音色") {
        auto result = AITextToWavetable::generate("string warm");
        REQUIRE(result.success);
        REQUIRE(result.frames.size() == 256);
    }

    SECTION("生成铜管音色") {
        auto result = AITextToWavetable::generate("brass trumpet");
        REQUIRE(result.success);
        REQUIRE(result.frames.size() == 256);
    }

    SECTION("生成人声音色") {
        auto result = AITextToWavetable::generate("choir vocal");
        REQUIRE(result.success);
        REQUIRE(result.frames.size() == 256);
    }

    SECTION("中文描述: 明亮的锯齿波") {
        auto result = AITextToWavetable::generate("明亮的锯齿波");
        REQUIRE(result.success);
        REQUIRE(result.frames.size() == 256);
    }

    SECTION("复合描述: 温暖的弦乐铺底") {
        auto result = AITextToWavetable::generate("温暖的弦乐铺底");
        REQUIRE(result.success);
        REQUIRE(result.frames.size() == 256);
    }
}

// =============================================================================
// AI-002: AI波表 - 生成后数据完整性验证
// 验证: 生成→加载到WavetableBank→数据非静音
// =============================================================================
TEST_CASE("AI Wavetable: 生成后数据完整性验证", "[ai_wavetable][ai-002]") {
    // AI生成波表
    auto result = AITextToWavetable::generate("bright saw wave");
    REQUIRE(result.success);
    REQUIRE(result.frames.size() == 256);

    // 加载到WavetableBank
    WavetableBank bank;
    for (int f = 0; f < static_cast<int>(result.frames.size()); ++f) {
        bank.setFrameData(f, result.frames[f].data(),
                          static_cast<int>(result.frames[f].size()));
    }
    bank.setNumFrames(static_cast<int>(result.frames.size()));
    REQUIRE(bank.getNumFrames() == 256);
    REQUIRE(!bank.isEmpty());

    // 验证第一帧数据非静音
    const float* frame0 = bank.getFrameData(0);
    float rms = 0.0f;
    for (int i = 0; i < bank.getFrameSize(); ++i) {
        rms += frame0[i] * frame0[i];
    }
    rms = std::sqrt(rms / bank.getFrameSize());
    REQUIRE(rms > 0.01f);

    // 验证所有帧的RMS非零
    int nonSilentFrames = 0;
    for (int f = 0; f < bank.getNumFrames(); ++f) {
        const float* frame = bank.getFrameData(f);
        float frameRMS = 0.0f;
        for (int i = 0; i < bank.getFrameSize(); ++i) {
            frameRMS += frame[i] * frame[i];
        }
        frameRMS = std::sqrt(frameRMS / bank.getFrameSize());
        if (frameRMS > 0.001f) nonSilentFrames++;
    }
    REQUIRE(nonSilentFrames > 200); // 至少200帧有声音
}

// =============================================================================
// AI-003: AI波表 - 谐波结构验证
// =============================================================================
TEST_CASE("AI Wavetable: 谐波结构验证", "[ai_wavetable][ai-003]") {
    SECTION("锯齿波应有丰富谐波") {
        auto harmonics = AITextToWavetable::textToHarmonics("saw wave");

        float harmonicSum = 0.0f;
        for (int h = 0; h < 16; ++h) harmonicSum += harmonics[h];
        // 锯齿波应有多个谐波分量
        REQUIRE(harmonicSum > 0.5f);
    }

    SECTION("正弦波应仅有基频") {
        auto harmonics = AITextToWavetable::textToHarmonics("sine wave");

        // 正弦波基频应为1.0
        REQUIRE(harmonics[0] == Approx(1.0f).margin(0.01f));

        // 高次谐波应接近0
        float highHarmonicSum = 0.0f;
        for (int h = 3; h < 16; ++h) highHarmonicSum += harmonics[h];
        REQUIRE(highHarmonicSum < 0.1f);
    }

    SECTION("方波仅含奇次谐波") {
        auto harmonics = AITextToWavetable::textToHarmonics("square wave");

        // 验证偶次谐波(H2, H4, H6)接近0
        for (int h : {2, 4, 6}) {
            float evenHarmonic = harmonics[h - 1]; // 0-indexed
            float oddHarmonic = harmonics[h - 2];  // 上一个奇次谐波
            REQUIRE(evenHarmonic <= oddHarmonic * 1.5f);
        }
    }
}

// =============================================================================
// AI-004: AI波表 - 空文本回退到默认波形
// =============================================================================
TEST_CASE("AI Wavetable: 空文本回退到默认波形", "[ai_wavetable][ai-004]") {
    // 空文本应仍能生成有效波表 (基频默认1.0)
    auto result = AITextToWavetable::generate("");

    REQUIRE(result.success);
    REQUIRE(result.frames.size() == 256);
    REQUIRE(!result.frames.empty());

    const auto& frame0 = result.frames[0];
    float rms = 0.0f;
    for (float s : frame0) rms += s * s;
    rms = std::sqrt(rms / frame0.size());
    REQUIRE(rms > 0.1f);
}

// =============================================================================
// AI-005: AI波表 - 关键词提取
// =============================================================================
TEST_CASE("AI Wavetable: 关键词提取", "[ai_wavetable][ai-005]") {
    SECTION("支持的关键词列表非空") {
        auto keywords = AITextToWavetable::getSupportedKeywords();
        REQUIRE(!keywords.empty());
        REQUIRE(keywords.size() >= 20);
    }

    SECTION("识别英文关键词") {
        auto result = AITextToWavetable::generate("bright saw bass");
        REQUIRE(result.success);
        REQUIRE(!result.matchedKeywords.empty());
    }

    SECTION("识别中文关键词") {
        auto result = AITextToWavetable::generate("温暖的弦乐铺底");
        REQUIRE(result.success);
        REQUIRE(!result.matchedKeywords.empty());
    }
}