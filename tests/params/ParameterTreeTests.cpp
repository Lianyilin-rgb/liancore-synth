// =============================================================================
// LianCore - ParameterTree morphTo 渐变过渡单元测试 (Beta Week 8)
// 覆盖: morphTo 初始化、中途取消、边界值、缓动曲线、空目标、完成状态
// =============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../src/params/ParameterTree.h"
#include <JuceHeader.h>
#include <vector>
#include <string>
#include <cmath>

using namespace LianCore;

// =============================================================================
// 测试辅助：创建带注册参数的 ParameterTree
// =============================================================================
class TestAudioProcessor : public juce::AudioProcessor {
public:
    TestAudioProcessor()
        : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}

    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    const juce::String getName() const override { return "Test"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override { return true; }
};

static std::unique_ptr<LianCoreParameterTree> createTestTree() {
    auto processor = std::make_unique<TestAudioProcessor>();
    auto tree = std::make_unique<LianCoreParameterTree>(*processor);
    // 允许测试拥有 processor 的生命周期
    // 注意: processor 必须在 tree 之后销毁
    return tree;
}

// 注册常用参数
static void registerTestParams(LianCoreParameterTree& tree) {
    tree.registerParameter("filter_cutoff", "Cutoff", 0.5f, 0.0f, 1.0f, 0.01f, "", "Filter cutoff frequency", "Filter");
    tree.registerParameter("filter_resonance", "Resonance", 0.3f, 0.0f, 1.0f, 0.01f, "", "Filter resonance", "Filter");
    tree.registerParameter("osc_waveform", "Waveform", 0.5f, 0.0f, 1.0f, 0.01f, "", "Oscillator waveform", "Oscillator");
    tree.registerParameter("env_attack", "Attack", 0.1f, 0.0f, 1.0f, 0.01f, "s", "Envelope attack", "Envelope");
    tree.registerParameter("env_release", "Release", 0.5f, 0.0f, 1.0f, 0.01f, "s", "Envelope release", "Envelope");
    tree.registerParameter("reverb_size", "Room Size", 0.5f, 0.0f, 1.0f, 0.01f, "", "Reverb room size", "Reverb");
    tree.registerParameter("lfo_rate", "LFO Rate", 0.5f, 0.0f, 1.0f, 0.01f, "Hz", "LFO rate", "LFO");
    tree.registerParameter("noise_level", "Noise", 0.0f, 0.0f, 1.0f, 0.01f, "", "Noise level", "Noise");
}

// =============================================================================
// 1. morphTo 基本功能测试
// =============================================================================
TEST_CASE("ParameterTree: morphTo 基本功能", "[params][morph]") {
    // 注意: 由于 TestAudioProcessor 生命周期问题，这里简化测试
    // 实际测试需要确保 processor 在 tree 之后销毁

    SECTION("morphTo 空目标列表不启动渐变") {
        // 使用裸指针管理生命周期
        auto* processor = new TestAudioProcessor();
        LianCoreParameterTree tree(*processor);
        registerTestParams(tree);

        std::vector<LianCoreParameterTree::MorphTarget> empty;
        tree.morphTo(empty, 300);
        REQUIRE_FALSE(tree.isMorphing());

        delete processor;
    }

    SECTION("morphTo 启动渐变后 isMorphing 返回 true") {
        auto* processor = new TestAudioProcessor();
        LianCoreParameterTree tree(*processor);
        registerTestParams(tree);

        std::vector<LianCoreParameterTree::MorphTarget> targets = {
            {"filter_cutoff", 0.8f},
            {"filter_resonance", 0.7f}
        };
        tree.morphTo(targets, 300);
        REQUIRE(tree.isMorphing());

        delete processor;
    }

    SECTION("morphTo 取消渐变后 isMorphing 返回 false") {
        auto* processor = new TestAudioProcessor();
        LianCoreParameterTree tree(*processor);
        registerTestParams(tree);

        std::vector<LianCoreParameterTree::MorphTarget> targets = {
            {"filter_cutoff", 0.8f}
        };
        tree.morphTo(targets, 300);
        REQUIRE(tree.isMorphing());

        tree.cancelMorph();
        REQUIRE_FALSE(tree.isMorphing());

        delete processor;
    }

    SECTION("morphTo 新渐变覆盖旧渐变") {
        auto* processor = new TestAudioProcessor();
        LianCoreParameterTree tree(*processor);
        registerTestParams(tree);

        // 启动第一个渐变
        std::vector<LianCoreParameterTree::MorphTarget> targets1 = {
            {"filter_cutoff", 0.8f}
        };
        tree.morphTo(targets1, 500);
        REQUIRE(tree.isMorphing());

        // 启动第二个渐变 (应覆盖第一个)
        std::vector<LianCoreParameterTree::MorphTarget> targets2 = {
            {"filter_resonance", 0.9f}
        };
        tree.morphTo(targets2, 200);
        REQUIRE(tree.isMorphing());

        delete processor;
    }

    SECTION("morphTo 默认 duration 为 300ms") {
        auto* processor = new TestAudioProcessor();
        LianCoreParameterTree tree(*processor);
        registerTestParams(tree);

        std::vector<LianCoreParameterTree::MorphTarget> targets = {
            {"filter_cutoff", 0.8f}
        };
        tree.morphTo(targets); // 使用默认 300ms
        REQUIRE(tree.isMorphing());

        delete processor;
    }
}

