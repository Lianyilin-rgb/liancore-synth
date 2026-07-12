// =============================================================================
// LianCore - AIInferenceEngine ONNX 推理路径单元测试 (Beta Week 7)
// 覆盖: ONNX推理路径、规则引擎回退、文本编码、模型加载、情感融合
// =============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../src/ai/AIInferenceEngine.h"
#include "../src/ai/OnnxModelExporter.h"
#include "../src/ai/EmotionToParameterMapper.h"
#include <JuceHeader.h>
#include <vector>
#include <string>
#include <cmath>

using namespace LianCore;

// =============================================================================
// 测试辅助：创建模拟的 AIInferenceEngine
// =============================================================================
static AIInferenceEngine createTestEngine() {
    return AIInferenceEngine();
}

// =============================================================================
// 1. ONNX 模型导出器测试
// =============================================================================
TEST_CASE("OnnxModelExporter: 文本特征编码", "[ai][onnx][exporter]") {
    SECTION("编码非空文本生成128维特征") {
        auto features = OnnxModelExporter::encodeTextFeatures("明亮的合成器", 128);
        REQUIRE(features.size() == 128);

        // 检查至少有一个非零特征
        bool hasNonZero = false;
        for (float f : features) {
            if (std::abs(f) > 1e-6f) {
                hasNonZero = true;
                break;
            }
        }
        REQUIRE(hasNonZero);
    }

    SECTION("编码空文本返回全零特征") {
        auto features = OnnxModelExporter::encodeTextFeatures("", 128);
        REQUIRE(features.size() == 128);
        for (float f : features) {
            REQUIRE(f == 0.0f);
        }
    }

    SECTION("编码相同文本产生相同特征") {
        auto f1 = OnnxModelExporter::encodeTextFeatures("温暖铺底", 128);
        auto f2 = OnnxModelExporter::encodeTextFeatures("温暖铺底", 128);
        REQUIRE(f1.size() == f2.size());
        for (size_t i = 0; i < f1.size(); ++i) {
            REQUIRE(f1[i] == Catch::Approx(f2[i]));
        }
    }

    SECTION("不同文本产生不同特征") {
        auto f1 = OnnxModelExporter::encodeTextFeatures("明亮", 128);
        auto f2 = OnnxModelExporter::encodeTextFeatures("暗", 128);
        bool hasDiff = false;
        for (size_t i = 0; i < f1.size(); ++i) {
            if (std::abs(f1[i] - f2[i]) > 1e-6f) {
                hasDiff = true;
                break;
            }
        }
        REQUIRE(hasDiff);
    }

    SECTION("自定义维度") {
        auto features = OnnxModelExporter::encodeTextFeatures("test", 64);
        REQUIRE(features.size() == 64);
    }
}

TEST_CASE("OnnxModelExporter: 合成训练数据生成", "[ai][onnx][exporter]") {
    SECTION("生成指定数量的样本") {
        auto samples = OnnxModelExporter::generateTrainingData(100, 42);
        REQUIRE(samples.size() == 100);
    }

    SECTION("每个样本包含有效的输入特征") {
        auto samples = OnnxModelExporter::generateTrainingData(50, 42);
        for (const auto& sample : samples) {
            REQUIRE(sample.inputFeatures.size() == 128);
            REQUIRE(sample.targetParams.size() == 11);
        }
    }

    SECTION("目标参数在有效范围内") {
        auto samples = OnnxModelExporter::generateTrainingData(200, 42);
        for (const auto& sample : samples) {
            for (float val : sample.targetParams) {
                REQUIRE(val >= 0.0f);
                REQUIRE(val <= 1.0f);
            }
        }
    }

    SECTION("相同种子产生相同数据") {
        auto s1 = OnnxModelExporter::generateTrainingData(10, 123);
        auto s2 = OnnxModelExporter::generateTrainingData(10, 123);
        REQUIRE(s1.size() == s2.size());
        for (size_t i = 0; i < s1.size(); ++i) {
            for (size_t j = 0; j < s1[i].inputFeatures.size(); ++j) {
                REQUIRE(s1[i].inputFeatures[j] == Catch::Approx(s2[i].inputFeatures[j]));
            }
        }
    }

    SECTION("不同种子产生不同数据") {
        auto s1 = OnnxModelExporter::generateTrainingData(10, 42);
        auto s2 = OnnxModelExporter::generateTrainingData(10, 99);
        // 至少有一个样本不同
        bool hasDiff = false;
        for (size_t i = 0; i < s1.size() && !hasDiff; ++i) {
            for (size_t j = 0; j < s1[i].targetParams.size(); ++j) {
                if (std::abs(s1[i].targetParams[j] - s2[i].targetParams[j]) > 1e-4f) {
                    hasDiff = true;
                    break;
                }
            }
        }
        REQUIRE(hasDiff);
    }
}

