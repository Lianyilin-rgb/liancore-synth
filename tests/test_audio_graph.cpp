// =============================================================================
// LianCore - 音频图引擎测试套件
// 验收标准: AE-003, AE-004, AE-005, AE-006
// =============================================================================
#include <catch2/catch_test_macros.hpp>
#include "AudioGraphEngine.h"
#include "AudioUtils.h"

using namespace LianCore;

// =============================================================================
// AE-003: 拓扑排序测试
// =============================================================================
TEST_CASE("AudioGraphEngine: 拓扑排序", "[audio_graph][ae-003]") {
    AudioGraphEngine graph;

    SECTION("简单链: OSC→Filter→Output") {
        auto osc = graph.addNode(NodeType::WavetableOscillator, "OSC");
        auto filter = graph.addNode(NodeType::Filter, "Filter");
        auto output = graph.addNode(NodeType::AudioOutput, "Output");

        graph.connect(osc, 0, filter, 0);
        graph.connect(filter, 0, output, 0);

        // 节点数应为3
        REQUIRE(graph.getNodeCount() == 3);
    }

    SECTION("复杂图: 30+节点") {
        auto prevNode = graph.addNode(NodeType::WavetableOscillator, "OSC_1");
        for (int i = 2; i <= 30; ++i) {
            auto filter = graph.addNode(NodeType::Filter,
                "Filter_" + juce::String(i));
            if (prevNode.isNotEmpty()) {
                graph.connect(prevNode, 0, filter, 0);
            }
            prevNode = filter;
        }
        auto output = graph.addNode(NodeType::AudioOutput, "Output");
        graph.connect(prevNode, 0, output, 0);

        REQUIRE(graph.getNodeCount() >= 31);
        REQUIRE(graph.getConnections().size() >= 30);
    }

    SECTION("节点移除") {
        auto osc = graph.addNode(NodeType::WavetableOscillator, "OSC");
        auto filter = graph.addNode(NodeType::Filter, "Filter");
        graph.connect(osc, 0, filter, 0);

        graph.removeNode(osc);
        REQUIRE(graph.getNodeCount() == 1);
        // 连接也应被移除
        REQUIRE(graph.getConnections().empty());
    }
}

// =============================================================================
// AE-004: 实时处理性能测试
// =============================================================================
TEST_CASE("AudioGraphEngine: 处理性能", "[audio_graph][ae-004]") {
    AudioGraphEngine graph;

    auto osc = graph.addNode(NodeType::WavetableOscillator, "OSC");
    auto filter = graph.addNode(NodeType::Filter, "Filter");
    auto output = graph.addNode(NodeType::AudioOutput, "Output");

    graph.connect(osc, 0, filter, 0);
    graph.connect(filter, 0, output, 0);

    graph.prepareToPlay(48000.0, 256);

    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;

    SECTION("1000次处理调用") {
        double totalTime = 0.0;
        for (int i = 0; i < 1000; ++i) {
            buffer.clear();
            auto start = juce::Time::getMillisecondCounterHiRes();
            graph.processBlock(buffer, midi);
            totalTime += juce::Time::getMillisecondCounterHiRes() - start;
        }
        double avgTime = totalTime / 1000.0;
        REQUIRE(avgTime < 2.0); // 平均处理时间 < 2ms
    }
}

// =============================================================================
// AE-005: 序列化/反序列化测试
// =============================================================================
TEST_CASE("AudioGraphEngine: 序列化", "[audio_graph][ae-005]") {
    AudioGraphEngine graph;

    auto osc = graph.addNode(NodeType::WavetableOscillator, "OSC");
    auto filter = graph.addNode(NodeType::Filter, "Filter");
    auto output = graph.addNode(NodeType::AudioOutput, "Output");

    graph.connect(osc, 0, filter, 0);
    graph.connect(filter, 0, output, 0);

    auto json = graph.toJson();
    REQUIRE(json.isObject());

    // 验证反序列化
    AudioGraphEngine graph2;
    graph2.fromJson(json);
    REQUIRE(graph2.getNodeCount() == 3);
    REQUIRE(graph2.getConnections().size() == 2);
}

// =============================================================================
// 内存追踪测试
// =============================================================================
TEST_CASE("AudioGraphEngine: 内存追踪", "[audio_graph][memory]") {
    AudioGraphEngine graph;

    auto osc = graph.addNode(NodeType::WavetableOscillator, "OSC");
    auto filter = graph.addNode(NodeType::Filter, "Filter");
    auto output = graph.addNode(NodeType::AudioOutput, "Output");

    size_t mem = graph.getTotalMemoryUsage();
    REQUIRE(mem > 0); // 应有内存占用
}

// =============================================================================
// 音频工具测试
// =============================================================================
TEST_CASE("AudioUtils: 频率转换", "[audio_utils]") {
    using namespace AudioUtils;

    SECTION("MIDI音符 → 频率") {
        REQUIRE(midiNoteToFrequency(69.0) == Approx(440.0).margin(0.1));
        REQUIRE(midiNoteToFrequency(81.0) == Approx(880.0).margin(0.5));
        REQUIRE(midiNoteToFrequency(57.0) == Approx(220.0).margin(0.1));
    }

    SECTION("半音 → 频率倍率") {
        REQUIRE(semitonesToRatio(12.0) == Approx(2.0).margin(0.001));
        REQUIRE(semitonesToRatio(0.0) == Approx(1.0).margin(0.001));
    }

    SECTION("值域裁剪") {
        REQUIRE(clamp(0.5f, 0.0f, 1.0f) == 0.5f);
        REQUIRE(clamp(-0.5f, 0.0f, 1.0f) == 0.0f);
        REQUIRE(clamp(1.5f, 0.0f, 1.0f) == 1.0f);
    }
}

TEST_CASE("AudioUtils: SIMD操作", "[audio_utils][simd]") {
    using namespace AudioUtils;

    SECTION("缓冲区清零") {
        float buffer[256];
        std::fill_n(buffer, 256, 1.0f);
        clearBufferSIMD(buffer, 256);
        for (int i = 0; i < 256; ++i) {
            REQUIRE(buffer[i] == 0.0f);
        }
    }

    SECTION("缓冲区乘法") {
        float buffer[256];
        std::fill_n(buffer, 256, 0.5f);
        multiplyBufferSIMD(buffer, 2.0f, 256);
        for (int i = 0; i < 256; ++i) {
            REQUIRE(buffer[i] == Approx(1.0f).margin(0.001f));
        }
    }
}