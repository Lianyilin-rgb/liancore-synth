// =============================================================================
// LianCore - SpectralOscillator 频谱振荡器实现
// 基于FFT的频谱重合成、拉伸、移调、谐波混合、共振峰滤波
// =============================================================================
#include "SpectralOscillator.h"
#include "../utils/AudioUtils.h"
#include <juce_dsp/juce_dsp.h>

namespace LianCore {

// =============================================================================
// 共振峰预设 - 标准元音共振峰频率 (F1, F2, F3) Hz
// =============================================================================
namespace FormantPresets {
    struct FormantData { float f1, f2, f3; float bw1, bw2, bw3; };

    static const FormantData kVowels[5] = {
        //  A (father)   E (bet)      I (beet)     O (boat)     U (boot)
        { 730, 1090, 2440, 120, 150, 200 },  // A
        { 530, 1840, 2480, 100, 140, 180 },  // E
        { 270, 2290, 3010,  80, 130, 170 },  // I
        { 480,  760, 2620, 100, 130, 200 },  // O
        { 300,  870, 2240,  90, 120, 180 },  // U
    };
}

// =============================================================================
// 内部辅助: 锯齿波源信号生成 (带抗混叠多项式逼近)
// =============================================================================
static void generateSawtoothSource(float* output, int numSamples,
                                    float& phase, float frequency, double sampleRate) {
    float phaseInc = static_cast<float>(frequency / sampleRate);

    for (int i = 0; i < numSamples; ++i) {
        // 归一化相位到 [-1, 1]
        float p = phase * 2.0f - 1.0f;

        // 5阶多项式锯齿波逼近 (BLIT-like, 减少混叠)
        // p - p^3/6 + p^5/120
        float p2 = p * p;
        float p3 = p2 * p;
        float p5 = p3 * p2;
        output[i] = p - p3 * 0.1666667f + p5 * 0.0083333f;

        // 相位累加
        phase += phaseInc;
        if (phase >= 1.0f) {
            phase -= 1.0f;
        }
    }
}

// =============================================================================
// 构造与析构
// =============================================================================
SpectralOscillator::SpectralOscillator(const juce::String& name)
    : AudioNode(NodeType::SpectralOscillator, name)
{
    // 初始化FFT引擎 (默认2048点)
    fft_ = std::make_unique<juce::dsp::FFT>(static_cast<int>(std::log2(fftSize_)));
    window_ = std::make_unique<juce::dsp::WindowingFunction<float>>(
        fftSize_, juce::dsp::WindowingFunction<float>::hann, false);

    // 计算hop大小
    hopSize_ = fftSize_ / overlap_;

    // 分配频谱数据缓冲区 (仅正频率部分: fftSize/2 + 1)
    int numBins = fftSize_ / 2 + 1;
    spectralMagnitudes_.resize(numBins, 0.0f);
    spectralPhases_.resize(numBins, 0.0f);

    // 初始化谐波模板 - 标准锯齿波谐波序列 (1/n衰减)
    int numHarmonics = std::min(numBins - 1, 128);
    harmonicTemplate_.resize(numBins, 0.0f);
    for (int h = 1; h <= numHarmonics; ++h) {
        harmonicTemplate_[h] = 1.0f / static_cast<float>(h);
    }

    // 归一化谐波模板
    float maxVal = 0.0f;
    for (float v : harmonicTemplate_) maxVal = std::max(maxVal, v);
    if (maxVal > 0.0f) {
        for (float& v : harmonicTemplate_) v /= maxVal;
    }

    // 初始化重叠-相加缓冲区 (overlap_个独立帧缓冲区)
    overlapBuffers_.resize(overlap_);
    for (auto& buf : overlapBuffers_) {
        buf.resize(fftSize_, 0.0f);
    }
    overlapWritePos_ = 0;

    // 初始化参考频谱
    referenceSpectrum_.resize(numBins, 0.0f);

    // 添加音频输出端口
    PortDescriptor outputDesc;
    outputDesc.name = "音频输出";
    outputDesc.isAudio = true;
    outputDesc.defaultValue = 0.0f;
    outputDesc.minValue = -1.0f;
    outputDesc.maxValue = 1.0f;
    addOutputPort("音频输出", outputDesc);
}

// =============================================================================
// 音频处理
// =============================================================================
void SpectralOscillator::prepareToPlay(double sampleRate, int maxSamplesPerBlock) {
    AudioNode::prepareToPlay(sampleRate, maxSamplesPerBlock);
    sampleRate_ = sampleRate;
    blockSize_ = maxSamplesPerBlock;

    // 重新计算hop大小
    hopSize_ = fftSize_ / overlap_;

    // 确保hopSize有效
    jassert(hopSize_ > 0);
    jassert(fftSize_ <= kMaxFftSize);

    int numBins = fftSize_ / 2 + 1;

    // 重新分配频谱缓冲区
    spectralMagnitudes_.assign(numBins, 0.0f);
    spectralPhases_.assign(numBins, 0.0f);

    // 重新分配重叠-相加缓冲区
    overlapBuffers_.clear();
    overlapBuffers_.resize(overlap_);
    for (auto& buf : overlapBuffers_) {
        buf.assign(fftSize_, 0.0f);
    }
    overlapWritePos_ = 0;

    // 重置相位
    phase_ = 0.0f;

    // 更新内存使用统计
    size_t memUsage = 0;
    memUsage += spectralMagnitudes_.size() * sizeof(float);
    memUsage += spectralPhases_.size() * sizeof(float);
    memUsage += harmonicTemplate_.size() * sizeof(float);
    memUsage += referenceSpectrum_.size() * sizeof(float);
    for (const auto& buf : overlapBuffers_) {
        memUsage += buf.size() * sizeof(float);
    }
    updateMemoryUsage(memUsage);
}

void SpectralOscillator::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    auto& output = getOutputBuffer(0);
    output.clear();

