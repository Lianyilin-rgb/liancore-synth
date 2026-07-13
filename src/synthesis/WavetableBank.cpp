// =============================================================================
// LianCore - WavetableBank 实现
// =============================================================================
#include "WavetableBank.h"
#include "AudioUtils.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include <algorithm>

namespace LianCore {

WavetableBank::WavetableBank() {
    wavetableData_.setSize(1, kTotalSamples);
    wavetableData_.clear();
}

// =============================================================================
// 数据访问
// =============================================================================
const float* WavetableBank::getFrameData(int frameIndex) const {
    jassert(frameIndex >= 0 && frameIndex < numFrames_);
    return wavetableData_.getReadPointer(0) + frameIndex * kFrameSize;
}

float* WavetableBank::getFrameData(int frameIndex) {
    jassert(frameIndex >= 0 && frameIndex < numFrames_);
    return wavetableData_.getWritePointer(0) + frameIndex * kFrameSize;
}

void WavetableBank::getInterpolatedFrame(float framePosition, float* output, int numSamples) {
    if (numFrames_ == 0) {
        std::fill_n(output, numSamples, 0.0f);
        return;
    }

    float clampedPos = AudioUtils::clamp(framePosition, 0.0f, static_cast<float>(numFrames_ - 1));
    int frameA = static_cast<int>(clampedPos);
    int frameB = std::min(frameA + 1, numFrames_ - 1);
    float frac = clampedPos - static_cast<float>(frameA);

    const float* dataA = getFrameData(frameA);
    const float* dataB = getFrameData(frameB);

    // 三次插值需要4个点
    int frameA2 = std::max(frameA - 1, 0);
    int frameB2 = std::min(frameB + 1, numFrames_ - 1);
    const float* dataA2 = getFrameData(frameA2);
    const float* dataB2 = getFrameData(frameB2);

    for (int i = 0; i < numSamples; ++i) {
        // 在帧内进行三次插值
        float sampleIdx = static_cast<float>(i) / numSamples * kFrameSize;
        int idx = static_cast<int>(sampleIdx);
        float t = sampleIdx - idx;

        // 帧间线性插值 + 帧内三次插值
        float vA = AudioUtils::cubicInterpolate(
            dataA2[(idx - 1 + kFrameSize) % kFrameSize],
            dataA[idx % kFrameSize],
            dataA[(idx + 1) % kFrameSize],
            dataA[(idx + 2) % kFrameSize],
            t
        );
        float vB = AudioUtils::cubicInterpolate(
            dataB2[(idx - 1 + kFrameSize) % kFrameSize],
            dataB[idx % kFrameSize],
            dataB[(idx + 1) % kFrameSize],
            dataB[(idx + 2) % kFrameSize],
            t
        );

        output[i] = AudioUtils::lerp(vA, vB, frac);
    }
}

// =============================================================================
// 波表管理
// =============================================================================
void WavetableBank::clear() {
    numFrames_ = 0;
    wavetableData_.clear();
}

void WavetableBank::setNumFrames(int numFrames) {
    numFrames_ = std::min(numFrames, kMaxFrames);
}

void WavetableBank::setData(const float* data, int numFrames) {
    numFrames_ = std::min(numFrames, kMaxFrames);
    int samples = numFrames_ * kFrameSize;
    std::copy_n(data, samples, wavetableData_.getWritePointer(0));
}

void WavetableBank::setFrameData(int frameIndex, const float* data, int size) {
    jassert(frameIndex < kMaxFrames);
    if (frameIndex >= numFrames_) {
        numFrames_ = frameIndex + 1;
    }
    int copySize = std::min(size, kFrameSize);
    std::copy_n(data, copySize, getFrameData(frameIndex));
    normalizeFrame(frameIndex);
}

// =============================================================================
// 文件 I/O
// =============================================================================
bool WavetableBank::loadFromWavFile(const juce::File& file) {
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(file));

    if (!reader) return false;

    // 读取整个文件
    juce::AudioBuffer<float> fileBuffer;
    fileBuffer.setSize(1, static_cast<int>(reader->lengthInSamples));
    reader->read(&fileBuffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

    // 计算帧数
    int totalSamples = static_cast<int>(reader->lengthInSamples);
    numFrames_ = std::min(totalSamples / kFrameSize, kMaxFrames);

    if (numFrames_ == 0) return false;

    // 复制到波表数据
    wavetableData_.copyFrom(0, 0, fileBuffer, 0, 0, numFrames_ * kFrameSize);

    // 归一化每帧
    for (int i = 0; i < numFrames_; ++i) {
        normalizeFrame(i);
    }

    return true;
}

bool WavetableBank::saveToWavFile(const juce::File& file) const {
    if (numFrames_ == 0) return false;

    juce::WavAudioFormat wavFormat;
    auto fos = file.createOutputStream();
    if (!fos) return false;

    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(fos.get(), 44100.0, 1, 16, {}, 0));

