// =============================================================================
// LianCore - 音频节点测试套件
// 验收标准: AE-001, AE-002
// =============================================================================
#include <catch2/catch_test_macros.hpp>
#include "AudioNode.h"
#include "AudioGraphEngine.h"
#include "WavetableOscillator.h"
#include "FilterProcessor.h"
#include "EnvelopeGenerator.h"
#include "LFOGenerator.h"

using namespace LianCore;

// =============================================================================
// AE-001: 节点创建/删除测试
// =============================================================================
TEST_CASE("AudioNode: 创建和删除节点", "[audio_node][ae-001]") {
    SECTION("创建WavetableOscillator节点") {
        WavetableOscillator osc("TestOSC");
        REQUIRE(osc.getNodeType() == NodeType::WavetableOscillator);
        REQUIRE(osc.getName() == "TestOSC");
        REQUIRE(osc.getNumOutputPorts() == 1);
        REQUIRE(osc.getNumParameters() == 10);
    }

    SECTION("创建FilterProcessor节点") {
        FilterProcessor filter("TestFilter");
        REQUIRE(filter.getNodeType() == NodeType::Filter);
        REQUIRE(filter.getNumInputPorts() == 1);
        REQUIRE(filter.getNumOutputPorts() == 1);
    }

    SECTION("创建EnvelopeGenerator节点") {
        EnvelopeGenerator env("TestEnv");
        REQUIRE(env.getNodeType() == NodeType::Envelope);
        REQUIRE(env.getCurrentStage() == EnvelopeStage::Idle);
    }

    SECTION("创建LFOGenerator节点") {
        LFOGenerator lfo("TestLFO");
        REQUIRE(lfo.getNodeType() == NodeType::LFO);
        REQUIRE(lfo.getNumParameters() == 6);
    }
}

// =============================================================================
// AE-002: 节点连接/断开测试
// =============================================================================
TEST_CASE("AudioGraphEngine: 连接管理", "[audio_graph][ae-002]") {
    AudioGraphEngine graph;

    auto oscId = graph.addNode(NodeType::WavetableOscillator, "OSC");
    auto filterId = graph.addNode(NodeType::Filter, "Filter");
    auto outputId = graph.addNode(NodeType::AudioOutput, "Output");

    SECTION("创建有效连接") {
        auto connId = graph.connect(oscId, 0, filterId, 0);
        REQUIRE(!connId.isEmpty());
        REQUIRE(graph.getConnections().size() == 1);
    }

    SECTION("创建完整信号链") {
        graph.connect(oscId, 0, filterId, 0);
        graph.connect(filterId, 0, outputId, 0);
        REQUIRE(graph.getConnections().size() == 2);
    }

    SECTION("断开连接") {
        auto connId = graph.connect(oscId, 0, filterId, 0);
        graph.disconnect(connId);
        REQUIRE(graph.getConnections().empty());
    }

    SECTION("断开所有连接") {
        graph.connect(oscId, 0, filterId, 0);
        graph.connect(filterId, 0, outputId, 0);
        graph.disconnectAll();
        REQUIRE(graph.getConnections().empty());
    }
}

// =============================================================================
// 节点类型测试
// =============================================================================
TEST_CASE("NodeType: 类型名称映射", "[node_type]") {
    REQUIRE(nodeTypeToString(NodeType::WavetableOscillator) == "WavetableOscillator");
    REQUIRE(nodeTypeToString(NodeType::Filter) == "Filter");
    REQUIRE(nodeTypeToString(NodeType::LFO) == "LFO");
    REQUIRE(nodeTypeToString(NodeType::AudioOutput) == "AudioOutput");

    REQUIRE(stringToNodeType("WavetableOscillator") == NodeType::WavetableOscillator);
    REQUIRE(stringToNodeType("Filter") == NodeType::Filter);
}

// =============================================================================
// 端口管理测试
// =============================================================================
TEST_CASE("AudioNode: 端口管理", "[audio_node][ports]") {
    WavetableOscillator osc("TestOSC");

    SECTION("输出端口") {
        REQUIRE(osc.getNumOutputPorts() == 1);
        auto& desc = osc.getPortDescriptor(0, false);
        REQUIRE(desc.isAudio == true);
    }

    FilterProcessor filter("TestFilter");
    SECTION("输入/输出端口") {
        REQUIRE(filter.getNumInputPorts() == 1);
        REQUIRE(filter.getNumOutputPorts() == 1);
    }
}

// =============================================================================
// 序列化测试
// =============================================================================
TEST_CASE("AudioNode: 序列化", "[audio_node][serialization]") {
    WavetableOscillator osc("TestOSC");
    osc.setFrequency(880.0f);
    osc.setVolume(0.5f);

    auto json = osc.toJson();
    REQUIRE(json.isObject());

    // 验证反序列化
    WavetableOscillator osc2("TestOSC2");
    osc2.fromJson(json);
    REQUIRE(osc2.getName() == "TestOSC");
}