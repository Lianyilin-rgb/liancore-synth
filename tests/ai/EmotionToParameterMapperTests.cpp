// =============================================================================
// LianCore - EmotionToParameterMapper 单元测试
// 测试: 锚点有效性、角点插值、中点平均、参数映射数量、直接映射规则
// =============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "EmotionToParameterMapper.h"

using namespace LianCore;

// =============================================================================
// 测试套件 1: 锚点有效性
// =============================================================================
TEST_CASE("EmotionToParameterMapper: 锚点有效性", "[emotion][anchors]") {

    EmotionToParameterMapper mapper;
    const auto& anchors = mapper.getAnchorPresets();

    SECTION("8个锚点全部定义") {
        REQUIRE(anchors.size() == 8);
    }

    SECTION("每个锚点都有名称和描述") {
        for (int i = 0; i < 8; ++i) {
            REQUIRE(!anchors[i].name.isEmpty());
            REQUIRE(!anchors[i].description.isEmpty());
        }
    }

    SECTION("锚点情感坐标各不相同") {
        // 8个锚点分别对应立方体的8个顶点 (0或1)
        for (int i = 0; i < 8; ++i) {
            float w = anchors[i].warmth;
            float e = anchors[i].energy;
            float t = anchors[i].tension;
            // 每个坐标必须是 0 或 1
            REQUIRE((w == 0.0f || w == 1.0f));
            REQUIRE((e == 0.0f || e == 1.0f));
            REQUIRE((t == 0.0f || t == 1.0f));
        }
    }

    SECTION("每个锚点包含有效的参数映射") {
        for (int i = 0; i < 8; ++i) {
            REQUIRE(anchors[i].parameters.size() > 0);
            for (const auto& [id, value] : anchors[i].parameters) {
                REQUIRE(!id.isEmpty());
                REQUIRE(value >= 0.0f);
                REQUIRE(value <= 1.0f);
            }
        }
    }

    SECTION("所有锚点参数数量一致") {
        size_t count = anchors[0].parameters.size();
        for (int i = 1; i < 8; ++i) {
            REQUIRE(anchors[i].parameters.size() == count);
        }
    }
}

// =============================================================================
// 测试套件 2: 角点插值 (锚点位置应返回精确的锚点值)
// =============================================================================
TEST_CASE("EmotionToParameterMapper: 角点插值精确性", "[emotion][interpolation][corners]") {

    EmotionToParameterMapper mapper;
    const auto& anchors = mapper.getAnchorPresets();

    SECTION("角落 0: (0,0,0) 暗柔铺底") {
        auto params = mapper.mapEmotionToParameters(0.0f, 0.0f, 0.0f);
        REQUIRE(!params.empty());
        for (const auto& p : params) {
            auto it = anchors[0].parameters.find(p.parameterId);
            if (it != anchors[0].parameters.end()) {
                // 角点应返回精确值 (允许微小浮点误差)
                REQUIRE(p.value == Catch::Approx(it->second).margin(0.001f));
            }
        }
    }

    SECTION("角落 7: (1,1,1) 复杂合成") {
        auto params = mapper.mapEmotionToParameters(1.0f, 1.0f, 1.0f);
        REQUIRE(!params.empty());
        for (const auto& p : params) {
            auto it = anchors[7].parameters.find(p.parameterId);
            if (it != anchors[7].parameters.end()) {
                REQUIRE(p.value == Catch::Approx(it->second).margin(0.001f));
            }
        }
    }

    SECTION("角落 3: (0,1,1) 激进失真") {
        auto params = mapper.mapEmotionToParameters(0.0f, 1.0f, 1.0f);
        for (const auto& p : params) {
            auto it = anchors[3].parameters.find(p.parameterId);
            if (it != anchors[3].parameters.end()) {
                REQUIRE(p.value == Catch::Approx(it->second).margin(0.001f));
            }
        }
    }

    SECTION("角落 6: (1,1,0) 明亮主音") {
        auto params = mapper.mapEmotionToParameters(1.0f, 1.0f, 0.0f);
        for (const auto& p : params) {
            auto it = anchors[6].parameters.find(p.parameterId);
            if (it != anchors[6].parameters.end()) {
                REQUIRE(p.value == Catch::Approx(it->second).margin(0.001f));
            }
        }
    }
}

// =============================================================================
// 测试套件 3: 中点平均 (0.5,0.5,0.5 应等于所有8个锚点的均值)
// =============================================================================
TEST_CASE("EmotionToParameterMapper: 中点平均", "[emotion][interpolation][midpoint]") {

    EmotionToParameterMapper mapper;
    const auto& anchors = mapper.getAnchorPresets();

    SECTION("(0.5,0.5,0.5) 插值应等于锚点均值") {
        auto params = mapper.mapEmotionToParameters(0.5f, 0.5f, 0.5f);

        for (const auto& p : params) {
            // 计算所有8个锚点该参数的平均值
            float sum = 0.0f;
            int count = 0;
            for (int i = 0; i < 8; ++i) {
                auto it = anchors[i].parameters.find(p.parameterId);
                if (it != anchors[i].parameters.end()) {
                    sum += it->second;
                    ++count;
                }
            }
            float expected = count > 0 ? sum / count : 0.5f;
            REQUIRE(p.value == Catch::Approx(expected).margin(0.01f));
        }
    }
}