    if (!writer) {
        return false;
    }

    // 写入所有帧数据
    writer->writeFromAudioSampleBuffer(wavetableData_, 0, numFrames_ * kFrameSize);

    return true;
}

// =============================================================================
// 波形生成
// =============================================================================
void WavetableBank::generateSineWave(int numHarmonics) {
    numFrames_ = kMaxFrames;
    auto* data = wavetableData_.getWritePointer(0);
    std::fill_n(data, kTotalSamples, 0.0f);

    for (int i = 0; i < kFrameSize; ++i) {
        float phase = static_cast<float>(i) / kFrameSize;
        float sample = 0.0f;
        for (int h = 0; h < numHarmonics; ++h) {
            sample += std::sin(static_cast<float>(AudioUtils::kTwoPI) * phase * (h + 1)) / (h + 1);
        }
        data[i] = sample;
    }

    normalizeFrame(0);

    // 其余帧为衰减版本
    for (int f = 1; f < numFrames_; ++f) {
        float filterAmount = 1.0f - static_cast<float>(f) / numFrames_;
        auto* frameData = getFrameData(f);
        auto* frameData0 = getFrameData(0);
        for (int i = 0; i < kFrameSize; ++i) {
            // 简单低通滤波模拟
            frameData[i] = frameData0[i] * filterAmount * filterAmount;
        }
        normalizeFrame(f);
    }
}

void WavetableBank::generateSawWave(int numHarmonics) {
    numFrames_ = kMaxFrames;
    auto* data = wavetableData_.getWritePointer(0);

    for (int i = 0; i < kFrameSize; ++i) {
        float phase = static_cast<float>(i) / kFrameSize;
        float sample = 0.0f;
        // 锯齿波: 所有谐波，奇偶交替符号
        for (int h = 1; h <= numHarmonics; ++h) {
            float sign = (h % 2 == 0) ? -1.0f : 1.0f;
            sample += sign * std::sin(static_cast<float>(AudioUtils::kTwoPI) * phase * h) / h;
        }
        data[i] = sample;
    }

    normalizeFrame(0);

    // 其余帧为不同谐波含量的版本
    for (int f = 1; f < numFrames_; ++f) {
        float harmonicsFraction = 1.0f - static_cast<float>(f) / numFrames_;
        int activeHarmonics = std::max(1, static_cast<int>(numHarmonics * harmonicsFraction));
        auto* frameData = getFrameData(f);
        for (int i = 0; i < kFrameSize; ++i) {
            float phase = static_cast<float>(i) / kFrameSize;
            float sample = 0.0f;
            for (int h = 1; h <= activeHarmonics; ++h) {
                float sign = (h % 2 == 0) ? -1.0f : 1.0f;
                sample += sign * std::sin(static_cast<float>(AudioUtils::kTwoPI) * phase * h) / h;
            }
            frameData[i] = sample;
        }
        normalizeFrame(f);
    }
}

void WavetableBank::generateSquareWave(int numHarmonics) {
    numFrames_ = kMaxFrames;
    auto* data = wavetableData_.getWritePointer(0);

    for (int i = 0; i < kFrameSize; ++i) {
        float phase = static_cast<float>(i) / kFrameSize;
        float sample = 0.0f;
        // 方波: 仅奇次谐波
        for (int h = 1; h <= numHarmonics; h += 2) {
            sample += std::sin(static_cast<float>(AudioUtils::kTwoPI) * phase * h) / h;
        }
        data[i] = sample;
    }

    normalizeFrame(0);

    for (int f = 1; f < numFrames_; ++f) {
        float pw = 0.5f - 0.4f * static_cast<float>(f) / numFrames_;
        auto* frameData = getFrameData(f);
        for (int i = 0; i < kFrameSize; ++i) {
            float phase = static_cast<float>(i) / kFrameSize;
            frameData[i] = phase < pw ? 1.0f : -1.0f;
        }
        normalizeFrame(f);
    }
}