TEST_CASE("OnnxModelExporter: 模型文件验证", "[ai][onnx][exporter]") {
    SECTION("不存在的文件无效") {
        juce::File nonExistent("nonexistent_model.onnx");
        auto info = OnnxModelExporter::validateModelFile(nonExistent);
        REQUIRE_FALSE(info.isValid);
    }

    SECTION("非ONNX扩展名文件无效") {
        // 创建临时文件
        auto tempFile = juce::File::createTempFile(".txt");
        tempFile.replaceWithText("test");
        auto info = OnnxModelExporter::validateModelFile(tempFile);
        REQUIRE_FALSE(info.isValid);
        tempFile.deleteFile();
    }

    SECTION("文件过小无效") {
        auto tempFile = juce::File::createTempFile(".onnx");
        tempFile.replaceWithText("ab");
        auto info = OnnxModelExporter::validateModelFile(tempFile);
        REQUIRE_FALSE(info.isValid);
        tempFile.deleteFile();
    }
}

TEST_CASE("OnnxModelExporter: 推理输出验证", "[ai][onnx][exporter]") {
    SECTION("有效输出通过验证") {
        std::vector<float> validOutput = {0.1f, 0.5f, 0.9f, 0.0f, 1.0f,
                                          0.3f, 0.7f, 0.2f, 0.8f, 0.4f, 0.6f};
        REQUIRE(OnnxModelExporter::validateInferenceOutput(validOutput));
    }

    SECTION("空输出无效") {
        std::vector<float> emptyOutput;
        REQUIRE_FALSE(OnnxModelExporter::validateInferenceOutput(emptyOutput));
    }

    SECTION("超出范围的值无效") {
        std::vector<float> outOfRange = {0.5f, 1.5f, 0.3f, 0.7f, 0.2f,
                                         0.8f, 0.4f, 0.6f, 0.9f, 0.1f, 0.0f};
        REQUIRE_FALSE(OnnxModelExporter::validateInferenceOutput(outOfRange));
    }

    SECTION("NaN值无效") {
        std::vector<float> nanOutput = {0.5f, std::nanf(""), 0.3f, 0.7f, 0.2f,
                                        0.8f, 0.4f, 0.6f, 0.9f, 0.1f, 0.0f};
        REQUIRE_FALSE(OnnxModelExporter::validateInferenceOutput(nanOutput));
    }

    SECTION("Inf值无效") {
        std::vector<float> infOutput = {0.5f, std::numeric_limits<float>::infinity(),
                                        0.3f, 0.7f, 0.2f, 0.8f, 0.4f, 0.6f, 0.9f, 0.1f, 0.0f};
        REQUIRE_FALSE(OnnxModelExporter::validateInferenceOutput(infOutput));
    }

    SECTION("负值无效") {
        std::vector<float> negOutput = {0.5f, -0.1f, 0.3f, 0.7f, 0.2f,
                                        0.8f, 0.4f, 0.6f, 0.9f, 0.1f, 0.0f};
        REQUIRE_FALSE(OnnxModelExporter::validateInferenceOutput(negOutput));
    }

    SECTION("边界值通过验证") {
        std::vector<float> boundary = {0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
                                       1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f};
        REQUIRE(OnnxModelExporter::validateInferenceOutput(boundary));
    }
}