    int numSamples = buffer.getNumSamples();
    float* outL = output.getWritePointer(0);
    float* outR = output.getWritePointer(1);

    // 处理MIDI消息
    for (const auto& msg : midi) {
        auto message = msg.getMessage();
        if (message.isNoteOn()) {
            setFrequency(static_cast<float>(
                AudioUtils::midiNoteToFrequency(message.getNoteNumber())));
        }
    }

    // ---- 线程局部工作缓冲区 (避免重复分配) ----
    static thread_local std::vector<float> tlSourceBuffer;
    static thread_local std::vector<float> tlFftBuffer;
    static thread_local std::vector<float> tlTimeDomain;
    static thread_local std::vector<float> tlFrameOutput;
    static thread_local std::vector<float> tlInputRing;
    static thread_local int tlThreadFftSize = 0;

    // 按需分配线程局部缓冲区
    if (tlThreadFftSize != fftSize_) {
        tlThreadFftSize = fftSize_;
        tlFftBuffer.assign(fftSize_ * 2, 0.0f);
        tlTimeDomain.assign(fftSize_, 0.0f);
        tlFrameOutput.assign(fftSize_, 0.0f);
        tlInputRing.assign(fftSize_, 0.0f);
        tlSourceBuffer.resize(kMaxBlockSize);
    }

    // ---- 线程局部重叠-相加状态 ----
    static thread_local int tlInputRingPos = 0;
    static thread_local int tlSamplesSinceLastFrame = 0;
    static thread_local int tlTotalFramesProcessed = 0;
    static thread_local int tlGlobalSampleCounter = 0;
    static thread_local int tlPrevFftSize = 0;

    // 如果FFT大小改变，重置所有状态
    if (tlPrevFftSize != fftSize_) {
        tlPrevFftSize = fftSize_;
        tlInputRingPos = 0;
        tlSamplesSinceLastFrame = 0;
        tlTotalFramesProcessed = 0;
        tlGlobalSampleCounter = 0;
        std::fill(tlInputRing.begin(), tlInputRing.end(), 0.0f);
    }

    // 生成锯齿波源信号
    generateSawtoothSource(tlSourceBuffer.data(), numSamples,
                           phase_, frequency_, sampleRate_);

