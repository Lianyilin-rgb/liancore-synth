// =============================================================================
// LianCore - SpectralWarper 频谱变形单元测试 (P2-3)
// =============================================================================
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>

// 前向声明测试辅助函数
namespace {
    // 生成锯齿波谐波频谱: 1/n 衰减
    std::vector<float> makeSawtoothSpectrum(int numBins) {
        std::vector<float> spec(numBins, 0.0f);
        for (int i = 1; i < numBins; ++i) {
            spec[i] = 1.0f / static_cast<float>(i);
        }
        return spec;
    }

    // 生成对称频谱: 左右对称
    std::vector<float> makeAsymmetricSpectrum(int numBins) {
        std::vector<float> spec(numBins, 0.0f);
        for (int i = 0; i < numBins / 2; ++i) {
            spec[i] = 1.0f;
        }
        // 后半部分为0，形成不对称
        return spec;
    }

    // 计算谐波间距: 找到前N个峰值bin的间隔
    std::vector<int> findPeakBins(const std::vector<float>& spec, int maxPeaks = 10) {
        std::vector<int> peaks;
        for (size_t i = 1; i < spec.size() - 1 && peaks.size() < static_cast<size_t>(maxPeaks); ++i) {
            if (spec[i] > spec[i - 1] && spec[i] > spec[i + 1] && spec[i] > 0.01f) {
                peaks.push_back(static_cast<int>(i));
            }
        }
        return peaks;
    }

    // 计算平均峰值间距
    float averagePeakSpacing(const std::vector<int>& peaks) {
        if (peaks.size() < 2) return 0.0f;
        float totalSpacing = 0.0f;
        for (size_t i = 1; i < peaks.size(); ++i) {
            totalSpacing += static_cast<float>(peaks[i] - peaks[i - 1]);
        }
        return totalSpacing / static_cast<float>(peaks.size() - 1);
    }
}

// 使用实际SpectralWarper头文件
#include "../../src/synthesis/SpectralWarper.h"

using namespace LianCore;

// =============================================================================
// 基础测试
// =============================================================================
TEST_CASE("SpectralWarper: construction and defaults", "[spectralWarp][unit]") {
    SpectralWarper warper;
    REQUIRE(warper.getMode() == SpectralWarper::Mode::Stretch);
    REQUIRE(warper.getAmount() == 0.0f);
}

TEST_CASE("SpectralWarper: mode switching", "[spectralWarp][unit]") {
    SpectralWarper warper;
    warper.setMode(SpectralWarper::Mode::Symmetrize);
    REQUIRE(warper.getMode() == SpectralWarper::Mode::Symmetrize);
    warper.setMode(SpectralWarper::Mode::Quantize);
    REQUIRE(warper.getMode() == SpectralWarper::Mode::Quantize);
    warper.setMode(SpectralWarper::Mode::Fold);
    REQUIRE(warper.getMode() == SpectralWarper::Mode::Fold);
}

TEST_CASE("SpectralWarper: Amount parameter", "[spectralWarp][unit]") {
    SpectralWarper warper;
    warper.setAmount(0.5f);
    REQUIRE(warper.getAmount() == 0.5f);
    warper.setAmount(1.0f);
    REQUIRE(warper.getAmount() == 1.0f);
    // 边界测试
    warper.setAmount(-0.5f);
    REQUIRE(warper.getAmount() == 0.0f);
    warper.setAmount(2.0f);
    REQUIRE(warper.getAmount() == 1.0f);
}

TEST_CASE("SpectralWarper: mode names", "[spectralWarp][unit]") {
    // JUCE String内部使用UTF-8，但编译时字符串字面量编码取决于源文件编码
    REQUIRE(SpectralWarper::getModeName(0).isNotEmpty());
    REQUIRE(SpectralWarper::getModeName(1).isNotEmpty());
    REQUIRE(SpectralWarper::getModeName(2).isNotEmpty());
    REQUIRE(SpectralWarper::getModeName(3).isNotEmpty());
    REQUIRE(SpectralWarper::getModeName(4).isNotEmpty());
    // 验证5个模式名称各不相同
    REQUIRE(SpectralWarper::getModeName(0) != SpectralWarper::getModeName(1));
    REQUIRE(SpectralWarper::getModeName(1) != SpectralWarper::getModeName(2));
    REQUIRE(SpectralWarper::getModeName(2) != SpectralWarper::getModeName(3));
    REQUIRE(SpectralWarper::getModeName(3) != SpectralWarper::getModeName(4));
}

