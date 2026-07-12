// =============================================================================
// LianCore - 调制系统测试套件
// 验收标准: MM-001, MM-003, MM-004, MM-005
// =============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "ModulationMatrix.h"
#include "EnvelopeGenerator.h"
#include "LFOGenerator.h"

using namespace LianCore;

// =============================================================================
// 模拟调制源
// =============================================================================
class MockModulationSource : public ModulationSource {
public:
    explicit MockModulationSource(const juce::String& name, float value = 0.5f)
        : name_(name), value_(value) {}

    float getValue() const override { return value_; }
    juce::String getName() const override { return name_; }
    bool isBipolar() const override { return true; }

    void setValue(float v) { value_ = v; }

private:
    juce::String name_;
    float value_;
};

// =============================================================================
// 模拟调制目标
// =============================================================================
class MockModulationTarget : public ModulationTarget {
public:
    explicit MockModulationTarget(const juce::String& name, float minV = 0.0f, float maxV = 1.0f)
        : name_(name), minValue_(minV), maxValue_(maxV) {}

    void applyModulation(float normalizedValue) override {
        currentValue_ = normalizedValue;
        modulationCount_++;
    }

    juce::String getName() const override { return name_; }
    float getCurrentValue() const override { return currentValue_; }
    float getMinValue() const override { return minValue_; }
    float getMaxValue() const override { return maxValue_; }

    int getModulationCount() const { return modulationCount_; }

private:
    juce::String name_;
    float currentValue_ = 0.0f;
    float minValue_;
    float maxValue_;
    int modulationCount_ = 0;
};

// =============================================================================
// MM-001: 调制源注册测试
// =============================================================================
TEST_CASE("ModulationMatrix: 源/目标注册", "[modulation][mm-001]") {
    ModulationMatrix matrix;

    MockModulationSource src("LFO", 0.5f);
    MockModulationTarget tgt("Cutoff", 20.0f, 20000.0f);

    SECTION("注册调制源") {
        matrix.registerSource("lfo", &src);
        REQUIRE(matrix.getSource("lfo") == &src);
    }

    SECTION("注册调制目标") {
        matrix.registerTarget("cutoff", &tgt);
        REQUIRE(matrix.getTarget("cutoff") == &tgt);
    }

    SECTION("注销调制源") {
        matrix.registerSource("lfo", &src);
        matrix.unregisterSource("lfo");
        REQUIRE(matrix.getSource("lfo") == nullptr);
    }

    SECTION("注销调制目标") {
        matrix.registerTarget("cutoff", &tgt);
        matrix.unregisterTarget("cutoff");
        REQUIRE(matrix.getTarget("cutoff") == nullptr);
    }
}

// =============================================================================
// MM-003: 调制路由测试
// =============================================================================
TEST_CASE("ModulationMatrix: 调制路由", "[modulation][mm-003]") {
    ModulationMatrix matrix;

    MockModulationSource src("LFO", 0.7f);
    MockModulationTarget tgt("Cutoff", 20.0f, 20000.0f);

    matrix.registerSource("lfo", &src);
    matrix.registerTarget("cutoff", &tgt);

    SECTION("添加调制路由") {
        int idx = matrix.addModulation("lfo", "cutoff", 0.5f);
        REQUIRE(idx >= 0);
        REQUIRE(matrix.getNumRoutes() == 1);
    }

    SECTION("调制量范围") {
        matrix.addModulation("lfo", "cutoff", 0.5f);
        REQUIRE(matrix.getModulationAmount(0) == Catch::Approx(0.5f).margin(0.001f));
    }

    SECTION("调制量裁剪") {
        matrix.addModulation("lfo", "cutoff", 2.0f);
        REQUIRE(matrix.getModulationAmount(0) == Catch::Approx(1.0f).margin(0.001f));
    }

    SECTION("负向调制") {
        matrix.addModulation("lfo", "cutoff", -0.5f);
        REQUIRE(matrix.getModulationAmount(0) == Catch::Approx(-0.5f).margin(0.001f));
    }

    SECTION("移除调制路由") {
        matrix.addModulation("lfo", "cutoff", 0.5f);
        matrix.removeModulation(0);
        REQUIRE(matrix.getNumRoutes() == 0);
    }

    SECTION("多路由 - 最多32条") {
        for (int i = 0; i < 40; ++i) {
            matrix.addModulation("lfo", "cutoff", 0.1f);
        }
        REQUIRE(matrix.getNumRoutes() <= 32);
    }

    SECTION("processBlock 调用") {
        matrix.addModulation("lfo", "cutoff", 0.5f);
        matrix.processBlock(256);
        REQUIRE(tgt.getModulationCount() > 0);
    }
}

// =============================================================================
// MM-004: 调制精度测试
// =============================================================================
TEST_CASE("ModulationMatrix: 调制精度", "[modulation][mm-004]") {
    ModulationMatrix matrix;

    MockModulationSource src("LFO", 0.5f);
    MockModulationTarget tgt("Cutoff", 20.0f, 20000.0f);

    matrix.registerSource("lfo", &src);
    matrix.registerTarget("cutoff", &tgt);
    matrix.addModulation("lfo", "cutoff", 0.5f);

    SECTION("长时间运行无漂移") {
        for (int i = 0; i < 10000; ++i) {
            matrix.processBlock(256);
        }
        // 目标值应在合理范围
        REQUIRE(tgt.getCurrentValue() >= -1.0f);
        REQUIRE(tgt.getCurrentValue() <= 1.0f);
    }
}