    // ---- 逐采样处理: 输入累积 → FFT帧处理 → 重叠-相加输出 ----
    for (int i = 0; i < numSamples; ++i) {
        // 写入输入环形缓冲区
        tlInputRing[tlInputRingPos] = tlSourceBuffer[i];
        tlInputRingPos = (tlInputRingPos + 1) % fftSize_;
        tlSamplesSinceLastFrame++;

        // 检查是否需要处理新的FFT帧
        if (tlSamplesSinceLastFrame >= hopSize_) {
            tlSamplesSinceLastFrame = 0;

            // 从输入环形缓冲区读取fftSize_个采样 (按时间顺序，从旧到新)
            int readStart = tlInputRingPos; // 最旧的采样在写入位置
            for (int j = 0; j < fftSize_; ++j) {
                tlTimeDomain[j] = tlInputRing[(readStart + j) % fftSize_];
            }

            // 应用分析窗函数
            window_->multiplyWithWindowingTable(tlTimeDomain.data(), fftSize_);

            // 填充FFT缓冲区 (实部交错格式: [re0, im0, re1, im1, ...])
            std::fill(tlFftBuffer.begin(), tlFftBuffer.end(), 0.0f);
            for (int j = 0; j < fftSize_; ++j) {
                tlFftBuffer[j * 2] = tlTimeDomain[j];
            }

            // 正向FFT
            fft_->performRealOnlyForwardTransform(tlFftBuffer.data());

            // 提取幅度和相位
            int numBins = fftSize_ / 2 + 1;
            for (int j = 0; j < numBins; ++j) {
                float re = tlFftBuffer[j * 2];
                float im = tlFftBuffer[j * 2 + 1];
                spectralMagnitudes_[j] = std::sqrt(re * re + im * im);
                spectralPhases_[j] = std::atan2(im, re);
            }

            // 应用频谱处理 (拉伸/移调/谐波混合/共振峰)
            processSpectralFrame();

            // 转换回实部交错格式
            for (int j = 0; j < numBins; ++j) {
                float mag = spectralMagnitudes_[j];
                float phase = spectralPhases_[j];
                tlFftBuffer[j * 2] = mag * std::cos(phase);
                tlFftBuffer[j * 2 + 1] = mag * std::sin(phase);
            }

            // 反向FFT
            fft_->performRealOnlyInverseTransform(tlFftBuffer.data());

            // 提取时域输出
            for (int j = 0; j < fftSize_; ++j) {
                tlFrameOutput[j] = tlFftBuffer[j * 2];
            }

            // 应用合成窗函数 (重叠-相加需要双重加窗)
            window_->multiplyWithWindowingTable(tlFrameOutput.data(), fftSize_);

            // 重叠-相加: 写入当前帧缓冲区 (先清除旧数据，再写入新帧)
            int writeIdx = overlapWritePos_;
            std::fill(overlapBuffers_[writeIdx].begin(),
                      overlapBuffers_[writeIdx].end(), 0.0f);
            for (int j = 0; j < fftSize_; ++j) {
                overlapBuffers_[writeIdx][j] = tlFrameOutput[j];
            }

            // 推进写入位置
            overlapWritePos_ = (overlapWritePos_ + 1) % overlap_;
            tlTotalFramesProcessed++;
        }

        // ---- 从重叠缓冲区混合输出 ----
        // 当前全局采样位置: tlGlobalSampleCounter
        // 对于每个活跃帧, 计算其贡献并求和
        float outSample = 0.0f;

        // 确定活跃帧范围: 覆盖当前全局采样的帧索引
        // 帧f覆盖全局采样 [f*hopSize_, f*hopSize_ + fftSize_ - 1]
        // 活跃帧: f*hopSize_ <= tlGlobalSampleCounter < f*hopSize_ + fftSize_
        // 即: f <= tlGlobalSampleCounter/hopSize_
        //  且 f > (tlGlobalSampleCounter - fftSize_)/hopSize_
        int lastActiveFrame = tlGlobalSampleCounter / hopSize_;
        int firstActiveFrame = lastActiveFrame - overlap_ + 1;
        if (firstActiveFrame < 0) firstActiveFrame = 0;

        for (int f = firstActiveFrame; f <= lastActiveFrame; ++f) {
            // 帧在全局输出中的起始位置
            int frameGlobalStart = f * hopSize_;
            // 帧内偏移
            int frameOffset = tlGlobalSampleCounter - frameGlobalStart;

            if (frameOffset >= 0 && frameOffset < fftSize_) {
                int bufIdx = f % overlap_;
                outSample += overlapBuffers_[bufIdx][frameOffset];
            }
        }

        // 写入输出
        outSample *= volume_;
        outL[i] = outSample;
        outR[i] = outSample;

        // 推进全局采样计数器
        tlGlobalSampleCounter++;
    }

