// =============================================================================
// LianCore - 微音程管理器 实现
// P0-3: 微音程/调音支持
// =============================================================================
#include "MicrotuningManager.h"
#include <cmath>

namespace LianCore {
namespace Tuning {

// =============================================================================
// 构造函数
// =============================================================================
MicrotuningManager::MicrotuningManager() {
    loadDefault();
}

// =============================================================================
// 音阶加载
// =============================================================================
bool MicrotuningManager::loadScalaFile(const juce::File& file) {
    bool success = loader_.loadFromFile(file);
    if (success) {
        isDefault_ = false;
    }
    return success;
}

bool MicrotuningManager::loadPreset(const std::string& presetName) {
    ScaleDefinition scale = ScalaFileLoader::loadPreset(presetName);
    if (scale.isEmpty()) {
        return false;
    }
    // 直接使用 loader_ 加载 (通过字符串方式)
    // 构建 .scl 内容
    std::string content = "! " + scale.name + "\n";
    content += "!\n";
    content += scale.description + "\n";
    content += " " + std::to_string(scale.numNotes) + "\n";
    content += "!\n";
    for (size_t i = 0; i < scale.cents.size(); ++i) {
        content += " " + std::to_string(scale.cents[i]) + "\n";
    }

    bool success = loader_.loadFromString(content);
    if (success) {
        isDefault_ = false;
    }
    return success;
}

bool MicrotuningManager::loadDefault() {
    isDefault_ = true;
    return loader_.loadFromString(
        "! 12-EDO.scl\n"
        "!\n"
        "12-tone Equal Temperament\n"
        " 12\n"
        "!\n"
        " 0.0\n"
        " 100.0\n"
        " 200.0\n"
        " 300.0\n"
        " 400.0\n"
        " 500.0\n"
        " 600.0\n"
        " 700.0\n"
        " 800.0\n"
        " 900.0\n"
        " 1000.0\n"
        " 1100.0\n"
    );
}

void MicrotuningManager::resetToDefault() {
    loadDefault();
    baseFrequency_ = 440.0;
}

// =============================================================================
// 调音计算
// =============================================================================

double MicrotuningManager::getTuningOffset(int midiNote) const {
    const auto& scale = loader_.getScale();
    if (scale.isEmpty() || scale.numNotes <= 1) {
        return 0.0;
    }

    // 计算 MIDI 音符在音阶中的位置
    // 12-EDO: 每个八度有 12 个音符, 每音符 100 cents
    // 自定义音阶: 每个八度有 numNotes 个音符
    int notesPerOctave = scale.numNotes;
    int octave = midiNote / notesPerOctave;
    int noteIndex = ((midiNote % notesPerOctave) + notesPerOctave) % notesPerOctave;

    // 音阶中该音符的 cents 值
    double noteCents = scale.getCents(noteIndex);

    // 八度偏移 (每八度 1200 cents)
    double octaveCents = octave * 1200.0;

    // 12-EDO 中该音符的 cents 值
    double edo12Cents = midiNote * 100.0;

    // 偏移 = 自定义音阶 cents - 12-EDO cents
    return (octaveCents + noteCents) - edo12Cents;
}

double MicrotuningManager::getTuningRatio(int midiNote) const {
    double offset = getTuningOffset(midiNote);
    return std::pow(2.0, offset / 1200.0);
}

double MicrotuningManager::getNoteFrequency(int midiNote) const {
    // 12-EDO 频率: A4 (MIDI 69) = 440Hz
    double edo12Freq = baseFrequency_ * std::pow(2.0, (midiNote - 69) / 12.0);
    // 应用调音倍率
    return edo12Freq * getTuningRatio(midiNote);
}

} // namespace Tuning
} // namespace LianCore