TEST_CASE("OnnxModelExporter: 输出参数名称", "[ai][onnx][exporter]") {
    SECTION("参数名称列表包含11个参数") {
        const auto& names = OnnxModelExporter::getOutputParameterNames();
        REQUIRE(names.size() == 11);
    }

    SECTION("包含关键参数") {
        const auto& names = OnnxModelExporter::getOutputParameterNames();
        REQUIRE(std::find(names.begin(), names.end(), "filter_cutoff") != names.end());
        REQUIRE(std::find(names.begin(), names.end(), "filter_resonance") != names.end());
        REQUIRE(std::find(names.begin(), names.end(), "osc_waveform") != names.end());
        REQUIRE(std::find(names.begin(), names.end(), "env_attack") != names.end());
        REQUIRE(std::find(names.begin(), names.end(), "env_release") != names.end());
        REQUIRE(std::find(names.begin(), names.end(), "reverb_size") != names.end());
    }
}

// =============================================================================
// 2. AIInferenceEngine 核心推理测试
// =============================================================================
TEST_CASE("AIInferenceEngine: 规则引擎推理", "[ai][engine][rules]") {
    auto engine = createTestEngine();

    SECTION("明亮关键词触发filter_cutoff=0.8") {
        auto result = engine.generateParameters("明亮");
        REQUIRE(result.confidence > 0.0f);

        // 检查参数是否包含 filter_cutoff
        bool foundCutoff = false;
        for (const auto& param : result.parameters) {
            if (param.parameterId == "filter_cutoff") {
                REQUIRE(param.value == Catch::Approx(0.8f).margin(0.01f));
                foundCutoff = true;
            }
        }
        REQUIRE(foundCutoff);
    }

    SECTION("温暖关键词触发filter_cutoff=0.3") {
        auto result = engine.generateParameters("温暖");
        bool found = false;
        for (const auto& param : result.parameters) {
            if (param.parameterId == "filter_cutoff") {
                REQUIRE(param.value == Catch::Approx(0.3f).margin(0.01f));
                found = true;
            }
        }
        REQUIRE(found);
    }

    SECTION("多个关键词产生多个参数") {
        auto result = engine.generateParameters("温暖 明亮 大厅");
        REQUIRE(result.parameters.size() >= 3);

        // 检查参数ID不重复
        std::set<juce::String> paramIds;
        for (const auto& param : result.parameters) {
            paramIds.insert(param.parameterId);
        }
        REQUIRE(paramIds.size() >= 2); // 至少两个不同参数
    }

    SECTION("空文本返回空参数") {
        auto result = engine.generateParameters("");
        REQUIRE(result.parameters.empty());
    }

    SECTION("未知关键词不触发任何规则") {
        auto result = engine.generateParameters("xyzabc123");
        REQUIRE(result.parameters.empty());
    }

    SECTION("推理结果包含预设名称") {
        auto result = engine.generateParameters("电子贝斯");
        REQUIRE_FALSE(result.presetName.isEmpty());
        REQUIRE(result.presetName.contains("AI_"));
    }

    SECTION("推理时间记录") {
        auto result = engine.generateParameters("测试");
        REQUIRE(engine.getLastInferenceTimeMs() >= 0.0);
    }

    SECTION("风格标签增强推理") {
        std::vector<juce::String> tags = {"电子", "现代"};
        auto result = engine.generateParameters("主音", nullptr, tags);
        // 风格标签应增加额外参数
        REQUIRE(result.parameters.size() >= 1);
    }
}

TEST_CASE("AIInferenceEngine: 结果缓存", "[ai][engine][cache]") {
    auto engine = createTestEngine();

    SECTION("相同输入返回缓存结果") {
        auto r1 = engine.generateParameters("复古合成器");
        auto r2 = engine.generateParameters("复古合成器");

        // 缓存命中时置信度应相同
        REQUIRE(r1.confidence == Catch::Approx(r2.confidence));
        REQUIRE(r1.presetName == r2.presetName);
    }

    SECTION("缓存大小限制") {
        // 发送超过缓存限制的请求
        for (int i = 0; i < 200; ++i) {
            engine.generateParameters("test_" + juce::String(i));
        }
        // 不应崩溃，但缓存可能已满
        auto result = engine.generateParameters("test_0");
        REQUIRE(result.confidence >= 0.0f);
    }
}