    // 更新处理时间
    updateProcessingTime(0.0);
}

void SpectralOscillator::releaseResources() {
    AudioNode::releaseResources();

    // 清空重叠缓冲区
    for (auto& buf : overlapBuffers_) {
        std::fill(buf.begin(), buf.end(), 0.0f);
    }
    overlapWritePos_ = 0;

    // 清空频谱数据
    std::fill(spectralMagnitudes_.begin(), spectralMagnitudes_.end(), 0.0f);
    std::fill(spectralPhases_.begin(), spectralPhases_.end(), 0.0f);

    // 重置相位
    phase_ = 0.0f;
}

// =============================================================================
// 统一频谱变形接口 (P2-3)
// =============================================================================
void SpectralOscillator::setSpectralWarpMode(SpectralWarper::Mode mode) {
    spectralWarper_.setMode(mode);
}

void SpectralOscillator::setSpectralWarpAmount(float amount) {
    spectralWarper_.setAmount(amount);
}

SpectralWarper::Mode SpectralOscillator::getSpectralWarpMode() const {
    return spectralWarper_.getMode();
}

float SpectralOscillator::getSpectralWarpAmount() const {
    return spectralWarper_.getAmount();
}

// =============================================================================
// 频谱帧处理
// =============================================================================
void SpectralOscillator::processSpectralFrame() {
    int numBins = fftSize_ / 2 + 1;

    // 复制当前幅度用于链式处理
    std::vector<float> processedMags = spectralMagnitudes_;

    // P2-3: 统一频谱变形处理 (优先使用)
    float warpAmount = spectralWarper_.getAmount();
    if (warpAmount > 0.001f) {
        spectralWarper_.process(processedMags, sampleRate_, fftSize_);
    }

    // 频谱拉伸 (仅当未使用统一变形Stretch模式时)
    if (spectralWarper_.getMode() != SpectralWarper::Mode::Stretch &&
        std::abs(spectralStretch_ - 1.0f) > 0.001f) {
        applySpectralStretch(processedMags, spectralStretch_);
    }

    // 频谱移调 (仅当未使用统一变形Shift模式时)
    if (spectralWarper_.getMode() != SpectralWarper::Mode::Shift &&
        std::abs(spectralShift_) > 0.01f) {
        applySpectralShift(processedMags, spectralShift_);
    }

    // 谐波混合
    if (harmonicBlend_ > 0.001f) {
        applyHarmonicBlend(processedMags, harmonicBlend_);
    }

    // 共振峰滤波
    if (std::abs(formantShift_) > 0.01f || formantPreset_ >= 0) {
        applyFormantFilter(processedMags, formantShift_);
    }

    // 将处理后的幅度写回
    for (int i = 0; i < numBins; ++i) {
        spectralMagnitudes_[i] = processedMags[i];
    }
}

// =============================================================================
// 频谱拉伸 - 通过重映射频率bin来拉伸/压缩频谱
// stretch: 0.5x(压缩) ~ 4.0x(拉伸)
// 算法: 对每个输出bin, 反向映射到源bin进行线性插值
// =============================================================================
void SpectralOscillator::applySpectralStretch(std::vector<float>& magnitudes, float stretch) {
    int numBins = static_cast<int>(magnitudes.size());
    std::vector<float> stretched(numBins, 0.0f);

    float invStretch = 1.0f / stretch;

    for (int i = 0; i < numBins; ++i) {
        // 拉伸时(stretch>1): 高频bin映射到更低的源索引 → 频谱扩展
        // 压缩时(stretch<1): 高频bin映射到更高的源索引 → 频谱收缩
        float srcBin = static_cast<float>(i) * invStretch;

        if (srcBin < static_cast<float>(numBins - 1)) {
            int srcIdx = static_cast<int>(srcBin);
            float frac = srcBin - static_cast<float>(srcIdx);

            // 线性插值
            if (srcIdx + 1 < numBins) {
                stretched[i] = AudioUtils::lerp(
                    magnitudes[srcIdx],
                    magnitudes[srcIdx + 1],
                    frac);
            } else {
                stretched[i] = magnitudes[srcIdx];
            }
        }
        // 超出范围的bin保持为0 (高频衰减)
    }

    magnitudes.swap(stretched);
}