// =============================================================================
// 2. morphTo 参数过渡测试
// =============================================================================
TEST_CASE("ParameterTree: morphTo 参数过渡", "[params][morph]") {
    SECTION("morphTo 单步更新后参数值变化") {
        auto* processor = new TestAudioProcessor();
        LianCoreParameterTree tree(*processor);
        registerTestParams(tree);

        float initialValue = tree.getParameterValue("filter_cutoff");
        REQUIRE(initialValue == Catch::Approx(0.5f));

        std::vector<LianCoreParameterTree::MorphTarget> targets = {
            {"filter_cutoff", 0.8f}
        };
        tree.morphTo(targets, 300);

        // 第一次更新
        tree.updateMorphStep();

        // 检查参数值是否有所变化 (应该从 0.5 向 0.8 移动)
        float currentValue = tree.getParameterValue("filter_cutoff");
        REQUIRE(currentValue > initialValue);
        REQUIRE(currentValue < 0.8f);

        delete processor;
    }

    SECTION("morphTo 多次更新后参数值更接近目标") {
        auto* processor = new TestAudioProcessor();
        LianCoreParameterTree tree(*processor);
        registerTestParams(tree);

        std::vector<LianCoreParameterTree::MorphTarget> targets = {
            {"filter_cutoff", 0.8f}
        };
        tree.morphTo(targets, 500);

        float val1 = tree.getParameterValue("filter_cutoff");

        // 多次更新
        for (int i = 0; i < 10; ++i) {
            tree.updateMorphStep();
        }

        float val2 = tree.getParameterValue("filter_cutoff");

        // 多次更新后值应更接近目标
        REQUIRE(val2 > val1);

        delete processor;
    }

    SECTION("morphTo 完成后参数值等于目标值") {
        auto* processor = new TestAudioProcessor();
        LianCoreParameterTree tree(*processor);
        registerTestParams(tree);

        std::vector<LianCoreParameterTree::MorphTarget> targets = {
            {"filter_cutoff", 0.8f}
        };
        // 很短的 duration，确保快速完成
        tree.morphTo(targets, 10);

        // 模拟足够多的更新步骤直到完成
        for (int i = 0; i < 10; ++i) {
            tree.updateMorphStep();
            if (!tree.isMorphing()) break;
        }

        // 渐变完成后应为 false
        REQUIRE_FALSE(tree.isMorphing());

        // 最终值应等于目标值
        float finalValue = tree.getParameterValue("filter_cutoff");
        REQUIRE(finalValue == Catch::Approx(0.8f));

        delete processor;
    }

    SECTION("morphTo 多个参数同时过渡") {
        auto* processor = new TestAudioProcessor();
        LianCoreParameterTree tree(*processor);
        registerTestParams(tree);

        std::vector<LianCoreParameterTree::MorphTarget> targets = {
            {"filter_cutoff", 0.8f},
            {"filter_resonance", 0.7f},
            {"reverb_size", 0.3f}
        };
        tree.morphTo(targets, 10);

        // 模拟完成
        for (int i = 0; i < 10; ++i) {
            tree.updateMorphStep();
            if (!tree.isMorphing()) break;
        }

        REQUIRE_FALSE(tree.isMorphing());
        REQUIRE(tree.getParameterValue("filter_cutoff") == Catch::Approx(0.8f));
        REQUIRE(tree.getParameterValue("filter_resonance") == Catch::Approx(0.7f));
        REQUIRE(tree.getParameterValue("reverb_size") == Catch::Approx(0.3f));

        delete processor;
    }
}

