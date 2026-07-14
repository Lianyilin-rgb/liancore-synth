// =============================================================================
// LianCore - 微音程/调音单元测试
// P0-3: 测试 Scala .scl 加载、音阶解析、MIDI音符映射
// =============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <JuceHeader.h>
#include "../../src/tuning/ScalaFileLoader.h"
#include "../../src/tuning/MicrotuningManager.h"

using namespace LianCore::Tuning;
using Catch::Approx;

// =============================================================================
// ScalaFileLoader: 基本加载
// =============================================================================

TEST_CASE("ScalaFileLoader: load valid 12-EDO scl string", "[tuning][unit]") {
    ScalaFileLoader loader;
    std::string scl = 
        "! 12-EDO.scl\n"
        "!\n"
        "12-tone Equal Temperament\n"
        " 12\n"
        "!\n"
        " 0.0\n"
        " 100.0\n"
        " 200.0\n"
        " 300.0\n"
        " 400.0\n"
        " 500.0\n"
        " 600.0\n"
        " 700.0\n"
        " 800.0\n"
        " 900.0\n"
        " 1000.0\n"
        " 1100.0\n";

    bool success = loader.loadFromString(scl);
    REQUIRE(success == true);
    REQUIRE(loader.isLoaded() == true);
    REQUIRE(loader.getScale().numNotes == 12);
    REQUIRE(loader.getScale().cents.size() == 12);
    REQUIRE(loader.getScale().cents[0] == Approx(0.0));
    REQUIRE(loader.getScale().cents[7] == Approx(700.0));
    REQUIRE(loader.getScale().cents[11] == Approx(1100.0));
}

TEST_CASE("ScalaFileLoader: load valid scl with ratios", "[tuning][unit]") {
    ScalaFileLoader loader;
    std::string scl = 
        "! just_intonation.scl\n"
        "!\n"
        "Just Intonation\n"
        " 3\n"
        "!\n"
        " 1/1\n"
        " 9/8\n"
        " 5/4\n";

    bool success = loader.loadFromString(scl);
    REQUIRE(success == true);
    REQUIRE(loader.getScale().numNotes == 3);

    // 9/8 ≈ 203.91 cents, 5/4 ≈ 386.31 cents
    REQUIRE(loader.getScale().cents[0] == Approx(0.0));
    REQUIRE(loader.getScale().cents[1] == Approx(203.91).margin(0.1));
    REQUIRE(loader.getScale().cents[2] == Approx(386.31).margin(0.1));
}

TEST_CASE("ScalaFileLoader: load 5-tone scale", "[tuning][unit]") {
    ScalaFileLoader loader;
    std::string scl = 
        "! slendro.scl\n"
        "!\n"
        "Slendro\n"
        " 5\n"
        "!\n"
        " 0.0\n"
        " 240.0\n"
        " 480.0\n"
        " 720.0\n"
        " 960.0\n";

    bool success = loader.loadFromString(scl);
    REQUIRE(success == true);
    REQUIRE(loader.getScale().numNotes == 5);
    REQUIRE(loader.getScale().cents.size() == 5);
}

TEST_CASE("ScalaFileLoader: reject invalid note count", "[tuning][unit]") {
    ScalaFileLoader loader;
    std::string scl = " 0\n";  // 0 notes is invalid

    bool success = loader.loadFromString(scl);
    REQUIRE(success == false);
    REQUIRE(loader.isLoaded() == false);
}

TEST_CASE("ScalaFileLoader: reject note count mismatch", "[tuning][unit]") {
    ScalaFileLoader loader;
    std::string scl = 
        " 5\n"
        " 0.0\n"
        " 100.0\n";  // only 2 notes, expected 5

    bool success = loader.loadFromString(scl);
    REQUIRE(success == false);
}

TEST_CASE("ScalaFileLoader: empty content", "[tuning][unit]") {
    ScalaFileLoader loader;
    bool success = loader.loadFromString("");
    REQUIRE(success == false);
}

TEST_CASE("ScalaFileLoader: extraction of scale name from comment", "[tuning][unit]") {
    ScalaFileLoader loader;
    std::string scl = 
        "! my_custom_scale.scl\n"
        "!\n"
        "Custom Scale\n"
        " 3\n"
        "!\n"
        " 0.0\n"
        " 200.0\n"
        " 400.0\n";

    bool success = loader.loadFromString(scl);
    REQUIRE(success == true);
    REQUIRE(loader.getScale().name.find("my_custom_scale") != std::string::npos);
}