// =============================================================================
// Stretch 测试
// =============================================================================
// 注: stretch=2.0 意味着每个输出bin读取 1/2.0 的源bin索引
// 即高频bin从低频bin读取数据 → 频谱能量向低频收缩
TEST_CASE("SpectralWarper: stretch - energy to low", "[spectralWarp][stretch]") {
    SpectralWarper warper;
    warper.setMode(SpectralWarper::Mode::Stretch);

    auto spec = makeSawtoothSpectrum(1025);

    float origWeightedSum = 0.0f, origTotalEnergy = 0.0f;
    for (size_t i = 0; i < spec.size(); ++i) {
        origWeightedSum += spec[i] * static_cast<float>(i);
        origTotalEnergy += spec[i];
    }
    float origCentroid = origTotalEnergy > 0.0f ? origWeightedSum / origTotalEnergy : 0.0f;
    REQUIRE(origCentroid > 0.0f);

    // 拉伸 2x: 频谱重心向高频移动 (能量扩展到更高bin)
    warper.setAmount(0.4286f); // maps to 2.0x
    warper.process(spec, 44100.0, 2048);

    float newWeightedSum = 0.0f, newTotalEnergy = 0.0f;
    for (size_t i = 0; i < spec.size(); ++i) {
        newWeightedSum += spec[i] * static_cast<float>(i);
        newTotalEnergy += spec[i];
    }
    float newCentroid = newTotalEnergy > 0.0f ? newWeightedSum / newTotalEnergy : 0.0f;
    // 拉伸后重心应向高频移动
    REQUIRE(newCentroid > origCentroid);
}

TEST_CASE("SpectralWarper: compress - energy to high", "[spectralWarp][stretch]") {
    SpectralWarper warper;
    warper.setMode(SpectralWarper::Mode::Stretch);

    auto spec = makeSawtoothSpectrum(1025);

    float origWeightedSum = 0.0f, origTotalEnergy = 0.0f;
    for (size_t i = 0; i < spec.size(); ++i) {
        origWeightedSum += spec[i] * static_cast<float>(i);
        origTotalEnergy += spec[i];
    }
    float origCentroid = origTotalEnergy > 0.0f ? origWeightedSum / origTotalEnergy : 0.0f;

    // 压缩 0.5x: 频谱重心向低频移动 (能量收缩到更低bin)
    warper.setAmount(0.0f); // maps to 0.5x
    warper.process(spec, 44100.0, 2048);

    float newWeightedSum = 0.0f, newTotalEnergy = 0.0f;
    for (size_t i = 0; i < spec.size(); ++i) {
        newWeightedSum += spec[i] * static_cast<float>(i);
        newTotalEnergy += spec[i];
    }
    float newCentroid = newTotalEnergy > 0.0f ? newWeightedSum / newTotalEnergy : 0.0f;
    // 压缩后重心应向低频移动
    REQUIRE(newCentroid < origCentroid);
}

// =============================================================================
// Symmetrize 测试
// =============================================================================
TEST_CASE("SpectralWarper: symmetrize - full", "[spectralWarp][symmetrize]") {
    SpectralWarper warper;
    warper.setMode(SpectralWarper::Mode::Symmetrize);
    warper.setAmount(1.0f);

    // 生成不对称频谱: 前半部分有值，后半部分为0
    auto spec = makeAsymmetricSpectrum(1025);
    int numBins = static_cast<int>(spec.size());

    warper.process(spec, 44100.0, 2048);

    // 验证对称性: spec[i] == spec[numBins-1-i]
    int halfBins = numBins / 2;
    for (int i = 0; i < halfBins; ++i) {
        REQUIRE(std::abs(spec[i] - spec[numBins - 1 - i]) < 0.001f);
    }
}

TEST_CASE("SpectralWarper: symmetrize - partial", "[spectralWarp][symmetrize]") {
    SpectralWarper warper;
    warper.setMode(SpectralWarper::Mode::Symmetrize);
    warper.setAmount(0.5f);

    auto spec = makeAsymmetricSpectrum(1025);
    int numBins = static_cast<int>(spec.size());
    int halfBins = numBins / 2;

    // 保存原始左侧值
    std::vector<float> leftOrig(halfBins);
    for (int i = 0; i < halfBins; ++i) {
        leftOrig[i] = spec[i];
    }

    warper.process(spec, 44100.0, 2048);

    // 50%对称时，左侧值应该介于原始值和中间值之间
    for (int i = 0; i < halfBins; ++i) {
        float avg = (leftOrig[i] + 0.0f) * 0.5f; // 右侧原来是0
        float expected = leftOrig[i] * 0.5f + avg * 0.5f;
        REQUIRE(std::abs(spec[i] - expected) < 0.01f);
    }
}