// =============================================================================
// 3. morphTo 边界值测试
// =============================================================================
TEST_CASE("ParameterTree: morphTo 边界值", "[params][morph]") {
    SECTION("目标值等于当前值") {
        auto* processor = new TestAudioProcessor();
        LianCoreParameterTree tree(*processor);
        registerTestParams(tree);

        float initialValue = tree.getParameterValue("filter_cutoff");

        std::vector<LianCoreParameterTree::MorphTarget> targets = {
            {"filter_cutoff", initialValue} // 目标值等于当前值
        };
        tree.morphTo(targets, 10);

        // 模拟完成
        for (int i = 0; i < 10; ++i) {
            tree.updateMorphStep();
            if (!tree.isMorphing()) break;
        }

        // 值应保持不变
        REQUIRE(tree.getParameterValue("filter_cutoff") == Catch::Approx(initialValue));

        delete processor;
    }

    SECTION("目标值为最小值 0.0") {
        auto* processor = new TestAudioProcessor();
        LianCoreParameterTree tree(*processor);
        registerTestParams(tree);

        std::vector<LianCoreParameterTree::MorphTarget> targets = {
            {"filter_cutoff", 0.0f}
        };
        tree.morphTo(targets, 10);

        for (int i = 0; i < 10; ++i) {
            tree.updateMorphStep();
            if (!tree.isMorphing()) break;
        }

        REQUIRE(tree.getParameterValue("filter_cutoff") == Catch::Approx(0.0f));

        delete processor;
    }

    SECTION("目标值为最大值 1.0") {
        auto* processor = new TestAudioProcessor();
        LianCoreParameterTree tree(*processor);
        registerTestParams(tree);

        std::vector<LianCoreParameterTree::MorphTarget> targets = {
            {"filter_cutoff", 1.0f}
        };
        tree.morphTo(targets, 10);

        for (int i = 0; i < 10; ++i) {
            tree.updateMorphStep();
            if (!tree.isMorphing()) break;
        }

        REQUIRE(tree.getParameterValue("filter_cutoff") == Catch::Approx(1.0f));

        delete processor;
    }

    SECTION("durationMs 为 0 时立即完成") {
        auto* processor = new TestAudioProcessor();
        LianCoreParameterTree tree(*processor);
        registerTestParams(tree);

        std::vector<LianCoreParameterTree::MorphTarget> targets = {
            {"filter_cutoff", 0.8f}
        };
        tree.morphTo(targets, 0); // 0 会被 clamp 到 10

        // 模拟完成
        for (int i = 0; i < 10; ++i) {
            tree.updateMorphStep();
            if (!tree.isMorphing()) break;
        }

        REQUIRE_FALSE(tree.isMorphing());

        delete processor;
    }

    SECTION("durationMs 为负数时 clamp 到最小值") {
        auto* processor = new TestAudioProcessor();
        LianCoreParameterTree tree(*processor);
        registerTestParams(tree);

        std::vector<LianCoreParameterTree::MorphTarget> targets = {
            {"filter_cutoff", 0.8f}
        };
        tree.morphTo(targets, -100); // 负数会被 clamp 到 10

        // 模拟完成
        for (int i = 0; i < 10; ++i) {
            tree.updateMorphStep();
            if (!tree.isMorphing()) break;
        }

        REQUIRE_FALSE(tree.isMorphing());

        delete processor;
    }
}