void WavetableBank::generateTriangleWave(int numHarmonics) {
    numFrames_ = kMaxFrames;
    auto* data = wavetableData_.getWritePointer(0);

    for (int i = 0; i < kFrameSize; ++i) {
        float phase = static_cast<float>(i) / kFrameSize;
        float sample = 0.0f;
        // 三角波: 奇次谐波，交替符号，平方分母
        for (int h = 1; h <= numHarmonics; h += 2) {
            float sign = ((h / 2) % 2 == 0) ? 1.0f : -1.0f;
            sample += sign * std::sin(static_cast<float>(AudioUtils::kTwoPI) * phase * h) / (h * h);
        }
        data[i] = sample;
    }

    normalizeFrame(0);

    for (int f = 1; f < numFrames_; ++f) {
        float harmonicsFraction = 1.0f - static_cast<float>(f) / numFrames_;
        int activeHarmonics = std::max(1, static_cast<int>(numHarmonics * harmonicsFraction));
        auto* frameData = getFrameData(f);
        for (int i = 0; i < kFrameSize; ++i) {
            float phase = static_cast<float>(i) / kFrameSize;
            float sample = 0.0f;
            for (int h = 1; h <= activeHarmonics; h += 2) {
                float sign = ((h / 2) % 2 == 0) ? 1.0f : -1.0f;
                sample += sign * std::sin(static_cast<float>(AudioUtils::kTwoPI) * phase * h) / (h * h);
            }
            frameData[i] = sample;
        }
        normalizeFrame(f);
    }
}

void WavetableBank::generateFromHarmonics(const std::vector<std::vector<float>>& harmonicAmplitudes) {
    numFrames_ = std::min(static_cast<int>(harmonicAmplitudes.size()), kMaxFrames);

    for (int f = 0; f < numFrames_; ++f) {
        auto* frameData = getFrameData(f);
        const auto& harmonics = harmonicAmplitudes[f];

        for (int i = 0; i < kFrameSize; ++i) {
            float phase = static_cast<float>(i) / kFrameSize;
            float sample = 0.0f;
            for (size_t h = 0; h < harmonics.size(); ++h) {
                if (harmonics[h] > 0.001f) {
                    sample += harmonics[h] * std::sin(
                        static_cast<float>(AudioUtils::kTwoPI) * phase * static_cast<float>(h + 1));
                }
            }
            frameData[i] = sample;
        }
        normalizeFrame(f);
    }
}

void WavetableBank::generateFromAIParams(const std::vector<float>& aiParams) {
    // AI参数映射:
    // params[0] = 基础波形类型 (0=sine, 0.25=triangle, 0.5=saw, 0.75=square)
    // params[1] = 谐波丰富度
    // params[2] = 偶数谐波量
    // params[3] = 奇数谐波量
    // params[4] = 高频衰减
    // params[5..] = 频谱形态参数

    if (aiParams.size() < 5) {
        generateSineWave(1);
        return;
    }

    numFrames_ = kMaxFrames;
    float baseWaveType = aiParams[0];
    float harmonicRichness = aiParams[1];
    float evenHarmonics = aiParams[2];
    float oddHarmonics = aiParams[3];
    float highFreqDecay = aiParams[4];

    for (int f = 0; f < numFrames_; ++f) {
        float frameParam = static_cast<float>(f) / numFrames_;
        auto* frameData = getFrameData(f);

        for (int i = 0; i < kFrameSize; ++i) {
            float phase = static_cast<float>(i) / kFrameSize;
            float sample = 0.0f;

            int numHarmonics = static_cast<int>(64.0f * harmonicRichness * (1.0f - frameParam * 0.5f));
            numHarmonics = std::max(1, numHarmonics);

            for (int h = 1; h <= numHarmonics; ++h) {
                float amplitude = 1.0f / h;
                amplitude *= std::exp(-static_cast<float>(h) * highFreqDecay / numHarmonics);

                if (h % 2 == 0) {
                    amplitude *= evenHarmonics;
                } else {
                    amplitude *= oddHarmonics;
                }

                // 基础波形类型混合
                float sign = 1.0f;
                if (baseWaveType > 0.5f && h % 2 == 0) sign = -1.0f; // 锯齿波特征
                if (baseWaveType > 0.75f && h % 2 == 0) amplitude = 0.0f; // 方波特征(仅奇数)

                sample += sign * amplitude * std::sin(
                    static_cast<float>(AudioUtils::kTwoPI) * phase * h);
            }

            frameData[i] = sample;
        }
        normalizeFrame(f);
    }
}

// =============================================================================
// 归一化
// =============================================================================
void WavetableBank::normalizeFrame(int frameIndex) {
    auto* data = getFrameData(frameIndex);
    float maxAbs = 0.0f;
    for (int i = 0; i < kFrameSize; ++i) {
        maxAbs = std::max(maxAbs, std::abs(data[i]));
    }
    if (maxAbs > 1e-10f) {
        float scale = 1.0f / maxAbs;
        for (int i = 0; i < kFrameSize; ++i) {
            data[i] *= scale;
        }
    }
}

// =============================================================================
// 信息
// =============================================================================
juce::String WavetableBank::getDescription() const {
    return juce::String::formatted("波表: %d帧 × %d采样 (%d kB)",
        numFrames_, kFrameSize,
        static_cast<int>(getMemoryUsage() / 1024));
}

size_t WavetableBank::getMemoryUsage() const {
    return numFrames_ * kFrameSize * sizeof(float);
}

} // namespace LianCore