// =============================================================================
// 频谱移调 - 通过平移频率bin来改变音高
// semitones: ±24 半音, 正值=升调, 负值=降调
// 算法: 将每个源bin的幅度散布到目标bin (使用线性插值分配)
// =============================================================================
void SpectralOscillator::applySpectralShift(std::vector<float>& magnitudes, float semitones) {
    int numBins = static_cast<int>(magnitudes.size());
    std::vector<float> shifted(numBins, 0.0f);

    // 计算频率倍率: +12半音 = 2x频率
    float ratio = static_cast<float>(AudioUtils::semitonesToRatio(static_cast<double>(semitones)));

    for (int i = 0; i < numBins; ++i) {
        // 源bin i 映射到目标bin: i * ratio
        float dstBin = static_cast<float>(i) * ratio;

        if (dstBin < static_cast<float>(numBins - 1)) {
            int dstIdx = static_cast<int>(dstBin);
            float frac = dstBin - static_cast<float>(dstIdx);

            // 线性插值分配: 将幅度按比例分配到相邻两个bin
            if (dstIdx + 1 < numBins) {
                shifted[dstIdx] += magnitudes[i] * (1.0f - frac);
                shifted[dstIdx + 1] += magnitudes[i] * frac;
            } else {
                shifted[dstIdx] += magnitudes[i];
            }
        }
        // 超出范围的bin丢弃 (频率超出奈奎斯特)
    }

    magnitudes.swap(shifted);
}

// =============================================================================
// 谐波混合 - 将当前频谱与谐波模板混合
// blend: 0.0 (纯原始频谱) ~ 1.0 (纯模板频谱)
// 回退: 如果模板为空, 使用参考频谱
// =============================================================================
void SpectralOscillator::applyHarmonicBlend(std::vector<float>& magnitudes, float blend) {
    int numBins = static_cast<int>(magnitudes.size());

    // 确保模板大小匹配
    if (harmonicTemplate_.size() != static_cast<size_t>(numBins)) {
        harmonicTemplate_.resize(numBins, 0.0f);
    }

    // 检查模板是否有效
    bool hasTemplate = false;
    for (float v : harmonicTemplate_) {
        if (v > 0.0001f) { hasTemplate = true; break; }
    }

    if (!hasTemplate) {
        // 回退到参考频谱
        if (referenceSpectrum_.size() == static_cast<size_t>(numBins)) {
            for (int i = 0; i < numBins; ++i) {
                magnitudes[i] = AudioUtils::lerp(magnitudes[i], referenceSpectrum_[i], blend);
            }
        }
        return;
    }

    // 混合当前频谱与谐波模板
    for (int i = 0; i < numBins; ++i) {
        magnitudes[i] = AudioUtils::lerp(magnitudes[i], harmonicTemplate_[i], blend);
    }
}