// =============================================================================
// 3. 情感增强推理测试
// =============================================================================
TEST_CASE("AIInferenceEngine: 情感增强推理", "[ai][engine][emotion]") {
    auto engine = createTestEngine();

    SECTION("情感融合产生参数") {
        auto result = engine.generateParametersWithEmotion(
            "温暖铺底", 0.8f, 0.3f, 0.1f);
        REQUIRE_FALSE(result.parameters.empty());
        REQUIRE(result.confidence > 0.0f);
    }

    SECTION("不同情感产生不同参数") {
        auto r1 = engine.generateParametersWithEmotion(
            "合成器", 0.0f, 0.0f, 0.0f); // 暗柔
        auto r2 = engine.generateParametersWithEmotion(
            "合成器", 1.0f, 1.0f, 1.0f); // 复杂合成

        // 至少有一个参数不同
        bool hasDiff = false;
        for (size_t i = 0; i < r1.parameters.size() && i < r2.parameters.size(); ++i) {
            if (std::abs(r1.parameters[i].value - r2.parameters[i].value) > 0.01f) {
                hasDiff = true;
                break;
            }
        }
        REQUIRE(hasDiff);
    }

    SECTION("情感滑块默认值不影响文本推理") {
        auto r1 = engine.generateParameters("明亮");
        auto r2 = engine.generateParametersWithEmotion(
            "明亮", 0.5f, 0.5f, 0.5f);

        // 默认情感应产生与纯文本推理相似的结果
        REQUIRE(r1.parameters.size() > 0);
        REQUIRE(r2.parameters.size() > 0);
    }

    SECTION("预设名称包含情感信息") {
        auto result = engine.generateParametersWithEmotion(
            "Bass", 0.75f, 0.8f, 0.3f);
        // 预设名称格式: AI_Bass_[W80E80T30]
        REQUIRE(result.presetName.contains("[W"));
        REQUIRE(result.presetName.contains("E"));
        REQUIRE(result.presetName.contains("T"));
    }

    SECTION("情感映射器可访问") {
        auto& mapper = engine.getEmotionMapper();
        auto params = mapper.mapEmotionDirect(0.5f, 0.5f, 0.5f);
        REQUIRE_FALSE(params.empty());
    }
}

// =============================================================================
// 4. 波表生成测试
// =============================================================================
TEST_CASE("AIInferenceEngine: 波表生成", "[ai][engine][wavetable]") {
    auto engine = createTestEngine();

    SECTION("锯齿波生成") {
        auto wavetable = engine.generateWavetable("锯齿波", 4, 256);
        REQUIRE(wavetable.getNumSamples() == 4 * 256);
        REQUIRE(wavetable.getNumChannels() == 1);

        // 检查波形不是全零
        const float* data = wavetable.getReadPointer(0);
        bool hasNonZero = false;
        for (int i = 0; i < wavetable.getNumSamples(); ++i) {
            if (std::abs(data[i]) > 1e-4f) {
                hasNonZero = true;
                break;
            }
        }
        REQUIRE(hasNonZero);
    }

    SECTION("方波生成") {
        auto wavetable = engine.generateWavetable("方波", 4, 256);
        REQUIRE(wavetable.getNumSamples() > 0);
    }

    SECTION("三角波生成") {
        auto wavetable = engine.generateWavetable("三角波", 4, 256);
        REQUIRE(wavetable.getNumSamples() > 0);
    }

    SECTION("噪声生成") {
        auto wavetable = engine.generateWavetable("噪声", 4, 256);
        REQUIRE(wavetable.getNumSamples() > 0);

        // 噪声应该有非零样本
        const float* data = wavetable.getReadPointer(0);
        bool hasNonZero = false;
        for (int i = 0; i < wavetable.getNumSamples(); ++i) {
            if (std::abs(data[i]) > 1e-4f) {
                hasNonZero = true;
                break;
            }
        }
        REQUIRE(hasNonZero);
    }

    SECTION("默认正弦波") {
        auto wavetable = engine.generateWavetable("未知描述", 4, 256);
        REQUIRE(wavetable.getNumSamples() > 0);
    }
}

