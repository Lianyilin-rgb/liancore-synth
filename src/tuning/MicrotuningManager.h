// =============================================================================
// LianCore - 微音程管理器
// P0-3: 微音程/调音支持
// 管理音阶加载、MIDI 音符到音高的映射
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include "ScalaFileLoader.h"
#include <vector>
#include <string>

namespace LianCore {
namespace Tuning {

// =============================================================================
// MicrotuningManager: 管理微音程调音
// - 加载 Scala .scl 文件
// - 支持预设音阶选择
// - 将 MIDI 音符映射到调音偏移
// - 为合成引擎提供频率倍率
// =============================================================================
class MicrotuningManager {
public:
    MicrotuningManager();

    // ---- 音阶加载 ----

    /** 加载 .scl 文件 */
    bool loadScalaFile(const juce::File& file);

    /** 加载预设音阶 */
    bool loadPreset(const std::string& presetName);

    /** 加载内置默认 12-EDO */
    bool loadDefault();

    /** 获取当前音阶 */
    const ScaleDefinition& getCurrentScale() const { return loader_.getScale(); }

    /** 获取当前音阶名称 */
    std::string getCurrentScaleName() const {
        return getCurrentScale().name;
    }

    /** 获取可用预设列表 */
    static std::vector<std::string> getAvailablePresets() {
        return ScalaFileLoader::getPresetNames();
    }

    // ---- 调音查询 ----

    /** 获取指定 MIDI 音符的调音偏移 (cents) */
    double getTuningOffset(int midiNote) const;

    /** 获取指定 MIDI 音符的频率倍率 (相对于12-EDO) */
    double getTuningRatio(int midiNote) const;

    /** 获取基础频率 (A4=440Hz 默认) */
    double getBaseFrequency() const { return baseFrequency_; }

    /** 设置基础频率 (A4=440Hz 默认) */
    void setBaseFrequency(double freq) { baseFrequency_ = freq; }

    /** 获取指定 MIDI 音符的实际频率 (Hz) */
    double getNoteFrequency(int midiNote) const;

    // ---- 状态查询 ----

    /** 是否已加载音阶 */
    bool isTuningLoaded() const { return loader_.isLoaded(); }

    /** 重置为默认 12-EDO */
    void resetToDefault();

    /** 获取上次错误信息 */
    const std::string& getLastError() const { return loader_.getLastError(); }

private:
    ScalaFileLoader loader_;
    double baseFrequency_ = 440.0;  // A4 = 440Hz
    bool isDefault_ = true;
};

} // namespace Tuning
} // namespace LianCore