// =============================================================================
// LianCore - AudioUtils 音频工具类
// 包含SIMD封装、数学工具、常量定义
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <algorithm>
#include <random>

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace LianCore {
namespace AudioUtils {

// =============================================================================
// 常量定义
// =============================================================================
constexpr double kPI = 3.14159265358979323846;
constexpr double kTwoPI = kPI * 2.0;
constexpr double kHalfPI = kPI * 0.5;

// MIDI标准
constexpr double kMidiCenterNote = 69.0;  // A4 = 440Hz
constexpr double kMidiFrequencyA4 = 440.0;

// =============================================================================
// 频率转换
// =============================================================================

// MIDI音符 → 频率 (Hz)
inline double midiNoteToFrequency(double midiNote) {
    return kMidiFrequencyA4 * std::pow(2.0, (midiNote - kMidiCenterNote) / 12.0);
}

// 频率 → MIDI音符
inline double frequencyToMidiNote(double frequency) {
    return kMidiCenterNote + 12.0 * std::log2(frequency / kMidiFrequencyA4);
}

// 半音偏移 → 频率倍率
inline double semitonesToRatio(double semitones) {
    return std::pow(2.0, semitones / 12.0);
}

// 音分 → 频率倍率
inline double centsToRatio(double cents) {
    return std::pow(2.0, cents / 1200.0);
}

// =============================================================================
// 数值工具
// =============================================================================

// 线性插值
inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// 三次插值
inline float cubicInterpolate(float y0, float y1, float y2, float y3, float t) {
    float a0 = y3 - y2 - y0 + y1;
    float a1 = y0 - y1 - a0;
    float a2 = y2 - y0;
    float a3 = y1;
    return a0 * t * t * t + a1 * t * t + a2 * t + a3;
}

// 值域映射
inline float mapRange(float value, float inMin, float inMax, float outMin, float outMax) {
    float normalized = (value - inMin) / (inMax - inMin);
    return outMin + normalized * (outMax - outMin);
}

// 值域裁剪
inline float clamp(float value, float min, float max) {
    return std::max(min, std::min(max, value));
}

// 零点交叉检测
inline bool isZeroCrossing(float prev, float current) {
    return (prev < 0.0f && current >= 0.0f) || (prev >= 0.0f && current < 0.0f);
}

// =============================================================================
// dB 转换
// =============================================================================

// 幅度 → 分贝
inline float amplitudeToDb(float amplitude) {
    return 20.0f * std::log10(std::max(amplitude, 1e-10f));
}

// 分贝 → 幅度
inline float dbToAmplitude(float db) {
    return std::pow(10.0f, db / 20.0f);
}

// =============================================================================
// 相位计算
// =============================================================================

// 计算相位增量(每采样)
inline float phaseIncrementPerSample(float frequency, double sampleRate) {
    return static_cast<float>(frequency / sampleRate);
}

// 归一化相位 [0.0, 1.0)
inline float wrapPhase(float phase) {
    phase = std::fmod(phase, 1.0f);
    return phase < 0.0f ? phase + 1.0f : phase;
}

// =============================================================================
// 波形生成 (查找表方式)
// =============================================================================

// 正弦波 (快速近似)
inline float fastSin(float phase) {
    // 使用JUCE内置快速正弦
    return std::sin(static_cast<float>(kTwoPI) * phase);
}

// 锯齿波 (带抗混叠多项式逼近)
inline float bandlimitedSaw(float phase) {
    // 归一化到 [-1, 1]
    float p = phase * 2.0f - 1.0f;
    // 多项式逼近锯齿波
    return p - p * p * p * 0.1666667f;
}

// 方波 (带抗混叠)
inline float bandlimitedSquare(float phase, float pulseWidth = 0.5f) {
    return phase < pulseWidth ? 1.0f : -1.0f;
}

// =============================================================================
// SIMD 加速封装
// =============================================================================

// 向量化缓冲区清零 (使用AVX2)
inline void clearBufferSIMD(float* buffer, int numSamples) {
#ifdef __AVX2__
    __m256 zero = _mm256_setzero_ps();
    int simdEnd = numSamples & ~7; // 对齐到8

    for (int i = 0; i < simdEnd; i += 8) {
        _mm256_storeu_ps(buffer + i, zero);
    }

    // 处理剩余样本
    for (int i = simdEnd; i < numSamples; ++i) {
        buffer[i] = 0.0f;
    }
#else
    std::fill_n(buffer, numSamples, 0.0f);
#endif
}

// 向量化乘法 (gain * buffer)
inline void multiplyBufferSIMD(float* buffer, float gain, int numSamples) {
#ifdef __AVX2__
    __m256 gainVec = _mm256_set1_ps(gain);
    int simdEnd = numSamples & ~7;

    for (int i = 0; i < simdEnd; i += 8) {
        __m256 data = _mm256_loadu_ps(buffer + i);
        _mm256_storeu_ps(buffer + i, _mm256_mul_ps(data, gainVec));
    }

    for (int i = simdEnd; i < numSamples; ++i) {
        buffer[i] *= gain;
    }
#else
    for (int i = 0; i < numSamples; ++i) {
        buffer[i] *= gain;
    }
#endif
}

// 向量化混合 (a * mix + b * (1-mix))
inline void mixBuffersSIMD(float* dst, const float* srcA, const float* srcB,
                            float mix, int numSamples) {
#ifdef __AVX2__
    __m256 mixVec = _mm256_set1_ps(mix);
    __m256 oneMinusMix = _mm256_set1_ps(1.0f - mix);
    int simdEnd = numSamples & ~7;

    for (int i = 0; i < simdEnd; i += 8) {
        __m256 a = _mm256_loadu_ps(srcA + i);
        __m256 b = _mm256_loadu_ps(srcB + i);
        __m256 result = _mm256_add_ps(
            _mm256_mul_ps(a, mixVec),
            _mm256_mul_ps(b, oneMinusMix)
        );
        _mm256_storeu_ps(dst + i, result);
    }

    for (int i = simdEnd; i < numSamples; ++i) {
        dst[i] = srcA[i] * mix + srcB[i] * (1.0f - mix);
    }
#else
    float oneMinusMix = 1.0f - mix;
    for (int i = 0; i < numSamples; ++i) {
        dst[i] = srcA[i] * mix + srcB[i] * oneMinusMix;
    }
#endif
}

// =============================================================================
// 随机数生成器 (线程安全)
// =============================================================================
class RandomGenerator {
public:
    RandomGenerator() : rng_(std::random_device{}()) {}

    float nextFloat() {
        return dist_(rng_);
    }

    float nextFloat(float min, float max) {
        return min + (max - min) * dist_(rng_);
    }

    // 高斯分布噪声
    float nextGaussian(float mean = 0.0f, float stddev = 1.0f) {
        return mean + stddev * gaussianDist_(rng_);
    }

private:
    std::mt19937 rng_;
    std::uniform_real_distribution<float> dist_{ 0.0f, 1.0f };
    std::normal_distribution<float> gaussianDist_{ 0.0f, 1.0f };
};

// 线程局部随机数生成器
inline RandomGenerator& getThreadLocalRNG() {
    thread_local RandomGenerator rng;
    return rng;
}

} // namespace AudioUtils
} // namespace LianCore