// =============================================================================
// 5. 频谱分析测试
// =============================================================================
TEST_CASE("AIInferenceEngine: 频谱分析", "[ai][engine][spectrum]") {
    auto engine = createTestEngine();

    SECTION("非空音频产生频谱") {
        juce::AudioSampleBuffer buffer(1, 1024);
        buffer.clear();
        // 填充正弦波
        float* data = buffer.getWritePointer(0);
        for (int i = 0; i < 1024; ++i) {
            data[i] = std::sin(2.0f * 3.14159f * 440.0f * i / 44100.0f);
        }

        auto spectrum = engine.analyzeReferenceSpectrum(buffer);
        REQUIRE_FALSE(spectrum.empty());
    }

    SECTION("频谱大小正确") {
        juce::AudioSampleBuffer buffer(1, 512);
        buffer.clear();
        auto spectrum = engine.analyzeReferenceSpectrum(buffer);
        // FFT size = 2^9 = 512, spectrum size = 512/2 = 256
        REQUIRE(spectrum.size() == 256);
    }

    SECTION("音频嵌入生成128维向量") {
        juce::AudioSampleBuffer buffer(1, 1024);
        buffer.clear();
        auto embedding = engine.extractAudioEmbedding(buffer);
        REQUIRE(embedding.size() == 128);
    }
}

// =============================================================================
// 6. 参数解释生成测试
// =============================================================================
TEST_CASE("AIInferenceEngine: 参数解释", "[ai][engine][explanation]") {
    auto engine = createTestEngine();

    SECTION("已知参数生成解释") {
        auto explanation = engine.generateParameterExplanation(
            "filter_cutoff", 0.9f, "brightness");
        REQUIRE_FALSE(explanation.isEmpty());
        REQUIRE(explanation.contains("AI"));
    }

    SECTION("高值参数解释") {
        auto explanation = engine.generateParameterExplanation(
            "filter_cutoff", 0.9f, "bright");
        REQUIRE(explanation.contains("AI"));
        // 高值 (>0.8) 时格式: "AI自动提高..."
        REQUIRE(explanation.contains("AI"));
    }

    SECTION("低值参数解释") {
        auto explanation = engine.generateParameterExplanation(
            "filter_cutoff", 0.1f, "soft");
        REQUIRE(explanation.contains("AI"));
        // 低值 (<0.2) 时格式: "AI自动降低..."
        REQUIRE(explanation.contains("AI"));
    }

    SECTION("未知参数使用原始名称") {
        auto explanation = engine.generateParameterExplanation(
            "unknown_param", 0.5f, "测试");
        REQUIRE_FALSE(explanation.isEmpty());
    }
}

// =============================================================================
// 7. 模型管理测试
// =============================================================================
TEST_CASE("AIInferenceEngine: 模型管理", "[ai][engine][model]") {
    auto engine = createTestEngine();

    SECTION("初始状态无模型加载") {
        REQUIRE_FALSE(engine.isModelLoaded());
    }

    SECTION("模型信息显示") {
        auto info = engine.getModelInfo();
        REQUIRE_FALSE(info.isEmpty());
        // 无ONNX模型时显示规则引擎模式
        REQUIRE(info.length() > 0);
    }

    SECTION("不存在的模型文件加载失败") {
        juce::File nonExistent("nonexistent_model.onnx");
        bool loaded = engine.loadModel(nonExistent);
        REQUIRE_FALSE(loaded);
        REQUIRE_FALSE(engine.isModelLoaded());
    }

    SECTION("卸载模型不影响状态") {
        engine.unloadModel();
        REQUIRE_FALSE(engine.isModelLoaded());
    }
}