// =============================================================================
// 4. morphTo 缓动曲线测试
// =============================================================================
TEST_CASE("ParameterTree: morphTo 缓动曲线", "[params][morph]") {
    SECTION("cubic ease-in-out 起始阶段增长较慢") {
        auto* processor = new TestAudioProcessor();
        LianCoreParameterTree tree(*processor);
        registerTestParams(tree);

        std::vector<LianCoreParameterTree::MorphTarget> targets = {
            {"filter_cutoff", 1.0f} // 从 0.5 到 1.0
        };
        tree.morphTo(targets, 1000); // 长 duration 用于观察曲线

        // 第一次更新 (t ≈ 0.016)
        tree.updateMorphStep();
        float val1 = tree.getParameterValue("filter_cutoff");

        // 线性增长应为: 0.5 + (1.0-0.5) * 0.016 = 0.508
        // cubic ease-in-out 在 t<0.5 时: 4*t^3, 在 t=0.016 时: 4*(0.016)^3 = 4*0.000004 = 0.000016
        // 所以 cubic 增长很慢: 0.5 + (1.0-0.5) * 0.000016 ≈ 0.500008
        // 应该远小于线性值
        float linearExpected = 0.5f + (1.0f - 0.5f) * (16.0f / 1000.0f);
        REQUIRE(val1 < linearExpected); // cubic 开始比线性慢

        delete processor;
    }

    SECTION("cubic ease-in-out 中间阶段增长较快") {
        auto* processor = new TestAudioProcessor();
        LianCoreParameterTree tree(*processor);
        registerTestParams(tree);

        std::vector<LianCoreParameterTree::MorphTarget> targets = {
            {"filter_cutoff", 1.0f}
        };
        tree.morphTo(targets, 1000);

        // 跳过到中间阶段 (t ≈ 0.5, 约 31 步)
        for (int i = 0; i < 30; ++i) {
            tree.updateMorphStep();
        }

        float valBefore = tree.getParameterValue("filter_cutoff");

        // 再更新几步
        tree.updateMorphStep();
        tree.updateMorphStep();
        float valAfter = tree.getParameterValue("filter_cutoff");

        // 中间阶段增长应明显
        float delta = valAfter - valBefore;
        REQUIRE(delta > 0.0f);

        delete processor;
    }

    SECTION("cubic ease-in-out 结束阶段增长减缓") {
        auto* processor = new TestAudioProcessor();
        LianCoreParameterTree tree(*processor);
        registerTestParams(tree);

        std::vector<LianCoreParameterTree::MorphTarget> targets = {
            {"filter_cutoff", 1.0f}
        };
        tree.morphTo(targets, 1000);

        // 接近结束阶段 (t ≈ 0.9, 约 56 步)
        for (int i = 0; i < 55; ++i) {
            tree.updateMorphStep();
        }

        float valBefore = tree.getParameterValue("filter_cutoff");

        // 再更新几步
        tree.updateMorphStep();
        tree.updateMorphStep();
        float valAfter = tree.getParameterValue("filter_cutoff");

        // 结束阶段增长应减缓
        float delta = valAfter - valBefore;
        REQUIRE(delta > 0.0f); // 仍然有增长
        REQUIRE(delta < 0.05f); // 但增长很小

        // 最终完成
        while (tree.isMorphing()) {
            tree.updateMorphStep();
        }
        REQUIRE(tree.getParameterValue("filter_cutoff") == Catch::Approx(1.0f));

        delete processor;
    }
}

// =============================================================================
// 5. morphTo 与 cancelMorph 交互测试
// =============================================================================
TEST_CASE("ParameterTree: morphTo cancelMorph 交互", "[params][morph]") {
    SECTION("cancelMorph 后参数值保持在中间状态") {
        auto* processor = new TestAudioProcessor();
        LianCoreParameterTree tree(*processor);
        registerTestParams(tree);

        std::vector<LianCoreParameterTree::MorphTarget> targets = {
            {"filter_cutoff", 1.0f}
        };
        tree.morphTo(targets, 500);

        // 更新几步
        for (int i = 0; i < 5; ++i) {
            tree.updateMorphStep();
        }

        float midValue = tree.getParameterValue("filter_cutoff");

        // 取消渐变
        tree.cancelMorph();

        // 参数值应保持在取消时的值
        REQUIRE(tree.getParameterValue("filter_cutoff") == Catch::Approx(midValue));

        delete processor;
    }

    SECTION("cancelMorph 后可以重新启动新渐变") {
        auto* processor = new TestAudioProcessor();
        LianCoreParameterTree tree(*processor);
        registerTestParams(tree);

        // 启动并取消
        std::vector<LianCoreParameterTree::MorphTarget> targets1 = {
            {"filter_cutoff", 0.8f}
        };
        tree.morphTo(targets1, 300);
        tree.cancelMorph();
        REQUIRE_FALSE(tree.isMorphing());

        // 重新启动
        std::vector<LianCoreParameterTree::MorphTarget> targets2 = {
            {"filter_resonance", 0.9f}
        };
        tree.morphTo(targets2, 300);
        REQUIRE(tree.isMorphing());

        delete processor;
    }
}

