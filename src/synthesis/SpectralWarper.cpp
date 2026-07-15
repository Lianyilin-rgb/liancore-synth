// =============================================================================
// LianCore - SpectralWarper 频谱变形模块实现
// =============================================================================
#include "SpectralWarper.h"
#include "../utils/AudioUtils.h"
#include <algorithm>
#include <cmath>

namespace LianCore {

// =============================================================================
SpectralWarper::SpectralWarper() {
}

// =============================================================================
void SpectralWarper::setMode(Mode mode) {
    mode_ = mode;
}

juce::String SpectralWarper::getModeName(Mode mode) {
    switch (mode) {
        case Mode::Stretch:    return "拉伸/压缩";
        case Mode::Shift:      return "频移";
        case Mode::Symmetrize: return "对称化";
        case Mode::Quantize:   return "量化";
        case Mode::Fold:       return "折叠";
    }
    return "未知";
}

juce::String SpectralWarper::getModeName(int modeIndex) {
    return getModeName(static_cast<Mode>(modeIndex));
}

void SpectralWarper::setAmount(float amount) {
    amount_ = juce::jlimit(0.0f, 1.0f, amount);
}

void SpectralWarper::setStretchAmount(float stretch) {
    stretchAmount_ = juce::jlimit(0.5f, 4.0f, stretch);
}

void SpectralWarper::setShiftAmount(float semitones) {
    shiftAmount_ = juce::jlimit(-24.0f, 24.0f, semitones);
}

void SpectralWarper::setSymmetrizeAmount(float amount) {
    symmetrizeAmount_ = juce::jlimit(0.0f, 1.0f, amount);
}

void SpectralWarper::setQuantizeAmount(float amount) {
    quantizeAmount_ = juce::jlimit(0.0f, 1.0f, amount);
}

void SpectralWarper::setFoldAmount(float amount) {
    foldAmount_ = juce::jlimit(0.0f, 1.0f, amount);
}

// =============================================================================
// 核心处理入口
// =============================================================================
void SpectralWarper::process(std::vector<float>& magnitudes, double sampleRate, int fftSize) {
    if (magnitudes.empty()) return;

    switch (mode_) {
        case Mode::Stretch:
            // 归一化amount映射到0.5-4.0
            applyStretch(magnitudes, 0.5f + amount_ * 3.5f);
            break;
        case Mode::Shift:
            // 归一化amount映射到±24半音
            applyShift(magnitudes, -24.0f + amount_ * 48.0f, sampleRate, fftSize);
            break;
        case Mode::Symmetrize:
            applySymmetrize(magnitudes, amount_, sampleRate, fftSize);
            break;
        case Mode::Quantize:
            applyQuantize(magnitudes, amount_);
            break;
        case Mode::Fold:
            applyFold(magnitudes, amount_, sampleRate, fftSize);
            break;
    }
}

// =============================================================================
// 频谱拉伸 - 重映射频率bin索引
// stretch: 0.5x(压缩) ~ 4.0x(拉伸)
// 算法: 对每个输出bin, 反向映射到源bin进行线性插值
// =============================================================================
void SpectralWarper::applyStretch(std::vector<float>& magnitudes, float stretch) {
    int numBins = static_cast<int>(magnitudes.size());
    std::vector<float> stretched(numBins, 0.0f);

    float invStretch = 1.0f / stretch;

    for (int i = 0; i < numBins; ++i) {
        float srcBin = static_cast<float>(i) * invStretch;

        if (srcBin < static_cast<float>(numBins - 1)) {
            int srcIdx = static_cast<int>(srcBin);
            float frac = srcBin - static_cast<float>(srcIdx);

            if (srcIdx + 1 < numBins) {
                stretched[i] = magnitudes[srcIdx] * (1.0f - frac) +
                               magnitudes[srcIdx + 1] * frac;
            } else {
                stretched[i] = magnitudes[srcIdx];
            }
        }
    }

    magnitudes.swap(stretched);
}

// =============================================================================
// 频谱移调 - 平移频率bin
// semitones: ±24, 正=升调, 负=降调
// 算法: 将每个源bin的幅度散布到目标bin (线性插值分配)
// =============================================================================
void SpectralWarper::applyShift(std::vector<float>& magnitudes, float semitones,
                                 double sampleRate, int fftSize) {
    juce::ignoreUnused(sampleRate, fftSize);
    int numBins = static_cast<int>(magnitudes.size());
    std::vector<float> shifted(numBins, 0.0f);

    float ratio = std::pow(2.0f, semitones / 12.0f);

    for (int i = 0; i < numBins; ++i) {
        float dstBin = static_cast<float>(i) * ratio;

        if (dstBin < static_cast<float>(numBins - 1)) {
            int dstIdx = static_cast<int>(dstBin);
            float frac = dstBin - static_cast<float>(dstIdx);

            if (dstIdx + 1 < numBins) {
                shifted[dstIdx] += magnitudes[i] * (1.0f - frac);
                shifted[dstIdx + 1] += magnitudes[i] * frac;
            } else {
                shifted[dstIdx] += magnitudes[i];
            }
        }
    }

    magnitudes.swap(shifted);
}

// =============================================================================
// 对称化 - 将频谱关于中心频率镜像
// amount=0: 原始频谱, amount=1: 完全对称 (左右镜像取平均)
// 算法: new_mag[i] = lerp(mag[i], (mag[i] + mag[N-1-i])/2, amount)
// =============================================================================
void SpectralWarper::applySymmetrize(std::vector<float>& magnitudes, float amount,
                                      double sampleRate, int fftSize) {
    juce::ignoreUnused(sampleRate, fftSize);
    if (amount < 0.001f) return;

    int numBins = static_cast<int>(magnitudes.size());
    std::vector<float> result(numBins, 0.0f);

    int halfBins = numBins / 2;
    for (int i = 0; i < halfBins; ++i) {
        int mirrorIdx = numBins - 1 - i;
        float avg = (magnitudes[i] + magnitudes[mirrorIdx]) * 0.5f;
        result[i] = magnitudes[i] * (1.0f - amount) + avg * amount;
        result[mirrorIdx] = magnitudes[mirrorIdx] * (1.0f - amount) + avg * amount;
    }
    // 中心bin不变
    result[halfBins] = magnitudes[halfBins];

    magnitudes.swap(result);
}

// =============================================================================
// 量化 - 将频谱幅度离散化到离散等级
// amount=0: 2级(开/关), amount=0.5: 8级, amount=1: 64级
// 算法: levels = 2 + quantizeLevels * 62
// =============================================================================
void SpectralWarper::applyQuantize(std::vector<float>& magnitudes, float amount) {
    if (amount < 0.001f) {
        // amount=0: 硬限幅(2级)
        for (float& mag : magnitudes) {
            float threshold = 0.1f;
            if (mag > threshold) {
                mag = 1.0f;
            } else {
                mag = 0.0f;
            }
        }
        return;
    }

    // 计算量化等级数: 2到64级
    int levels = 2 + static_cast<int>(amount * 62.0f);
    if (levels < 2) levels = 2;
    if (levels > 64) levels = 64;

    // 找到最大幅度用于归一化
    float maxMag = 0.0f;
    for (float mag : magnitudes) {
        if (mag > maxMag) maxMag = mag;
    }
    if (maxMag < 0.0001f) return;

    float invMax = 1.0f / maxMag;
    float step = 1.0f / static_cast<float>(levels - 1);

    for (float& mag : magnitudes) {
        float normalized = mag * invMax;
        // 量化到最近的等级
        int level = static_cast<int>(normalized / step + 0.5f);
        if (level < 0) level = 0;
        if (level > levels - 1) level = levels - 1;
        mag = static_cast<float>(level) * step * maxMag;
    }
}

// =============================================================================
// 折叠 - 将超过折叠频率的频谱能量反射回低频
// amount=0: 折叠频率=奈奎斯特(无折叠)
// amount=1: 折叠频率=0Hz(最大折叠)
// 算法: foldBin = numBins * (1.0 - amount * 0.98)
//       超过foldBin的能量被折叠: foldIdx = 2*foldBin - srcIdx
// =============================================================================
void SpectralWarper::applyFold(std::vector<float>& magnitudes, float amount,
                                double sampleRate, int fftSize) {
    juce::ignoreUnused(sampleRate, fftSize);
    if (amount < 0.001f) return;

    int numBins = static_cast<int>(magnitudes.size());

    // 计算折叠bin索引
    // amount=0: foldBin ≈ numBins (无折叠)
    // amount=1: foldBin ≈ 2 (极端折叠)
    int foldBin = static_cast<int>(numBins * (1.0f - amount * 0.98f));
    if (foldBin < 2) foldBin = 2;
    if (foldBin >= numBins) return;

    std::vector<float> folded(numBins, 0.0f);

    // 未折叠的部分保持原样
    for (int i = 0; i < foldBin; ++i) {
        folded[i] = magnitudes[i];
    }

    // 折叠部分: 将高频反射回低频
    for (int i = foldBin; i < numBins; ++i) {
        // 反射索引: foldBin - (i - foldBin) = 2*foldBin - i
        int foldIdx = 2 * foldBin - i;
        if (foldIdx < 0) {
            // 二次反射
            foldIdx = -foldIdx;
        }
        if (foldIdx < numBins) {
            folded[foldIdx] += magnitudes[i] * 0.5f; // 衰减避免过载
        }
    }

    magnitudes.swap(folded);
}

} // namespace LianCore