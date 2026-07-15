// =============================================================================
// LianCore - AI 文本到波表转换引擎实现
// 纯逻辑层，无 GUI 依赖
// =============================================================================
#include "AITextToWavetable.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <cctype>

namespace LianCore {
namespace AI {

// =============================================================================
// 关键词匹配
// =============================================================================
bool AITextToWavetable::matchKeyword(const std::string& text, const std::string& keyword) {
    std::string lower = text;
    std::string lowerKW = keyword;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    std::transform(lowerKW.begin(), lowerKW.end(), lowerKW.begin(), ::tolower);
    return lower.find(lowerKW) != std::string::npos;
}

// =============================================================================
// 文本 → 谐波结构
// =============================================================================
std::vector<float> AITextToWavetable::textToHarmonics(const std::string& text) {
    std::vector<float> harmonics(kMaxHarmonics, 0.0f);
    harmonics[0] = 1.0f;  // 基频

    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // --- 基础波形类型 ---
    if (matchKeyword(lower, "saw") || matchKeyword(lower, "锯齿")) {
        for (int i = 0; i < kMaxHarmonics; ++i)
            harmonics[i] = 1.0f / (i + 1);
    } else if (matchKeyword(lower, "square") || matchKeyword(lower, "方波")) {
        for (int i = 0; i < kMaxHarmonics; i += 2)
            harmonics[i] = 1.0f / (i + 1);
    } else if (matchKeyword(lower, "triangle") || matchKeyword(lower, "tri") ||
               matchKeyword(lower, "三角")) {
        for (int i = 0; i < kMaxHarmonics; i += 2) {
            float n = static_cast<float>(i + 1);
            harmonics[i] = 1.0f / (n * n);
        }
    } else if (matchKeyword(lower, "sine") || matchKeyword(lower, "正弦")) {
        harmonics[0] = 1.0f;
    } else if (matchKeyword(lower, "pulse") || matchKeyword(lower, "脉冲")) {
        for (int i = 0; i < kMaxHarmonics; ++i)
            harmonics[i] = 0.5f / (i + 1);
    }

    // --- 音色修饰 ---
    bool isBright = matchKeyword(lower, "bright") || matchKeyword(lower, "明亮") ||
                    matchKeyword(lower, "harsh") || matchKeyword(lower, "尖锐");
    bool isDark = matchKeyword(lower, "dark") || matchKeyword(lower, "暗") ||
                  matchKeyword(lower, "warm") || matchKeyword(lower, "温暖") ||
                  matchKeyword(lower, "soft") || matchKeyword(lower, "柔和");
    bool isRich = matchKeyword(lower, "rich") || matchKeyword(lower, "丰富") ||
                  matchKeyword(lower, "fat") || matchKeyword(lower, "肥厚");
    bool isThin = matchKeyword(lower, "thin") || matchKeyword(lower, "薄") ||
                  matchKeyword(lower, "light") || matchKeyword(lower, "轻");

    if (isBright) {
        for (int i = 0; i < kMaxHarmonics; ++i)
            harmonics[i] *= (1.0f + 0.5f * static_cast<float>(i) / (kMaxHarmonics - 1));
    }
    if (isDark) {
        for (int i = 0; i < kMaxHarmonics; ++i)
            harmonics[i] *= std::exp(-static_cast<float>(i) / 8.0f);
    }
    if (isRich) {
        for (int i = 0; i < kMaxHarmonics; ++i)
            if (harmonics[i] < 0.05f) harmonics[i] = 0.3f / (i + 1);
    }
    if (isThin) {
        for (int i = 8; i < kMaxHarmonics; ++i) harmonics[i] = 0.0f;
    }

    // --- 乐器类型 ---
    if (matchKeyword(lower, "organ") || matchKeyword(lower, "管风琴") || matchKeyword(lower, "风琴")) {
        for (int i = 0; i < 12; ++i) harmonics[i] = 1.0f / (i + 1);
        for (int i = 12; i < kMaxHarmonics; ++i) harmonics[i] = 0.0f;
    }
    if (matchKeyword(lower, "string") || matchKeyword(lower, "弦乐") ||
        matchKeyword(lower, "violin") || matchKeyword(lower, "cello") ||
        matchKeyword(lower, "小提琴") || matchKeyword(lower, "大提琴")) {
        for (int i = 0; i < kMaxHarmonics; ++i)
            harmonics[i] = 0.8f / (i + 1) * std::exp(-static_cast<float>(i) / 20.0f);
    }
    if (matchKeyword(lower, "brass") || matchKeyword(lower, "铜管") ||
        matchKeyword(lower, "trumpet") || matchKeyword(lower, "horn") ||
        matchKeyword(lower, "小号") || matchKeyword(lower, "号")) {
        for (int i = 0; i < kMaxHarmonics; ++i) {
            harmonics[i] = (i % 2 == 0) ? 1.0f / (i + 1) : 0.3f / (i + 1);
        }
    }
    if (matchKeyword(lower, "bell") || matchKeyword(lower, "钟声") || matchKeyword(lower, "铃") ||
        matchKeyword(lower, "chime") || matchKeyword(lower, "金属")) {
        harmonics[0] = 1.0f;  harmonics[2] = 0.7f;  harmonics[5] = 0.5f;
        harmonics[9] = 0.3f;  harmonics[14] = 0.2f; harmonics[20] = 0.15f;
        harmonics[27] = 0.1f; harmonics[35] = 0.07f;
        for (int i = 0; i < kMaxHarmonics; ++i)
            harmonics[i] *= std::exp(-static_cast<float>(i) / 12.0f);
    }
    if (matchKeyword(lower, "wood") || matchKeyword(lower, "木管") ||
        matchKeyword(lower, "flute") || matchKeyword(lower, "笛") ||
        matchKeyword(lower, "clarinet") || matchKeyword(lower, "oboe")) {
        harmonics[0] = 1.0f; harmonics[1] = 0.6f; harmonics[2] = 0.3f; harmonics[3] = 0.15f;
        for (int i = 4; i < kMaxHarmonics; ++i) harmonics[i] = 0.0f;
    }

    // --- 效果修饰 ---
    if (matchKeyword(lower, "distorted") || matchKeyword(lower, "失真") || matchKeyword(lower, "overdrive")) {
        for (int i = 0; i < kMaxHarmonics; ++i)
            harmonics[i] = std::sqrt(std::max(0.0f, harmonics[i])) * 0.8f;
    }
    if (matchKeyword(lower, "pad") || matchKeyword(lower, "铺底") ||
        matchKeyword(lower, "ambient") || matchKeyword(lower, "氛围") ||
        matchKeyword(lower, "atmosphere")) {
        for (int i = 0; i < kMaxHarmonics; ++i)
            harmonics[i] = 0.6f / (i + 1) * std::exp(-static_cast<float>(i) / 30.0f);
    }
    if (matchKeyword(lower, "bass") || matchKeyword(lower, "低音") ||
        matchKeyword(lower, "sub") || matchKeyword(lower, "重低音")) {
        harmonics[0] = 1.0f; harmonics[1] = 0.8f; harmonics[2] = 0.5f; harmonics[3] = 0.2f;
        for (int i = 4; i < kMaxHarmonics; ++i) harmonics[i] = 0.0f;
    }
    if (matchKeyword(lower, "lead") || matchKeyword(lower, "主音") || matchKeyword(lower, "solo")) {
        harmonics[0] = 1.0f;
        for (int i = 1; i < kMaxHarmonics; ++i) harmonics[i] = 0.7f / (i + 1);
    }
    if (matchKeyword(lower, "pluck") || matchKeyword(lower, "拨弦") ||
        matchKeyword(lower, "guitar") || matchKeyword(lower, "吉他") ||
        matchKeyword(lower, "harp") || matchKeyword(lower, "竖琴")) {
        for (int i = 0; i < kMaxHarmonics; ++i)
            harmonics[i] = 0.7f / (i + 1) * std::exp(-static_cast<float>(i) / 5.0f);
    }
    if (matchKeyword(lower, "choir") || matchKeyword(lower, "合唱") ||
        matchKeyword(lower, "vocal") || matchKeyword(lower, "人声") ||
        matchKeyword(lower, "voice") || matchKeyword(lower, "vox")) {
        harmonics[0] = 1.0f; harmonics[1] = 0.9f; harmonics[2] = 0.8f;
        harmonics[3] = 0.6f; harmonics[4] = 0.4f; harmonics[5] = 0.2f;
        for (int i = 6; i < kMaxHarmonics; ++i) harmonics[i] = 0.1f / (i + 1);
    }

    // 限制在 [0, 1] 范围
    for (int i = 0; i < kMaxHarmonics; ++i)
        harmonics[i] = std::max(0.0f, std::min(1.0f, harmonics[i]));

    return harmonics;
}

// =============================================================================
// 生成单帧波形
// =============================================================================
std::vector<float> AITextToWavetable::generateFrame(const std::vector<float>& harmonics,
                                                     int frameSize, float phaseOff) {
    std::vector<float> frame(frameSize, 0.0f);
    float norm = 0.0f;
    for (float h : harmonics) norm += h;
    if (norm < 0.001f) norm = 0.001f;

    for (int i = 0; i < frameSize; ++i) {
        float phase = static_cast<float>(i) / static_cast<float>(frameSize) + phaseOff;
        float value = 0.0f;
        for (int h = 0; h < static_cast<int>(harmonics.size()); ++h) {
            if (harmonics[h] > 0.001f) {
                value += harmonics[h] * std::sin(phase * (h + 1) * 2.0f * 3.14159265358979323846f);
            }
        }
        frame[i] = value / norm;
    }

    return frame;
}

// =============================================================================
// 谐波 → 多帧波表
// =============================================================================
std::vector<std::vector<float>> AITextToWavetable::harmonicsToFrames(
    const std::vector<float>& harmonics, int numFrames, int frameSize) {

    std::vector<std::vector<float>> frames(numFrames);

    for (int f = 0; f < numFrames; ++f) {
        float framePhase = static_cast<float>(f) / static_cast<float>(numFrames);
        float modulation = 0.5f + 0.5f * std::sin(framePhase * 2.0f * 3.14159265358979323846f);

        // 调制谐波
        std::vector<float> modulatedHarmonics = harmonics;
        for (size_t i = 0; i < modulatedHarmonics.size(); ++i) {
            float hMod = 1.0f + 0.1f * modulation * std::sin(framePhase * (static_cast<float>(i) + 1));
            modulatedHarmonics[i] = harmonics[i] * hMod;
        }

        frames[f] = generateFrame(modulatedHarmonics, frameSize, framePhase * 0.02f);
    }

    return frames;
}

// =============================================================================
// 主生成方法
// =============================================================================
AITextToWavetable::Result AITextToWavetable::generate(
    const std::string& text, int numFrames, int frameSize) {

    Result result;

    // 文本 → 谐波
    result.harmonics = textToHarmonics(text);

    // 提取关键词
    result.matchedKeywords = extractKeywords(text);

    // 谐波 → 多帧波表
    result.frames = harmonicsToFrames(result.harmonics, numFrames, frameSize);

    result.success = !result.frames.empty();
    return result;
}

// =============================================================================
// 关键词提取
// =============================================================================
std::string AITextToWavetable::extractKeywords(const std::string& text) {
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    std::ostringstream oss;

    const char* allKeywords[] = {
        "saw", "square", "sine", "triangle", "pulse",
        "锯齿", "方波", "正弦", "三角", "脉冲",
        "bright", "dark", "warm", "soft", "rich", "thin",
        "明亮", "暗", "温暖", "柔和", "丰富", "薄",
        "organ", "string", "brass", "bell", "wood",
        "管风琴", "弦乐", "铜管", "钟声", "木管",
        "pad", "bass", "lead", "pluck", "choir",
        "铺底", "低音", "主音", "拨弦", "合唱",
        "distorted", "失真"
    };

    for (const auto& kw : allKeywords) {
        if (matchKeyword(lower, kw)) {
            if (oss.tellp() > 0) oss << ", ";
            oss << kw;
        }
    }

    return oss.str();
}

// =============================================================================
// 获取支持的关键词列表
// =============================================================================
std::vector<std::string> AITextToWavetable::getSupportedKeywords() {
    return {
        "saw/锯齿", "square/方波", "sine/正弦", "triangle/三角", "pulse/脉冲",
        "bright/明亮", "dark/暗", "warm/温暖", "soft/柔和", "rich/丰富", "thin/薄",
        "organ/管风琴", "string/弦乐", "brass/铜管", "bell/钟声", "wood/木管",
        "pad/铺底", "bass/低音", "lead/主音", "pluck/拨弦", "choir/合唱",
        "distorted/失真"
    };
}

} // namespace AI
} // namespace LianCore