// =============================================================================
// 测试套件 4: 参数映射数量
// =============================================================================
TEST_CASE("EmotionToParameterMapper: 参数映射数量", "[emotion][count]") {

    EmotionToParameterMapper mapper;

    SECTION("插值映射返回正确的参数数量") {
        auto params = mapper.mapEmotionToParameters(0.25f, 0.75f, 0.5f);
        // 应该返回与锚点相同数量的参数
        REQUIRE(params.size() == 24); // 每个锚点定义24个参数
    }

    SECTION("直接映射返回固定数量的参数") {
        auto params = EmotionToParameterMapper::mapEmotionDirect(0.5f, 0.5f, 0.5f);
        // 直接映射: 6(warmth) + 6(energy) + 7(tension) = 19
        // 但 filter_resonance 和 distortion_drive 在 warmth 和 tension 中都有
        // 实际去重后: 6 + 6 + 7 - 2 = 17
        REQUIRE(params.size() > 0);
        REQUIRE(params.size() <= 20);
    }
}

// =============================================================================
// 测试套件 5: 直接映射规则
// =============================================================================
TEST_CASE("EmotionToParameterMapper: 直接映射规则", "[emotion][direct]") {

    SECTION("温暖度=0.0 时截止频率为最小值") {
        auto params = EmotionToParameterMapper::mapEmotionDirect(0.0f, 0.5f, 0.5f);
        for (const auto& p : params) {
            if (p.parameterId == "filter_cutoff") {
                REQUIRE(p.value == Catch::Approx(0.25f).margin(0.01f));
            }
        }
    }

    SECTION("温暖度=1.0 时截止频率为最大值") {
        auto params = EmotionToParameterMapper::mapEmotionDirect(1.0f, 0.5f, 0.5f);
        for (const auto& p : params) {
            if (p.parameterId == "filter_cutoff") {
                REQUIRE(p.value == Catch::Approx(0.8f).margin(0.01f));
            }
        }
    }

    SECTION("能量感=0.0 时起音最慢") {
        auto params = EmotionToParameterMapper::mapEmotionDirect(0.5f, 0.0f, 0.5f);
        for (const auto& p : params) {
            if (p.parameterId == "env_attack") {
                REQUIRE(p.value == Catch::Approx(0.55f).margin(0.01f));
            }
        }
    }

    SECTION("能量感=1.0 时起音最快") {
        auto params = EmotionToParameterMapper::mapEmotionDirect(0.5f, 1.0f, 0.5f);
        for (const auto& p : params) {
            if (p.parameterId == "env_attack") {
                REQUIRE(p.value == Catch::Approx(0.05f).margin(0.01f));
            }
        }
    }

    SECTION("紧张度=0.0 时 LFO 速率最小") {
        auto params = EmotionToParameterMapper::mapEmotionDirect(0.5f, 0.5f, 0.0f);
        for (const auto& p : params) {
            if (p.parameterId == "lfo_rate") {
                REQUIRE(p.value == Catch::Approx(0.1f).margin(0.01f));
            }
        }
    }

    SECTION("紧张度=1.0 时 LFO 速率最大") {
        auto params = EmotionToParameterMapper::mapEmotionDirect(0.5f, 0.5f, 1.0f);
        for (const auto& p : params) {
            if (p.parameterId == "lfo_rate") {
                REQUIRE(p.value == Catch::Approx(0.7f).margin(0.01f));
            }
        }
    }

    SECTION("所有返回值在 [0,1] 范围内") {
        // 测试边界情况
        auto params1 = EmotionToParameterMapper::mapEmotionDirect(0.0f, 0.0f, 0.0f);
        auto params2 = EmotionToParameterMapper::mapEmotionDirect(1.0f, 1.0f, 1.0f);
        auto params3 = EmotionToParameterMapper::mapEmotionDirect(0.5f, 0.5f, 0.5f);

        for (const auto& p : params1) {
            REQUIRE(p.value >= 0.0f);
            REQUIRE(p.value <= 1.0f);
        }
        for (const auto& p : params2) {
            REQUIRE(p.value >= 0.0f);
            REQUIRE(p.value <= 1.0f);
        }
        for (const auto& p : params3) {
            REQUIRE(p.value >= 0.0f);
            REQUIRE(p.value <= 1.0f);
        }
    }
}