// =============================================================================
// ScalaFileLoader: 预设音阶
// =============================================================================

TEST_CASE("ScalaFileLoader: create12EDO", "[tuning][unit]") {
    auto scale = ScalaFileLoader::create12EDO();
    REQUIRE(scale.numNotes == 12);
    REQUIRE(scale.cents.size() == 12);
    REQUIRE(scale.cents[0] == Approx(0.0));
    REQUIRE(scale.cents[7] == Approx(700.0));
    REQUIRE(scale.cents[11] == Approx(1100.0));
}

TEST_CASE("ScalaFileLoader: create24EDO", "[tuning][unit]") {
    auto scale = ScalaFileLoader::create24EDO();
    REQUIRE(scale.numNotes == 24);
    REQUIRE(scale.cents.size() == 24);
    REQUIRE(scale.cents[0] == Approx(0.0));
    REQUIRE(scale.cents[1] == Approx(50.0));
    REQUIRE(scale.cents[23] == Approx(1150.0));
}

TEST_CASE("ScalaFileLoader: getPresetNames returns all presets", "[tuning][unit]") {
    auto names = ScalaFileLoader::getPresetNames();
    REQUIRE(names.size() == 7);
    REQUIRE(names[0] == "12-EDO (Equal Temperament)");
    REQUIRE(names[1] == "24-EDO (Quarter Tone)");
}

TEST_CASE("ScalaFileLoader: loadPreset for each preset", "[tuning][unit]") {
    auto names = ScalaFileLoader::getPresetNames();
    for (const auto& name : names) {
        auto scale = ScalaFileLoader::loadPreset(name);
        REQUIRE(scale.isEmpty() == false);
        REQUIRE(scale.numNotes > 0);
        REQUIRE(scale.cents.size() == (size_t)scale.numNotes);
    }
}

// =============================================================================
// MicrotuningManager: 基本功能
// =============================================================================

TEST_CASE("MicrotuningManager: default initialization", "[tuning][unit]") {
    MicrotuningManager mgr;
    REQUIRE(mgr.isTuningLoaded() == true);
    REQUIRE(mgr.getCurrentScaleName() == "12-EDO.scl");
    REQUIRE(mgr.getBaseFrequency() == Approx(440.0));
}

TEST_CASE("MicrotuningManager: load preset", "[tuning][unit]") {
    MicrotuningManager mgr;
    bool success = mgr.loadPreset("Just Intonation (Major)");
    REQUIRE(success == true);
    REQUIRE(mgr.isTuningLoaded() == true);
    REQUIRE(mgr.getCurrentScaleName().find("Just Intonation") != std::string::npos);
}

TEST_CASE("MicrotuningManager: load invalid preset", "[tuning][unit]") {
    MicrotuningManager mgr;
    // 加载无效预设后仍保持原有音阶
    std::string prevName = mgr.getCurrentScaleName();
    bool success = mgr.loadPreset("NonExistent Tuning");
    // loadPreset 返回 false 但不会改变当前音阶
    REQUIRE(mgr.isTuningLoaded() == true);
}

TEST_CASE("MicrotuningManager: reset to default", "[tuning][unit]") {
    MicrotuningManager mgr;
    mgr.loadPreset("Pythagorean");
    REQUIRE(mgr.getCurrentScaleName().find("Pythagorean") != std::string::npos);

    mgr.resetToDefault();
    REQUIRE(mgr.getCurrentScaleName() == "12-EDO.scl");
    REQUIRE(mgr.getBaseFrequency() == Approx(440.0));
}

TEST_CASE("MicrotuningManager: set base frequency", "[tuning][unit]") {
    MicrotuningManager mgr;
    mgr.setBaseFrequency(432.0);
    REQUIRE(mgr.getBaseFrequency() == Approx(432.0));
}

// =============================================================================
// MicrotuningManager: 调音偏移计算
// =============================================================================