// =============================================================================
// 共振峰滤波 - 使用元音共振峰频率进行频谱塑形
// shift: 共振峰频率偏移量 (半音), 改变元音音色
// 算法: 对每个bin应用三个共振峰的高斯带通增益
// =============================================================================
void SpectralOscillator::applyFormantFilter(std::vector<float>& magnitudes, float shift) {
    int numBins = static_cast<int>(magnitudes.size());
    int clampedPreset = AudioUtils::clamp(static_cast<float>(formantPreset_), 0.0f, 4.0f);
    int presetIdx = static_cast<int>(clampedPreset);

    if (presetIdx < 0 || presetIdx >= 5) return;

    const auto& vowel = FormantPresets::kVowels[presetIdx];

    // 频率倍率 (用于共振峰整体偏移)
    float freqRatio = static_cast<float>(AudioUtils::semitonesToRatio(static_cast<double>(shift)));

    // 共振峰中心频率 (Hz)
    float f1 = vowel.f1 * freqRatio;
    float f2 = vowel.f2 * freqRatio;
    float f3 = vowel.f3 * freqRatio;

    // 共振峰带宽 (Hz)
    float bw1 = vowel.bw1;
    float bw2 = vowel.bw2;
    float bw3 = vowel.bw3;

    // 每个bin的频率分辨率
    float binWidth = static_cast<float>(sampleRate_) / static_cast<float>(fftSize_);

    // 对每个频率bin应用共振峰增益
    for (int i = 0; i < numBins; ++i) {
        float binFreq = static_cast<float>(i) * binWidth;

        // 高斯带通增益函数
        auto gaussianGain = [](float freq, float center, float bandwidth) -> float {
            if (bandwidth <= 0.0f) return 0.0f;
            float diff = (freq - center) / bandwidth;
            return std::exp(-diff * diff * 0.5f);
        };

        float gain1 = gaussianGain(binFreq, f1, bw1);
        float gain2 = gaussianGain(binFreq, f2, bw2);
        float gain3 = gaussianGain(binFreq, f3, bw3);

        // 加权组合三个共振峰 (F1权重最高, 决定元音音色)
        float formantGain = gain1 * 0.5f + gain2 * 0.35f + gain3 * 0.15f;

        // 增益范围: 1.0 ~ 3.0 (增强而非衰减)
        formantGain = 1.0f + formantGain * 2.0f;

        magnitudes[i] *= formantGain;
    }
}

// =============================================================================
// 重合成 - 预留接口, 实际处理已在processBlock中完成
// =============================================================================
void SpectralOscillator::resynthesize(juce::AudioBuffer<float>& buffer, int numSamples) {
    juce::ignoreUnused(buffer, numSamples);
}

// =============================================================================
// FFT参数
// =============================================================================
void SpectralOscillator::setFftSize(int size) {
    // 确保FFT大小为2的幂且在有效范围内
    int clamped = static_cast<int>(AudioUtils::clamp(static_cast<float>(size),
                                     static_cast<float>(kMinFftSize),
                                     static_cast<float>(kMaxFftSize)));

    // 向上取整到最近的2的幂
    int powerOfTwo = 1;
    while (powerOfTwo < clamped) {
        powerOfTwo <<= 1;
    }
    fftSize_ = powerOfTwo;

    // 重新创建FFT引擎
    int order = static_cast<int>(std::log2(fftSize_));
    fft_ = std::make_unique<juce::dsp::FFT>(order);
    window_ = std::make_unique<juce::dsp::WindowingFunction<float>>(
        fftSize_, juce::dsp::WindowingFunction<float>::hann, false);

    // 重新计算hop大小
    hopSize_ = fftSize_ / overlap_;

    // 重新分配频谱缓冲区
    int numBins = fftSize_ / 2 + 1;
    spectralMagnitudes_.assign(numBins, 0.0f);
    spectralPhases_.assign(numBins, 0.0f);

    // 重新分配重叠缓冲区
    for (auto& buf : overlapBuffers_) {
        buf.assign(fftSize_, 0.0f);
    }
    overlapWritePos_ = 0;
}

void SpectralOscillator::setOverlap(int overlap) {
    overlap_ = static_cast<int>(AudioUtils::clamp(static_cast<float>(overlap),
                                   2.0f, static_cast<float>(kMaxOverlap)));
    // 确保overlap是2的幂
    int powerOfTwo = 2;
    while (powerOfTwo < overlap_) {
        powerOfTwo <<= 1;
    }
    overlap_ = powerOfTwo;

    hopSize_ = fftSize_ / overlap_;

    // 重新分配重叠缓冲区
    overlapBuffers_.resize(overlap_);
    for (auto& buf : overlapBuffers_) {
        buf.assign(fftSize_, 0.0f);
    }
    overlapWritePos_ = 0;
}

// =============================================================================
// 频谱操作
// =============================================================================
void SpectralOscillator::setSpectralStretch(float amount) {
    spectralStretch_ = AudioUtils::clamp(amount, 0.5f, 4.0f);
}

void SpectralOscillator::setSpectralShift(float semitones) {
    spectralShift_ = AudioUtils::clamp(semitones, -24.0f, 24.0f);
}

void SpectralOscillator::setHarmonicBlend(float blend) {
    harmonicBlend_ = AudioUtils::clamp(blend, 0.0f, 1.0f);
}

