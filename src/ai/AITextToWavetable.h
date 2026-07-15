// =============================================================================
// LianCore - AI 文本到波表转换引擎
// 纯逻辑层，不依赖 JUCE GUI，可独立测试
// =============================================================================
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

namespace LianCore {
namespace AI {

// =============================================================================
// AITextToWavetable - 基于规则的文本→波表生成器
// 解析文本描述中的关键词，映射为谐波结构，生成多帧波表
// =============================================================================
class AITextToWavetable {
public:
    static constexpr int kMaxHarmonics = 64;
    static constexpr int kDefaultFrames = 256;
    static constexpr int kDefaultFrameSize = 2048;

    // 生成结果
    struct Result {
        std::vector<std::vector<float>> frames;  // frames[frame][sample]
        std::vector<float> harmonics;             // 最终谐波结构
        std::string matchedKeywords;              // 匹配到的关键词
        bool success = false;
    };

    // 从文本生成波表
    static Result generate(const std::string& text,
                           int numFrames = kDefaultFrames,
                           int frameSize = kDefaultFrameSize);

    // 从文本生成谐波结构 (仅第一帧)
    static std::vector<float> textToHarmonics(const std::string& text);

    // 从谐波生成多帧波表 (模拟渐变)
    static std::vector<std::vector<float>> harmonicsToFrames(
        const std::vector<float>& harmonics,
        int numFrames = kDefaultFrames,
        int frameSize = kDefaultFrameSize);

    // 获取所有支持的关键词
    static std::vector<std::string> getSupportedKeywords();

private:
    // 关键词匹配
    static bool matchKeyword(const std::string& text, const std::string& keyword);
    static std::string extractKeywords(const std::string& text);

    // 波形生成
    static std::vector<float> generateFrame(const std::vector<float>& harmonics,
                                            int frameSize, float phaseOff = 0.0f);
};

} // namespace AI
} // namespace LianCore