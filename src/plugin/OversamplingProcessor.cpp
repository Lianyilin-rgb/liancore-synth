// =============================================================================
// LianCore - OversamplingProcessor 实现 (P2-4)
// =============================================================================
#include "OversamplingProcessor.h"
#include <cmath>
#include <algorithm>

namespace LianCore {

// =============================================================================
// IEEE 754 零阶修正贝塞尔函数 I0(x) (Kaiser窗设计用)
// =============================================================================
static double besselI0(double x) {
    double sum = 1.0;
    double term = 1.0;
    double x2 = x * x * 0.25;

    for (int k = 1; k < 50; ++k) {
        term *= x2 / (k * k);
        sum += term;
        if (term < sum * 1e-15) break;
    }
    return sum;
}

// =============================================================================
OversamplingProcessor::OversamplingProcessor() {
}

OversamplingProcessor::~OversamplingProcessor() = default;

// =============================================================================
void OversamplingProcessor::prepare(double sampleRate, int samplesPerBlock, int numChannels) {
    baseSampleRate_ = sampleRate;
    oversampledRate_ = sampleRate * oversamplingFactor_;
    numChannels_ = numChannels;
    maxBlockSize_ = samplesPerBlock;

    // 设计抗混叠 FIR 滤波器
    designAntiAliasingFilter();

    // 分配内部缓冲区
    int upsampledSize = samplesPerBlock * oversamplingFactor_;
    upsampledBuffers_.clear();
    for (int ch = 0; ch < numChannels; ++ch) {
        upsampledBuffers_.emplace_back(1, upsampledSize);
    }
    tempBuffer_.setSize(numChannels, upsampledSize);

    // 初始化 FIR 状态 (延迟线)
    // 每个通道需要 firOrder+1 个状态元素 (索引 0..firOrder)
    int firOrder = static_cast<int>(firCoefficients_.size()) - 1;
    int stateSize = numChannels_ * (firOrder + 1);
    firUpsampleState_.assign(stateSize, 0.0f);
    firDownsampleState_.assign(stateSize, 0.0f);

    reset();
}

void OversamplingProcessor::reset() {
    std::fill(firUpsampleState_.begin(), firUpsampleState_.end(), 0.0f);
    std::fill(firDownsampleState_.begin(), firDownsampleState_.end(), 0.0f);
    for (auto& buf : upsampledBuffers_) {
        buf.clear();
    }
    tempBuffer_.clear();
}

// =============================================================================
// Kaiser 窗线性相位 FIR 低通滤波器设计
// 截止频率: 0.22 * Nyquist (在过采样率下)
// 即: 0.22 * oversampledRate/2 = 0.11 * oversampledRate
// 对应到原始采样率: 0.11 * 4 * baseRate = 0.44 * baseRate (安全低于 0.5)
// 阶数: 128 (高精度离线渲染)
// =============================================================================
void OversamplingProcessor::designAntiAliasingFilter() {
    int firOrder = 128;  // 奇数阶 → 偶数抽头 (Type I 线性相位)
    int numTaps = firOrder + 1;

    firCoefficients_.resize(numTaps, 0.0f);

    double cutoff = 0.22;  // 归一化截止频率 (相对于过采样率)
    double beta = 8.0;     // Kaiser窗参数 (高衰减)

    // 计算 Kaiser 窗
    double i0beta = besselI0(beta);
    std::vector<double> window(numTaps);
    for (int n = 0; n < numTaps; ++n) {
        double x = 2.0 * n / firOrder - 1.0;  // [-1, 1]
        double arg = beta * std::sqrt(1.0 - x * x);
        window[n] = besselI0(arg) / i0beta;
    }

    // 设计理想低通滤波器 + 加窗
    int center = firOrder / 2;
    for (int n = 0; n < numTaps; ++n) {
        int k = n - center;
        if (k == 0) {
            firCoefficients_[n] = static_cast<float>(2.0 * cutoff);
        } else {
            firCoefficients_[n] = static_cast<float>(
                std::sin(2.0 * juce::MathConstants<double>::pi * cutoff * k) /
                (juce::MathConstants<double>::pi * k));
        }
        firCoefficients_[n] *= static_cast<float>(window[n]);
    }

    // 归一化: 使 DC 增益为 1
    float dcGain = 0.0f;
    for (float c : firCoefficients_) {
        dcGain += c;
    }
    if (std::abs(dcGain) > 1e-10f) {
        float invGain = 1.0f / dcGain;
        for (float& c : firCoefficients_) {
            c *= invGain;
        }
    }
}

// =============================================================================
// 上采样: 插入零 → FIR 插值滤波
// 简化版: 零阶保持插值 + FIR 滤波
// =============================================================================
void OversamplingProcessor::upsample(juce::AudioBuffer<float>& buffer) {
    int inSamples = buffer.getNumSamples();
    int outSamples = inSamples * oversamplingFactor_;

    for (int ch = 0; ch < numChannels_; ++ch) {
        auto& upBuf = upsampledBuffers_[ch];
        upBuf.setSize(1, outSamples, false, false, true);
        upBuf.clear();

        const float* inData = buffer.getReadPointer(ch);

        // 零阶保持插值: 每个输入采样重复4次
        for (int i = 0; i < inSamples; ++i) {
            float val = inData[i];
            for (int j = 0; j < oversamplingFactor_; ++j) {
                upBuf.setSample(0, i * oversamplingFactor_ + j, val);
            }
        }
    }

    // FIR 低通滤波 (平滑零阶保持引入的镜像)
    int firOrder = static_cast<int>(firCoefficients_.size()) - 1;
    int outSamplesTotal = outSamples;

    for (int ch = 0; ch < numChannels_; ++ch) {
        auto& upBuf = upsampledBuffers_[ch];
        float* state = &firUpsampleState_[ch * (firOrder + 1)];

        for (int n = 0; n < outSamplesTotal; ++n) {
            // 移位状态
            for (int i = firOrder - 1; i >= 0; --i) {
                state[i + 1] = state[i];
            }
            state[0] = upBuf.getSample(0, n);

            // FIR 卷积
            float sum = 0.0f;
            for (int i = 0; i <= firOrder; ++i) {
                sum += firCoefficients_[i] * state[i];
            }
            upBuf.setSample(0, n, sum);
        }
    }
}

// =============================================================================
// 降采样: FIR 抗混叠滤波 → 抽取
// =============================================================================
void OversamplingProcessor::downsample(juce::AudioBuffer<float>& buffer) {
    int inSamples = buffer.getNumSamples();
    int outSamples = inSamples / oversamplingFactor_;

    int firOrder = static_cast<int>(firCoefficients_.size()) - 1;

    // 使用临时缓冲区存储降采样结果，避免原地读写冲突
    juce::AudioBuffer<float> downsampled(numChannels_, outSamples);

    for (int ch = 0; ch < numChannels_; ++ch) {
        const float* inData = buffer.getReadPointer(ch);
        float* outData = downsampled.getWritePointer(ch);
        float* state = &firDownsampleState_[ch * (firOrder + 1)];

        for (int n = 0; n < outSamples; ++n) {
            int inIdx = n * oversamplingFactor_;

            // 将当前过采样帧推入 FIR 延迟线
            for (int j = 0; j < oversamplingFactor_; ++j) {
                // 移位
                for (int i = firOrder - 1; i >= 0; --i) {
                    state[i + 1] = state[i];
                }
                state[0] = inData[inIdx + j];
            }

            // FIR 卷积 (抽取: 只取每第4个采样)
            float sum = 0.0f;
            for (int i = 0; i <= firOrder; ++i) {
                sum += firCoefficients_[i] * state[i];
            }
            outData[n] = sum;
        }
    }

    // 将降采样结果复制回 buffer
    buffer.setSize(numChannels_, outSamples, false, false, true);
    for (int ch = 0; ch < numChannels_; ++ch) {
        buffer.copyFrom(ch, 0, downsampled.getReadPointer(ch), outSamples);
    }
}

// =============================================================================
// 核心处理
// =============================================================================
bool OversamplingProcessor::process(juce::AudioBuffer<float>& buffer) {
    if (!enabled_ && !forceEnabled_) return false;

    int inSamples = buffer.getNumSamples();

    // 1. 上采样
    upsample(buffer);

    // 2. 将上采样后的数据复制到输出缓冲区 (4x大小)
    int outSamples = inSamples * oversamplingFactor_;
    tempBuffer_.setSize(numChannels_, outSamples, false, false, true);

    for (int ch = 0; ch < numChannels_; ++ch) {
        tempBuffer_.copyFrom(ch, 0,
            upsampledBuffers_[ch].getReadPointer(0), outSamples);
    }

    // 3. 交换缓冲区: buffer ← tempBuffer (调用者处理过采样数据)
    buffer.setSize(numChannels_, outSamples, false, false, true);
    for (int ch = 0; ch < numChannels_; ++ch) {
        buffer.copyFrom(ch, 0, tempBuffer_.getReadPointer(ch), outSamples);
    }

    return true;
}

// =============================================================================
// 降采样处理 (在音频图处理完成后调用)
// =============================================================================
void OversamplingProcessor::setEnabled(bool enabled) {
    forceEnabled_ = enabled;
    enabled_ = enabled;
}

} // namespace LianCore