// =============================================================================
// 共振峰
// =============================================================================
void SpectralOscillator::setFormantShift(float amount) {
    formantShift_ = AudioUtils::clamp(amount, -12.0f, 12.0f);
}

void SpectralOscillator::setFormantPreset(int index) {
    formantPreset_ = static_cast<int>(AudioUtils::clamp(static_cast<float>(index), 0.0f, 4.0f));
}

// =============================================================================
// 音频参考 - 分析参考音频的频谱
// =============================================================================
void SpectralOscillator::loadReferenceSpectrum(const juce::AudioSampleBuffer& audio) {
    int numSamples = audio.getNumSamples();
    if (numSamples == 0) return;

    int numBins = fftSize_ / 2 + 1;
    referenceSpectrum_.assign(numBins, 0.0f);

    // 使用临时FFT分析参考音频
    int fftOrder = static_cast<int>(std::log2(fftSize_));
    juce::dsp::FFT refFft(fftOrder);

    std::vector<float> refFftBuffer(fftSize_ * 2, 0.0f);
    juce::dsp::WindowingFunction<float> refWindow(
        fftSize_, juce::dsp::WindowingFunction<float>::hann, false);

    // 多帧分析取平均
    int numFrames = 0;
    int refHopSize = fftSize_ / 4;

    for (int start = 0; start + fftSize_ <= numSamples; start += refHopSize) {
        // 复制并加窗
        const float* readPtr = audio.getReadPointer(0, start);
        for (int j = 0; j < fftSize_; ++j) {
            refFftBuffer[j * 2] = readPtr[j];
            refFftBuffer[j * 2 + 1] = 0.0f;
        }
        refWindow.multiplyWithWindowingTable(refFftBuffer.data(), fftSize_);

        // 正向FFT
        refFft.performRealOnlyForwardTransform(refFftBuffer.data());

        // 累加幅度谱
        for (int j = 0; j < numBins; ++j) {
            float re = refFftBuffer[j * 2];
            float im = refFftBuffer[j * 2 + 1];
            referenceSpectrum_[j] += std::sqrt(re * re + im * im);
        }

        numFrames++;
    }

    // 取平均并归一化
    if (numFrames > 0) {
        float maxVal = 0.0f;
        for (int j = 0; j < numBins; ++j) {
            referenceSpectrum_[j] /= static_cast<float>(numFrames);
            maxVal = std::max(maxVal, referenceSpectrum_[j]);
        }
        if (maxVal > 0.0f) {
            for (int j = 0; j < numBins; ++j) {
                referenceSpectrum_[j] /= maxVal;
            }
        }
    }
}

// =============================================================================
// AI频谱映射 - 使用外部谐波参数更新谐波模板
// =============================================================================
void SpectralOscillator::applyAiSpectralMapping(const std::vector<float>& harmonicParams) {
    int numBins = fftSize_ / 2 + 1;

    // 使用AI谐波参数更新谐波模板
    harmonicTemplate_.assign(numBins, 0.0f);

    int numHarmonics = static_cast<int>(harmonicParams.size());
    for (int h = 0; h < numHarmonics && h < numBins; ++h) {
        harmonicTemplate_[h] = AudioUtils::clamp(harmonicParams[h], 0.0f, 1.0f);
    }

    // 归一化
    float maxVal = 0.0f;
    for (float v : harmonicTemplate_) maxVal = std::max(maxVal, v);
    if (maxVal > 0.0f) {
        for (float& v : harmonicTemplate_) v /= maxVal;
    }
}

// =============================================================================
// 播放控制
// =============================================================================
void SpectralOscillator::setFrequency(float hz) {
    frequency_ = AudioUtils::clamp(hz, 1.0f, 20000.0f);
}

void SpectralOscillator::setVolume(float volume) {
    volume_ = AudioUtils::clamp(volume, 0.0f, 1.0f);
}