// =============================================================================
// 6. morphTo 与参数注册交互测试
// =============================================================================
TEST_CASE("ParameterTree: morphTo 与参数注册", "[params][morph]") {
    SECTION("morphTo 未注册的参数ID不崩溃") {
        auto* processor = new TestAudioProcessor();
        LianCoreParameterTree tree(*processor);
        registerTestParams(tree);

        // 包含未注册的参数ID
        std::vector<LianCoreParameterTree::MorphTarget> targets = {
            {"filter_cutoff", 0.8f},
            {"non_existent_param", 0.5f} // 不存在的参数
        };
        tree.morphTo(targets, 10);

        // 模拟完成 (不应崩溃)
        for (int i = 0; i < 10; ++i) {
            tree.updateMorphStep();
            if (!tree.isMorphing()) break;
        }

        REQUIRE_FALSE(tree.isMorphing());
        // 已注册的参数应正确更新
        REQUIRE(tree.getParameterValue("filter_cutoff") == Catch::Approx(0.8f));

        delete processor;
    }

    SECTION("morphTo 所有参数ID未注册仍不崩溃") {
        auto* processor = new TestAudioProcessor();
        LianCoreParameterTree tree(*processor);
        // 不注册任何参数

        std::vector<LianCoreParameterTree::MorphTarget> targets = {
            {"non_existent_1", 0.8f},
            {"non_existent_2", 0.3f}
        };
        tree.morphTo(targets, 10);

        // 模拟完成 (不应崩溃)
        for (int i = 0; i < 10; ++i) {
            tree.updateMorphStep();
            if (!tree.isMorphing()) break;
        }

        REQUIRE_FALSE(tree.isMorphing());

        delete processor;
    }
}

// =============================================================================
// 7. morphTo 大量参数测试
// =============================================================================
TEST_CASE("ParameterTree: morphTo 大量参数", "[params][morph]") {
    SECTION("50个参数同时渐变") {
        auto* processor = new TestAudioProcessor();
        LianCoreParameterTree tree(*processor);

        // 注册 50 个参数
        for (int i = 0; i < 50; ++i) {
            tree.registerParameter(
                "param_" + juce::String(i),
                "Param " + juce::String(i),
                0.5f, 0.0f, 1.0f, 0.01f
            );
        }

        // 创建 50 个目标
        std::vector<LianCoreParameterTree::MorphTarget> targets;
        for (int i = 0; i < 50; ++i) {
            targets.push_back({"param_" + juce::String(i), 0.7f});
        }

        tree.morphTo(targets, 10);

        // 模拟完成
        for (int i = 0; i < 10; ++i) {
            tree.updateMorphStep();
            if (!tree.isMorphing()) break;
        }

        REQUIRE_FALSE(tree.isMorphing());

        // 验证所有参数达到目标值
        for (int i = 0; i < 50; ++i) {
            REQUIRE(tree.getParameterValue("param_" + juce::String(i)) == Catch::Approx(0.7f));
        }

        delete processor;
    }
}

// =============================================================================
// 8. morphTo 预设切换场景测试
// =============================================================================
TEST_CASE("ParameterTree: morphTo 预设切换场景", "[params][morph][preset]") {
    SECTION("模拟预设A到预设B的平滑过渡") {
        auto* processor = new TestAudioProcessor();
        LianCoreParameterTree tree(*processor);
        registerTestParams(tree);

        // 设置预设A的值
        tree.setParameterValue("filter_cutoff", 0.2f);
        tree.setParameterValue("filter_resonance", 0.1f);
        tree.setParameterValue("reverb_size", 0.8f);
        tree.setParameterValue("env_attack", 0.05f);

        // 预设B的目标值
        std::vector<LianCoreParameterTree::MorphTarget> presetB = {
            {"filter_cutoff", 0.9f},
            {"filter_resonance", 0.7f},
            {"reverb_size", 0.2f},
            {"env_attack", 0.3f}
        };

        tree.morphTo(presetB, 10);

        // 模拟完成
        for (int i = 0; i < 10; ++i) {
            tree.updateMorphStep();
            if (!tree.isMorphing()) break;
        }

        REQUIRE_FALSE(tree.isMorphing());
        REQUIRE(tree.getParameterValue("filter_cutoff") == Catch::Approx(0.9f));
        REQUIRE(tree.getParameterValue("filter_resonance") == Catch::Approx(0.7f));
        REQUIRE(tree.getParameterValue("reverb_size") == Catch::Approx(0.2f));
        REQUIRE(tree.getParameterValue("env_attack") == Catch::Approx(0.3f));

        delete processor;
    }
}