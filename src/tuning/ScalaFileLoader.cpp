// =============================================================================
// LianCore - Scala .scl 文件加载器 实现
// P0-3: 微音程/调音支持
// =============================================================================
#include "ScalaFileLoader.h"
#include <sstream>
#include <cmath>
#include <cstdlib>

namespace LianCore {
namespace Tuning {

// =============================================================================
// 解析一行值: 支持 cents 和比例
// =============================================================================
double ScalaFileLoader::parseValue(const std::string& line) {
    if (line.empty()) return 0.0;

    // 检查是否为比例 (如 "3/2")
    size_t slashPos = line.find('/');
    if (slashPos != std::string::npos) {
        // 尝试解析比例
        try {
            double numerator = std::stod(line.substr(0, slashPos));
            double denominator = std::stod(line.substr(slashPos + 1));
            if (denominator != 0.0) {
                // 比例转 cents: 1200 * log2(numerator/denominator)
                return 1200.0 * std::log2(numerator / denominator);
            }
        } catch (...) {
            // 解析失败, 尝试作为 cents 值
        }
    }

    // 尝试解析为 cents 值
    try {
        return std::stod(line);
    } catch (...) {
        return 0.0;
    }
}

// =============================================================================
// 从字符串内容加载
// =============================================================================
bool ScalaFileLoader::loadFromString(const std::string& content) {
    scale_.clear();
    lastError_.clear();

    std::istringstream stream(content);
    std::string line;
    std::string description;
    int expectedNotes = 0;
    bool foundNumNotes = false;

    while (std::getline(stream, line)) {
        // 去除首尾空白
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        size_t end = line.find_last_not_of(" \t\r\n");
        line = line.substr(start, end - start + 1);

        // 跳过注释行
        if (line[0] == '!') {
            if (scale_.name.empty()) {
                // 第一行注释可能是音阶名称
                scale_.name = line.substr(1);
                // 去除开头的空格和文件名
                size_t nameStart = scale_.name.find_first_not_of(" \t");
                if (nameStart != std::string::npos) {
                    scale_.name = scale_.name.substr(nameStart);
                }
            }
            continue;
        }

        // 如果还没找到音符数
        if (!foundNumNotes) {
            // 跳过空行继续找音符数
            if (line.empty()) continue;

            // 检查是否为纯数字行 (音符数)
            // 使用 strtol 验证整行是否可解析为纯整数
            const char* cstr = line.c_str();
            char* endptr = nullptr;
            long val = std::strtol(cstr, &endptr, 10);
            // 跳过前导空白
            while (*endptr == ' ' || *endptr == '\t') ++endptr;

            if (endptr != cstr && *endptr == '\0') {
                // 整行是纯整数 → 这是音符数
                expectedNotes = (int)val;
                if (expectedNotes <= 0 || expectedNotes > 256) {
                    lastError_ = "Invalid number of notes: " + line;
                    scale_.clear();
                    return false;
                }
                foundNumNotes = true;
                scale_.numNotes = expectedNotes;
                scale_.cents.reserve(expectedNotes);
                // 不自动添加根音, 由 SCL 内容定义所有音符
            } else {
                // 不是纯数字 → 描述行
                description = line;
                scale_.description = description;
            }
        } else {
            // 解析音符 cents 值
            if (line.empty()) continue;

            double cents = parseValue(line);
            scale_.cents.push_back(cents);
        }
    }

    // 验证: 必须找到音符数
    if (!foundNumNotes) {
        lastError_ = "No note count found in SCL content";
        scale_.clear();
        return false;
    }

    // 验证音符数
    if ((int)scale_.cents.size() != expectedNotes) {
        lastError_ = "Expected " + std::to_string(expectedNotes)
                   + " notes, but found " + std::to_string(scale_.cents.size());
        scale_.clear();
        return false;
    }

    return true;
}

// =============================================================================
// 从文件加载
// =============================================================================
bool ScalaFileLoader::loadFromFile(const juce::File& file) {
    if (!file.existsAsFile()) {
        lastError_ = "File not found: " + file.getFullPathName().toStdString();
        return false;
    }

    juce::String content = file.loadFileAsString();
    if (content.isEmpty()) {
        lastError_ = "File is empty or could not be read: " + file.getFullPathName().toStdString();
        return false;
    }

    return loadFromString(content.toStdString());
}

// =============================================================================
// 预设音阶
// =============================================================================
ScaleDefinition ScalaFileLoader::create12EDO() {
    ScaleDefinition scale;
    scale.name = "12-EDO (Equal Temperament)";
    scale.description = "Standard 12-tone equal temperament";
    scale.numNotes = 12;
    scale.cents = { 0.0, 100.0, 200.0, 300.0, 400.0, 500.0,
                    600.0, 700.0, 800.0, 900.0, 1000.0, 1100.0 };
    return scale;
}

ScaleDefinition ScalaFileLoader::create24EDO() {
    ScaleDefinition scale;
    scale.name = "24-EDO (Quarter Tone)";
    scale.description = "24-tone equal temperament";
    scale.numNotes = 24;
    for (int i = 0; i < 24; ++i) {
        scale.cents.push_back(i * 50.0);
    }
    return scale;
}

std::vector<std::string> ScalaFileLoader::getPresetNames() {
    return {
        "12-EDO (Equal Temperament)",
        "24-EDO (Quarter Tone)",
        "Just Intonation (Major)",
        "Pythagorean",
        "Pelog (5-tone)",
        "Slendro (5-tone)",
        "Bohlen-Pierce (13-tone)"
    };
}

ScaleDefinition ScalaFileLoader::loadPreset(const std::string& presetName) {
    if (presetName.find("12-EDO") != std::string::npos) {
        return create12EDO();
    }
    if (presetName.find("24-EDO") != std::string::npos) {
        return create24EDO();
    }
    if (presetName.find("Just Intonation") != std::string::npos) {
        ScaleDefinition scale;
        scale.name = "Just Intonation (Major)";
        scale.description = "5-limit just intonation major scale";
        scale.numNotes = 12;
        scale.cents = {
            0.0,          // 1/1
            111.731,      // 16/15
            203.910,      // 9/8
            315.641,      // 6/5
            386.314,      // 5/4
            498.045,      // 4/3
            590.224,      // 45/32
            701.955,      // 3/2
            813.686,      // 8/5
            884.359,      // 5/3
            905.865,      // 16/9 (minor 7th)
            1088.269      // 15/8
        };
        return scale;
    }
    if (presetName.find("Pythagorean") != std::string::npos) {
        ScaleDefinition scale;
        scale.name = "Pythagorean";
        scale.description = "Pythagorean tuning (3-limit)";
        scale.numNotes = 12;
        scale.cents = {
            0.0, 90.225, 203.910, 294.135,
            407.820, 498.045, 588.270, 701.955,
            792.180, 905.865, 996.090, 1109.775
        };
        return scale;
    }
    if (presetName.find("Pelog") != std::string::npos) {
        ScaleDefinition scale;
        scale.name = "Pelog (5-tone)";
        scale.description = "Javanese Pelog scale";
        scale.numNotes = 5;
        scale.cents = { 0.0, 120.0, 520.0, 720.0, 920.0 };
        return scale;
    }
    if (presetName.find("Slendro") != std::string::npos) {
        ScaleDefinition scale;
        scale.name = "Slendro (5-tone)";
        scale.description = "Javanese Slendro scale";
        scale.numNotes = 5;
        scale.cents = { 0.0, 240.0, 480.0, 720.0, 960.0 };
        return scale;
    }
    if (presetName.find("Bohlen-Pierce") != std::string::npos) {
        ScaleDefinition scale;
        scale.name = "Bohlen-Pierce (13-tone)";
        scale.description = "Bohlen-Pierce scale based on 3:1 ratio";
        scale.numNotes = 13;
        // BP scale: 13 equal divisions of the tritave (3:1 ≈ 1901.955 cents)
        double step = 1901.955 / 13.0;
        for (int i = 0; i < 13; ++i) {
            scale.cents.push_back(i * step);
        }
        return scale;
    }

    // 默认返回 12-EDO
    return create12EDO();
}

} // namespace Tuning
} // namespace LianCore