// =============================================================================
// 参数接口 (9个参数, 归一化到 0.0~1.0)
// =============================================================================
float SpectralOscillator::getParameter(int index) const {
    switch (index) {
        case 0: return (spectralStretch_ - 0.5f) / 3.5f;         // 0.5-4.0 → 0.0-1.0
        case 1: return (spectralShift_ + 24.0f) / 48.0f;         // -24~24 → 0.0-1.0
        case 2: return harmonicBlend_;                             // 0.0-1.0
        case 3: return (formantShift_ + 12.0f) / 24.0f;          // -12~12 → 0.0-1.0
        case 4: return static_cast<float>(formantPreset_) / 4.0f; // 0-4 → 0.0-1.0
        case 5: return static_cast<float>(fftSize_) / static_cast<float>(kMaxFftSize);
        case 6: return static_cast<float>(overlap_) / static_cast<float>(kMaxOverlap);
        case 7: return frequency_ / 20000.0f;                     // 1-20000 → 0.0-1.0
        case 8: return volume_;                                   // 0.0-1.0
        default: return 0.0f;
    }
}

void SpectralOscillator::setParameter(int index, float value) {
    switch (index) {
        case 0: setSpectralStretch(0.5f + value * 3.5f); break;
        case 1: setSpectralShift(-24.0f + value * 48.0f); break;
        case 2: setHarmonicBlend(value); break;
        case 3: setFormantShift(-12.0f + value * 24.0f); break;
        case 4: setFormantPreset(static_cast<int>(value * 4.0f)); break;
        case 5: setFftSize(static_cast<int>(value * kMaxFftSize)); break;
        case 6: setOverlap(static_cast<int>(value * kMaxOverlap)); break;
        case 7: setFrequency(value * 20000.0f); break;
        case 8: setVolume(value); break;
        default: break;
    }
}

juce::String SpectralOscillator::getParameterName(int index) const {
    switch (index) {
        case 0: return "频谱拉伸";
        case 1: return "频谱移调";
        case 2: return "谐波混合";
        case 3: return "共振峰偏移";
        case 4: return "共振峰预设";
        case 5: return "FFT大小";
        case 6: return "重叠数";
        case 7: return "频率";
        case 8: return "音量";
        default: return "未知";
    }
}

// =============================================================================
// 序列化
// =============================================================================
juce::var SpectralOscillator::toJson() const {
    auto json = AudioNode::toJson();

    // 频谱参数
    json.getDynamicObject()->setProperty("spectralStretch", spectralStretch_);
    json.getDynamicObject()->setProperty("spectralShift", spectralShift_);
    json.getDynamicObject()->setProperty("harmonicBlend", harmonicBlend_);
    json.getDynamicObject()->setProperty("formantShift", formantShift_);
    json.getDynamicObject()->setProperty("formantPreset", formantPreset_);

    // FFT参数
    json.getDynamicObject()->setProperty("fftSize", fftSize_);
    json.getDynamicObject()->setProperty("overlap", overlap_);

    // 播放参数
    json.getDynamicObject()->setProperty("frequency", frequency_);
    json.getDynamicObject()->setProperty("volume", volume_);

    return json;
}

void SpectralOscillator::fromJson(const juce::var& json) {
    AudioNode::fromJson(json);

    if (auto* obj = json.getDynamicObject()) {
        // 频谱参数
        if (obj->hasProperty("spectralStretch"))
            setSpectralStretch(static_cast<float>(json["spectralStretch"]));
        if (obj->hasProperty("spectralShift"))
            setSpectralShift(static_cast<float>(json["spectralShift"]));
        if (obj->hasProperty("harmonicBlend"))
            setHarmonicBlend(static_cast<float>(json["harmonicBlend"]));
        if (obj->hasProperty("formantShift"))
            setFormantShift(static_cast<float>(json["formantShift"]));
        if (obj->hasProperty("formantPreset"))
            setFormantPreset(static_cast<int>(json["formantPreset"]));

        // FFT参数 (先设置overlap再设置fftSize, 避免hopSize计算问题)
        if (obj->hasProperty("overlap"))
            setOverlap(static_cast<int>(json["overlap"]));
        if (obj->hasProperty("fftSize"))
            setFftSize(static_cast<int>(json["fftSize"]));

        // 播放参数
        if (obj->hasProperty("frequency"))
            setFrequency(static_cast<float>(json["frequency"]));
        if (obj->hasProperty("volume"))
            setVolume(static_cast<float>(json["volume"]));
    }
}

} // namespace LianCore