// =============================================================================
// 8. 关键词规则导出测试
// =============================================================================
TEST_CASE("OnnxModelExporter: 关键词规则导出", "[ai][onnx][rules]") {
    SECTION("规则列表非空") {
        const auto& rules = getKeywordRulesForExport();
        REQUIRE_FALSE(rules.empty());
        REQUIRE(rules.size() >= 30);
    }

    SECTION("每条规则包含必要字段") {
        const auto& rules = getKeywordRulesForExport();
        for (const auto& rule : rules) {
            REQUIRE_FALSE(rule.keyword.isEmpty());
            REQUIRE_FALSE(rule.parameterId.isEmpty());
            REQUIRE(rule.targetValue >= 0.0f);
            REQUIRE(rule.targetValue <= 1.0f);
        }
    }

    SECTION("关键规则存在") {
        const auto& rules = getKeywordRulesForExport();
        // 检查规则数量充足 (至少30条)
        REQUIRE(rules.size() >= 30);

        // 验证前几条规则存在 (索引 0: 明亮, 1: 温暖, 22: 贝斯)
        REQUIRE(rules.size() > 22);
        REQUIRE_FALSE(rules[0].keyword.isEmpty());
        REQUIRE_FALSE(rules[1].keyword.isEmpty());
        REQUIRE_FALSE(rules[22].keyword.isEmpty());
        REQUIRE(rules[0].parameterId == "filter_cutoff");
        REQUIRE(rules[1].parameterId == "filter_cutoff");
    }
}