// =============================================================================
// MM-005: 双向调制测试
// =============================================================================
TEST_CASE("ModulationMatrix: 双向调制", "[modulation][mm-005]") {
    ModulationMatrix matrix;

    MockModulationSource src("LFO", -1.0f);
    MockModulationTarget tgt("Cutoff", 20.0f, 20000.0f);

    matrix.registerSource("lfo", &src);
    matrix.registerTarget("cutoff", &tgt);

    SECTION("bipolar调制源") {
        REQUIRE(src.isBipolar() == true);
    }

    SECTION("负值调制") {
        matrix.addModulation("lfo", "cutoff", 1.0f);
        matrix.processBlock(256);
        REQUIRE(tgt.getCurrentValue() < 0.0f);
    }

    SECTION("正值调制") {
        src.setValue(1.0f);
        matrix.addModulation("lfo", "cutoff", 1.0f);
        matrix.processBlock(256);
        REQUIRE(tgt.getCurrentValue() > 0.0f);
    }
}

// =============================================================================
// 可视化快照测试
// =============================================================================
TEST_CASE("ModulationMatrix: 可视化快照", "[modulation][snapshot]") {
    ModulationMatrix matrix;

    MockModulationSource src("LFO", 0.5f);
    MockModulationTarget tgt("Cutoff", 20.0f, 20000.0f);

    matrix.registerSource("lfo", &src);
    matrix.registerTarget("cutoff", &tgt);
    matrix.addModulation("lfo", "cutoff", 0.5f);

    matrix.processBlock(256);

    auto snapshot = matrix.getSnapshot();
    REQUIRE(snapshot.size() == 1);
    REQUIRE(snapshot[0].sourceId == "lfo");
    REQUIRE(snapshot[0].targetId == "cutoff");
    REQUIRE(snapshot[0].amount == Catch::Approx(0.5f).margin(0.001f));
}

// =============================================================================
// 序列化测试
// =============================================================================
TEST_CASE("ModulationMatrix: 序列化", "[modulation][serialization]") {
    ModulationMatrix matrix;

    MockModulationSource src("LFO", 0.5f);
    MockModulationTarget tgt("Cutoff", 20.0f, 20000.0f);

    matrix.registerSource("lfo", &src);
    matrix.registerTarget("cutoff", &tgt);
    matrix.addModulation("lfo", "cutoff", 0.5f);

    auto json = matrix.toJson();
    REQUIRE(json.isArray());

    ModulationMatrix matrix2;
    matrix2.registerSource("lfo", &src);
    matrix2.registerTarget("cutoff", &tgt);
    matrix2.fromJson(json);
    REQUIRE(matrix2.getNumRoutes() == 1);
    REQUIRE(matrix2.getModulationAmount(0) == Catch::Approx(0.5f).margin(0.001f));
}

// =============================================================================
// 包络发生器测试
// =============================================================================
TEST_CASE("EnvelopeGenerator: ADSR", "[modulation][envelope]") {
    EnvelopeGenerator env("TestEnv");
    env.prepareToPlay(44100.0, 256);

    SECTION("初始状态") {
        REQUIRE(env.getCurrentStage() == EnvelopeStage::Idle);
        REQUIRE(env.getCurrentValue() == 0.0f);
    }

    SECTION("参数设置") {
        env.setAttack(5.0f);
        REQUIRE(env.getParameter(0) == Catch::Approx(5.0f / 10000.0f).margin(0.01f));

        env.setSustain(0.7f);
        REQUIRE(env.getParameter(2) == Catch::Approx(0.7f).margin(0.01f));
    }

    SECTION("noteOn触发") {
        env.noteOn();
        REQUIRE(env.getCurrentStage() == EnvelopeStage::Attack);
    }

    SECTION("noteOff触发") {
        env.noteOn();
        env.noteOff();
        REQUIRE(env.getCurrentStage() == EnvelopeStage::Release);
    }
}

// =============================================================================
// LFO发生器测试
// =============================================================================
TEST_CASE("LFOGenerator: 波形输出", "[modulation][lfo]") {
    LFOGenerator lfo("TestLFO");
    lfo.prepareToPlay(44100.0, 256);

    SECTION("默认状态") {
        REQUIRE(lfo.getNumParameters() == 6);
    }

    SECTION("频率设置") {
        lfo.setFrequency(2.0f);
        REQUIRE(lfo.getParameter(0) == Catch::Approx(2.0f / 100.0f).margin(0.001f));
    }

    SECTION("处理输出") {
        lfo.setFrequency(1.0f);
        lfo.setDepth(1.0f);
        juce::AudioBuffer<float> buffer(2, 256);
        juce::MidiBuffer midi;
        lfo.processBlock(buffer, midi);

        float value = lfo.getCurrentValue();
        REQUIRE(value >= -1.0f);
        REQUIRE(value <= 1.0f);
    }
}