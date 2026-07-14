// =============================================================================
// LianCore - Scala .scl 文件加载器
// P0-3: 微音程/调音支持
// 解析 Scala 音阶文件 (.scl)，提取音阶定义
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <string>

namespace LianCore {
namespace Tuning {

// =============================================================================
// 音阶定义: 存放从 .scl 文件解析出的音阶数据
// =============================================================================
struct ScaleDefinition {
    std::string name;              // 音阶名称 (从 ! 注释行提取)
    std::string description;       // 音阶描述
    std::vector<double> cents;     // 每个音符的 cents 值 (相对于根音)
    int numNotes = 0;              // 音阶中的音符数

    /** 获取指定音符的 cents 偏移 (0-based, 相对于根音) */
    double getCents(int noteIndex) const {
        if (noteIndex < 0 || noteIndex >= (int)cents.size())
            return 0.0;
        return cents[noteIndex];
    }

    /** 获取指定音符的频率倍率 (相对于根音) */
    double getFrequencyRatio(int noteIndex) const {
        return std::pow(2.0, getCents(noteIndex) / 1200.0);
    }

    /** 判断音阶是否为空 */
    bool isEmpty() const { return cents.empty(); }

    /** 清除所有数据 */
    void clear() {
        name.clear();
        description.clear();
        cents.clear();
        numNotes = 0;
    }
};

// =============================================================================
// Scala .scl 文件加载器
// 支持标准 Scala 音阶格式:
//   ! comment
//   description
//   <numNotes>
//   !
//   <cents_or_ratio>
//   ...
// =============================================================================
class ScalaFileLoader {
public:
    ScalaFileLoader() = default;

    /** 从文件路径加载 .scl 文件 */
    bool loadFromFile(const juce::File& file);

    /** 从字符串内容加载 .scl 格式数据 */
    bool loadFromString(const std::string& content);

    /** 获取已加载的音阶定义 */
    const ScaleDefinition& getScale() const { return scale_; }

    /** 获取上次加载的错误信息 */
    const std::string& getLastError() const { return lastError_; }

    /** 判断是否已成功加载音阶 */
    bool isLoaded() const { return !scale_.isEmpty(); }

    /** 生成默认的 12-EDO 音阶 */
    static ScaleDefinition create12EDO();

    /** 生成默认的 24-EDO 音阶 (八度音阶) */
    static ScaleDefinition create24EDO();

    /** 列出预设音阶名称 */
    static std::vector<std::string> getPresetNames();

    /** 加载预设音阶 */
    static ScaleDefinition loadPreset(const std::string& presetName);

private:
    /** 解析一行内容: 支持 cents 值 (如 "100.0") 和比例 (如 "3/2") */
    static double parseValue(const std::string& line);

    ScaleDefinition scale_;
    std::string lastError_;
};

} // namespace Tuning
} // namespace LianCore