TEST_CASE("MicrotuningManager: 12-EDO has zero offset", "[tuning][unit]") {
    MicrotuningManager mgr;
    mgr.resetToDefault();

    // 12-EDO: 所有音符偏移应为 0
    REQUIRE(mgr.getTuningOffset(0) == Approx(0.0).margin(0.01));
    REQUIRE(mgr.getTuningOffset(60) == Approx(0.0).margin(0.01));  // C4
    REQUIRE(mgr.getTuningOffset(69) == Approx(0.0).margin(0.01));  // A4
    REQUIRE(mgr.getTuningOffset(127) == Approx(0.0).margin(0.01));
}

TEST_CASE("MicrotuningManager: 12-EDO frequency matches standard", "[tuning][unit]") {
    MicrotuningManager mgr;
    mgr.resetToDefault();

    // A4 (MIDI 69) = 440Hz
    double freq = mgr.getNoteFrequency(69);
    REQUIRE(freq == Approx(440.0).margin(0.1));

    // A5 (MIDI 81) = 880Hz
    double freqA5 = mgr.getNoteFrequency(81);
    REQUIRE(freqA5 == Approx(880.0).margin(0.1));

    // C4 (MIDI 60) ≈ 261.63Hz
    double freqC4 = mgr.getNoteFrequency(60);
    REQUIRE(freqC4 == Approx(261.63).margin(0.1));
}

TEST_CASE("MicrotuningManager: Just Intonation has non-zero offsets", "[tuning][unit]") {
    MicrotuningManager mgr;
    mgr.loadPreset("Just Intonation (Major)");

    // Just Intonation 的偏移不应全为零
    double offset3 = mgr.getTuningOffset(3);  // E (M3 in JI)
    REQUIRE(offset3 != Approx(0.0).margin(0.01));
}

TEST_CASE("MicrotuningManager: tuning ratio is consistent", "[tuning][unit]") {
    MicrotuningManager mgr;
    mgr.resetToDefault();

    // 12-EDO: 倍率应为 1.0
    REQUIRE(mgr.getTuningRatio(60) == Approx(1.0).margin(0.001));
    REQUIRE(mgr.getTuningRatio(69) == Approx(1.0).margin(0.001));
}

TEST_CASE("MicrotuningManager: negative MIDI note", "[tuning][unit]") {
    MicrotuningManager mgr;
    mgr.resetToDefault();

    // 负 MIDI 音符应正确处理
    double freq = mgr.getNoteFrequency(-12);
    REQUIRE(freq > 0.0);
    REQUIRE(freq < 440.0);
}

TEST_CASE("MicrotuningManager: high MIDI note", "[tuning][unit]") {
    MicrotuningManager mgr;
    mgr.resetToDefault();

    // 高 MIDI 音符 (127) 应产生有效频率
    double freq = mgr.getNoteFrequency(127);
    REQUIRE(freq > 0.0);
    REQUIRE(freq > 10000.0);
}

TEST_CASE("MicrotuningManager: octave consistency in 12-EDO", "[tuning][unit]") {
    MicrotuningManager mgr;
    mgr.resetToDefault();

    // 同音符不同八度: 频率应为 2 倍关系
    double freqC4 = mgr.getNoteFrequency(60);
    double freqC5 = mgr.getNoteFrequency(72);
    REQUIRE(freqC5 == Approx(freqC4 * 2.0).margin(0.2));
}

// =============================================================================
// MicrotuningManager: 边界条件
// =============================================================================

TEST_CASE("MicrotuningManager: getAvailablePresets", "[tuning][unit]") {
    auto presets = MicrotuningManager::getAvailablePresets();
    REQUIRE(presets.size() == 7);
}

TEST_CASE("MicrotuningManager: load all presets sequentially", "[tuning][unit]") {
    MicrotuningManager mgr;
    auto presets = MicrotuningManager::getAvailablePresets();
    for (const auto& preset : presets) {
        bool success = mgr.loadPreset(preset);
        REQUIRE(success == true);
        REQUIRE(mgr.isTuningLoaded() == true);
    }
}

TEST_CASE("MicrotuningManager: scale definition frequency ratio", "[tuning][unit]") {
    auto scale = ScalaFileLoader::create12EDO();
    // 第7个音符 (G) = 700 cents, 频率倍率 = 2^(700/1200) ≈ 1.498
    REQUIRE(scale.getFrequencyRatio(7) == Approx(1.498).margin(0.001));
    // 第12个音符不在音阶中, 应返回 1.0
    REQUIRE(scale.getFrequencyRatio(12) == Approx(1.0));
}