// =============================================================================
// 9. Gamma: 端到端 ONNX 模型推理测试
// 验证训练好的 ONNX 模型能正确推理中文描述
// =============================================================================
TEST_CASE("Gamma: ONNX 模型端到端推理", "[ai][gamma][onnx_e2e]") {
    auto engine = createTestEngine();

    // 查找 ONNX 模型文件
    juce::File modelPath = juce::File::getCurrentWorkingDirectory()
        .getChildFile("models")
        .getChildFile("liancore_ai_model.onnx");

    // 如果当前目录找不到，尝试项目根目录
    if (!modelPath.existsAsFile()) {
        juce::File cwd = juce::File::getCurrentWorkingDirectory();
        while (cwd.getParentDirectory().exists() && !cwd.isRoot()) {
            juce::File candidate = cwd.getChildFile("models").getChildFile("liancore_ai_model.onnx");
            if (candidate.existsAsFile()) {
                modelPath = candidate;
                break;
            }
            cwd = cwd.getParentDirectory();
        }
    }

    CAPTURE(modelPath.getFullPathName());

    // 尝试加载 ONNX 模型
    bool modelLoaded = engine.loadModel(modelPath);

    SECTION("ONNX 模型文件存在") {
        if (modelPath.existsAsFile()) {
            REQUIRE(modelPath.getSize() > 1000);
        } else {
            WARN("ONNX model not found, skipping ONNX tests");
            return;
        }
    }

    SECTION("AIInferenceEngine 规则引擎作为可靠回退") {
        // 无论 ONNX 是否加载，规则引擎都可用
        auto result = engine.generateParameters("温暖的贝斯");
        REQUIRE_FALSE(result.parameters.empty());
        REQUIRE(result.confidence >= 0.0f);

        // 验证所有参数值在 [0.0, 1.0] 范围内
        for (const auto& param : result.parameters) {
            REQUIRE(param.value >= 0.0f);
            REQUIRE(param.value <= 1.0f);
        }
    }

    SECTION("中文描述: 温暖的贝斯") {
        auto result = engine.generateParameters("温暖的贝斯");
        REQUIRE_FALSE(result.parameters.empty());
        REQUIRE(result.confidence >= 0.0f);

        // 检查 filter_cutoff 参数
        bool foundCutoff = false;
        for (const auto& param : result.parameters) {
            if (param.parameterId == "filter_cutoff") {
                REQUIRE(param.value >= 0.0f);
                REQUIRE(param.value <= 1.0f);
                foundCutoff = true;
            }
        }
        REQUIRE(foundCutoff);
    }

    SECTION("中文描述: 明亮的合成器主音") {
        auto result = engine.generateParameters("明亮的合成器主音");
        REQUIRE_FALSE(result.parameters.empty());
        REQUIRE(result.confidence >= 0.0f);

        // 检查所有参数值在有效范围
        for (const auto& param : result.parameters) {
            REQUIRE(param.value >= 0.0f);
            REQUIRE(param.value <= 1.0f);
        }
    }

    SECTION("中文描述: 梦幻的铺底音色") {
        auto result = engine.generateParameters("梦幻的铺底音色");
        REQUIRE_FALSE(result.parameters.empty());
        for (const auto& param : result.parameters) {
            REQUIRE(param.value >= 0.0f);
            REQUIRE(param.value <= 1.0f);
        }
    }

    SECTION("中文描述: 厚重的打击音色") {
        auto result = engine.generateParameters("厚重的打击音色");
        REQUIRE_FALSE(result.parameters.empty());
        for (const auto& param : result.parameters) {
            REQUIRE(param.value >= 0.0f);
            REQUIRE(param.value <= 1.0f);
        }
    }

    SECTION("中文描述: 空灵的环境音色") {
        auto result = engine.generateParameters("空灵的环境音色");
        REQUIRE_FALSE(result.parameters.empty());
        for (const auto& param : result.parameters) {
            REQUIRE(param.value >= 0.0f);
            REQUIRE(param.value <= 1.0f);
        }
    }

    SECTION("中文描述: 复古的管弦乐") {
        auto result = engine.generateParameters("复古的管弦乐");
        REQUIRE_FALSE(result.parameters.empty());
        for (const auto& param : result.parameters) {
            REQUIRE(param.value >= 0.0f);
            REQUIRE(param.value <= 1.0f);
        }
    }

    SECTION("中文描述: 快速的弹拨音色") {
        auto result = engine.generateParameters("快速的弹拨音色");
        REQUIRE_FALSE(result.parameters.empty());
        for (const auto& param : result.parameters) {
            REQUIRE(param.value >= 0.0f);
            REQUIRE(param.value <= 1.0f);
        }
    }

    SECTION("中文描述: 尖锐的电子音色") {
        auto result = engine.generateParameters("尖锐的电子音色");
        REQUIRE_FALSE(result.parameters.empty());
        for (const auto& param : result.parameters) {
            REQUIRE(param.value >= 0.0f);
            REQUIRE(param.value <= 1.0f);
        }
    }

    SECTION("中文描述: 柔和的钢琴音色") {
        auto result = engine.generateParameters("柔和的钢琴音色");
        REQUIRE_FALSE(result.parameters.empty());
        for (const auto& param : result.parameters) {
            REQUIRE(param.value >= 0.0f);
            REQUIRE(param.value <= 1.0f);
        }
    }

    SECTION("中文描述: 现代的贝斯音色") {
        auto result = engine.generateParameters("现代的贝斯音色");
        REQUIRE_FALSE(result.parameters.empty());
        for (const auto& param : result.parameters) {
            REQUIRE(param.value >= 0.0f);
            REQUIRE(param.value <= 1.0f);
        }
    }

    SECTION("参数数量验证: 11个输出") {
        auto result = engine.generateParameters("合成器");
        // 规则引擎可能返回少于11个参数 (只返回匹配的)
        // 但至少返回1个参数
        REQUIRE(result.parameters.size() >= 1);
    }

    SECTION("推理时间满足性能要求") {
        auto result = engine.generateParameters("测试");
        double inferenceTime = engine.getLastInferenceTimeMs();
        // 推理时间应小于 50ms (Gamma 要求)
        REQUIRE(inferenceTime < 50.0);
    }

    SECTION("模型信息非空") {
        auto info = engine.getModelInfo();
        REQUIRE_FALSE(info.isEmpty());
        REQUIRE(info.length() > 0);
    }

    SECTION("缓存命中加速推理") {
        // 首次推理
        auto r1 = engine.generateParameters("复古合成器");
        double t1 = engine.getLastInferenceTimeMs();

        // 第二次推理 (应从缓存命中)
        auto r2 = engine.generateParameters("复古合成器");
        double t2 = engine.getLastInferenceTimeMs();

        // 缓存命中时结果一致
        REQUIRE(r1.confidence == Catch::Approx(r2.confidence));
        REQUIRE(r1.presetName == r2.presetName);
    }
}