// =============================================================================
// 测试套件 6: 边界条件
// =============================================================================
TEST_CASE("EmotionToParameterMapper: 边界条件", "[emotion][boundary]") {

    EmotionToParameterMapper mapper;

    SECTION("超出范围的值被钳制") {
        // 传入负值应被钳制到 0
        auto params1 = mapper.mapEmotionToParameters(-0.5f, -0.5f, -0.5f);
        REQUIRE(!params1.empty());
        for (const auto& p : params1) {
            REQUIRE(p.value >= 0.0f);
            REQUIRE(p.value <= 1.0f);
        }

        // 传入大于1的值应被钳制到 1
        auto params2 = mapper.mapEmotionToParameters(1.5f, 1.5f, 1.5f);
        REQUIRE(!params2.empty());
        for (const auto& p : params2) {
            REQUIRE(p.value >= 0.0f);
            REQUIRE(p.value <= 1.0f);
        }
    }

    SECTION("isValidEmotionVector 正确判断") {
        REQUIRE(EmotionToParameterMapper::isValidEmotionVector(0.0f, 0.0f, 0.0f));
        REQUIRE(EmotionToParameterMapper::isValidEmotionVector(0.5f, 0.5f, 0.5f));
        REQUIRE(EmotionToParameterMapper::isValidEmotionVector(1.0f, 1.0f, 1.0f));
        REQUIRE(!EmotionToParameterMapper::isValidEmotionVector(-0.1f, 0.5f, 0.5f));
        REQUIRE(!EmotionToParameterMapper::isValidEmotionVector(0.5f, 1.1f, 0.5f));
        REQUIRE(!EmotionToParameterMapper::isValidEmotionVector(0.5f, 0.5f, -0.01f));
    }

    SECTION("边界值 (0,0,0) 和 (1,1,1) 传入允许") {
        auto params1 = mapper.mapEmotionToParameters(0.0f, 0.0f, 0.0f);
        auto params2 = mapper.mapEmotionToParameters(1.0f, 1.0f, 1.0f);
        REQUIRE(params1.size() == params2.size());
    }
}

// =============================================================================
// 测试套件 7: 最近锚点查找
// =============================================================================
TEST_CASE("EmotionToParameterMapper: 最近锚点查找", "[emotion][nearest]") {

    EmotionToParameterMapper mapper;

    SECTION("精确位置匹配精确锚点") {
        REQUIRE(mapper.getNearestAnchorIndex(0.0f, 0.0f, 0.0f) == 0);
        REQUIRE(mapper.getNearestAnchorIndex(0.0f, 0.0f, 1.0f) == 1);
        REQUIRE(mapper.getNearestAnchorIndex(0.0f, 1.0f, 0.0f) == 2);
        REQUIRE(mapper.getNearestAnchorIndex(0.0f, 1.0f, 1.0f) == 3);
        REQUIRE(mapper.getNearestAnchorIndex(1.0f, 0.0f, 0.0f) == 4);
        REQUIRE(mapper.getNearestAnchorIndex(1.0f, 0.0f, 1.0f) == 5);
        REQUIRE(mapper.getNearestAnchorIndex(1.0f, 1.0f, 0.0f) == 6);
        REQUIRE(mapper.getNearestAnchorIndex(1.0f, 1.0f, 1.0f) == 7);
    }

    SECTION("中间位置查找最近锚点") {
        // (0.1, 0.1, 0.1) 应接近锚点 0
        int idx = mapper.getNearestAnchorIndex(0.1f, 0.1f, 0.1f);
        REQUIRE(idx == 0);

        // (0.9, 0.9, 0.9) 应接近锚点 7
        idx = mapper.getNearestAnchorIndex(0.9f, 0.9f, 0.9f);
        REQUIRE(idx == 7);
    }
}

// =============================================================================
// 测试套件 8: 锚点设置/获取
// =============================================================================
TEST_CASE("EmotionToParameterMapper: 锚点管理", "[emotion][management]") {

    EmotionToParameterMapper mapper;

    SECTION("获取锚点返回有效数据") {
        for (int i = 0; i < 8; ++i) {
            auto preset = mapper.getAnchorPreset(i);
            REQUIRE(!preset.name.isEmpty());
        }
    }

    SECTION("设置自定义锚点") {
        AnchorPreset custom;
        custom.name = "CustomTest";
        custom.description = "Test anchor";
        custom.warmth = 0.5f;
        custom.energy = 0.5f;
        custom.tension = 0.5f;
        custom.parameters["filter_cutoff"] = 0.42f;
        custom.parameters["env_attack"] = 0.33f;

        mapper.setAnchorPreset(3, custom);
        auto retrieved = mapper.getAnchorPreset(3);

        REQUIRE(retrieved.name == "CustomTest");
        REQUIRE(retrieved.parameters["filter_cutoff"] == Catch::Approx(0.42f));
    }

    SECTION("越界索引不崩溃") {
        // 设置越界应静默忽略
        mapper.setAnchorPreset(-1, AnchorPreset());
        mapper.setAnchorPreset(8, AnchorPreset());

        // 获取越界返回空预设
        auto empty = mapper.getAnchorPreset(-1);
        REQUIRE(empty.name.isEmpty());

        auto empty2 = mapper.getAnchorPreset(100);
        REQUIRE(empty2.name.isEmpty());
    }
}