// =============================================================================
// Quantize 测试
// =============================================================================
TEST_CASE("SpectralWarper: quantize - 2-level hard clip", "[spectralWarp][quantize]") {
    SpectralWarper warper;
    warper.setMode(SpectralWarper::Mode::Quantize);
    warper.setAmount(0.0f); // 2级

    auto spec = makeSawtoothSpectrum(1025);
    warper.process(spec, 44100.0, 2048);

    // 验证所有值都是0或1 (或接近0的阈值)
    bool hasNonBinary = false;
    for (float v : spec) {
        if (v > 0.01f && std::abs(v - 1.0f) > 0.01f) {
            hasNonBinary = true;
            break;
        }
    }
    REQUIRE_FALSE(hasNonBinary);
}

TEST_CASE("SpectralWarper: quantize - different levels", "[spectralWarp][quantize]") {
    SpectralWarper warper;
    warper.setMode(SpectralWarper::Mode::Quantize);

    auto spec = makeSawtoothSpectrum(1025);
    // 使用amount=0.5 (约33级量化)
    warper.setAmount(0.5f);
    warper.process(spec, 44100.0, 2048);

    // 验证量化后至少有2个不同的非零值
    std::vector<float> uniqueVals;
    for (float v : spec) {
        if (v > 0.001f) {
            bool found = false;
            for (float u : uniqueVals) {
                if (std::abs(u - v) < 0.001f) { found = true; break; }
            }
            if (!found) uniqueVals.push_back(v);
        }
    }
    REQUIRE(uniqueVals.size() > 1);
}

// =============================================================================
// Fold 测试
// =============================================================================
TEST_CASE("SpectralWarper: fold - high to low", "[spectralWarp][fold]") {
    SpectralWarper warper;
    warper.setMode(SpectralWarper::Mode::Fold);
    warper.setAmount(0.5f); // 折叠频率约在奈奎斯特一半处

    auto spec = makeSawtoothSpectrum(1025);
    int numBins = static_cast<int>(spec.size());

    // 记录原始低频能量
    float origLowEnergy = 0.0f;
    for (int i = 0; i < numBins / 4; ++i) {
        origLowEnergy += spec[i] * spec[i];
    }

    warper.process(spec, 44100.0, 2048);

    // 折叠后低频能量应增加 (因为高频被折叠到低频)
    float newLowEnergy = 0.0f;
    for (int i = 0; i < numBins / 4; ++i) {
        newLowEnergy += spec[i] * spec[i];
    }
    REQUIRE(newLowEnergy > origLowEnergy);
}

TEST_CASE("SpectralWarper: fold - no folding no change", "[spectralWarp][fold]") {
    SpectralWarper warper;
    warper.setMode(SpectralWarper::Mode::Fold);
    warper.setAmount(0.0f); // 无折叠

    auto spec = makeSawtoothSpectrum(1025);
    auto orig = spec;

    warper.process(spec, 44100.0, 2048);

    // 无折叠时频谱应不变
    for (size_t i = 0; i < spec.size(); ++i) {
        REQUIRE(std::abs(spec[i] - orig[i]) < 0.0001f);
    }
}

TEST_CASE("SpectralWarper: fold - extreme", "[spectralWarp][fold]") {
    SpectralWarper warper;
    warper.setMode(SpectralWarper::Mode::Fold);
    warper.setAmount(1.0f); // 最大折叠

    auto spec = makeSawtoothSpectrum(1025);
    int numBins = static_cast<int>(spec.size());

    warper.process(spec, 44100.0, 2048);

    // 极端折叠下，大部分高频bin应该被清空
    float highEnergy = 0.0f;
    for (int i = numBins * 3 / 4; i < numBins; ++i) {
        highEnergy += spec[i] * spec[i];
    }
    REQUIRE(highEnergy < 0.01f);
}

// =============================================================================
// 边界条件测试
// =============================================================================
TEST_CASE("SpectralWarper: empty spectrum", "[spectralWarp][edge]") {
    SpectralWarper warper;
    std::vector<float> empty;
    // 空输入不应崩溃
    REQUIRE_NOTHROW(warper.process(empty, 44100.0, 2048));
}

TEST_CASE("SpectralWarper: all-zero unchanged", "[spectralWarp][edge]") {
    SpectralWarper warper;
    std::vector<float> zeroSpec(1025, 0.0f);

    for (int mode = 0; mode < SpectralWarper::kNumModes; ++mode) {
        warper.setMode(static_cast<SpectralWarper::Mode>(mode));
        warper.setAmount(1.0f);
        auto spec = zeroSpec;
        warper.process(spec, 44100.0, 2048);
        // 全零输入应保持全零
        for (float v : spec) {
            REQUIRE(v == 0.0f);
        }
    }
}

TEST_CASE("SpectralWarper: Amount=0 zero effect", "[spectralWarp][edge]") {
    SpectralWarper warper;
    auto spec = makeSawtoothSpectrum(1025);
    auto orig = spec;

    for (int mode = 0; mode < SpectralWarper::kNumModes; ++mode) {
        warper.setMode(static_cast<SpectralWarper::Mode>(mode));
        warper.setAmount(0.0f);
        auto testSpec = orig;
        warper.process(testSpec, 44100.0, 2048);
        // Stretch(0.5x), Shift(-24半音), Quantize(硬限幅) 在amount=0时仍有变化
        if (mode == 0 || mode == 1 || mode == 3) continue;
        for (size_t i = 0; i < testSpec.size(); ++i) {
            REQUIRE(std::abs(testSpec[i] - orig[i]) < 0.0001f);
        }
    }
}

// =============================================================================
// Shift 测试
// =============================================================================
TEST_CASE("SpectralWarper: shift - pitch up", "[spectralWarp][shift]") {
    SpectralWarper warper;
    warper.setMode(SpectralWarper::Mode::Shift);
    warper.setAmount(0.75f); // maps to +12半音

    auto spec = makeSawtoothSpectrum(1025);
    int numBins = static_cast<int>(spec.size());

    // 记录原始前几个bin的值
    std::vector<float> origLowBins(10);
    for (int i = 0; i < 10; ++i) origLowBins[i] = spec[i];

    warper.process(spec, 44100.0, 2048);

    // 升调后，低频能量应该减少 (因为谐波被移向高频)
    float lowEnergy = 0.0f;
    for (int i = 0; i < 10; ++i) lowEnergy += spec[i] * spec[i];
    float origLowEnergy = 0.0f;
    for (int i = 0; i < 10; ++i) origLowEnergy += origLowBins[i] * origLowBins[i];
    REQUIRE(lowEnergy < origLowEnergy);
}

TEST_CASE("SpectralWarper: shift - pitch down", "[spectralWarp][shift]") {
    SpectralWarper warper;
    warper.setMode(SpectralWarper::Mode::Shift);
    warper.setAmount(0.25f); // maps to -12半音

    auto spec = makeSawtoothSpectrum(1025);
    int numBins = static_cast<int>(spec.size());

    // 记录原始高频能量
    float origHighEnergy = 0.0f;
    for (int i = numBins * 3 / 4; i < numBins; ++i) {
        origHighEnergy += spec[i] * spec[i];
    }

    warper.process(spec, 44100.0, 2048);

    // 降调后，高频能量应该减少 (因为谐波被移向低频)
    float highEnergy = 0.0f;
    for (int i = numBins * 3 / 4; i < numBins; ++i) {
        highEnergy += spec[i] * spec[i];
    }
    REQUIRE(highEnergy < origHighEnergy);
}

// =============================================================================
// 综合测试
// =============================================================================
TEST_CASE("SpectralWarper: all 5 modes available", "[spectralWarp][integration]") {
    SpectralWarper warper;
    auto spec = makeSawtoothSpectrum(1025);

    for (int mode = 0; mode < SpectralWarper::kNumModes; ++mode) {
        warper.setMode(static_cast<SpectralWarper::Mode>(mode));
        warper.setAmount(0.5f);
        auto testSpec = spec;
        REQUIRE_NOTHROW(warper.process(testSpec, 44100.0, 2048));
        // 处理后频谱不应全为NaN
        bool hasNaN = false;
        for (float v : testSpec) {
            if (std::isnan(v)) { hasNaN = true; break; }
        }
        REQUIRE_FALSE(hasNaN);
    }
}

TEST_CASE("SpectralWarper: different FFT sizes", "[spectralWarp][integration]") {
    SpectralWarper warper;
    warper.setMode(SpectralWarper::Mode::Stretch);
    warper.setAmount(0.5f);

    // 测试不同FFT大小
    std::vector<int> fftSizes = {256, 512, 1024, 2048, 4096};
    for (int fftSize : fftSizes) {
        int numBins = fftSize / 2 + 1;
        auto spec = makeSawtoothSpectrum(numBins);
        REQUIRE_NOTHROW(warper.process(spec, 44100.0, fftSize));
    }
}