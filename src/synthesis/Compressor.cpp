// =============================================================================
// LianCore - Compressor 实现
// 前馈式RMS压缩器: 软拐点、RMS包络检测、立体声联动、增益补偿
// =============================================================================
#include "Compressor.h"
#include "../utils/AudioUtils.h"

namespace LianCore {

// =============================================================================
// 构造/析构
// =============================================================================
Compressor::Compressor(const juce::String& name)
    : AudioNode(NodeType::Compressor, name) {}

// =============================================================================
// 生命周期
// =============================================================================
void Compressor::prepareToPlay(double sampleRate, int maxSamplesPerBlock) {
    AudioNode::prepareToPlay(sampleRate, maxSamplesPerBlock);

    // 计算10ms RMS窗口大小
    rmsWindowSize_ = static_cast<int>(kRmsWindowMs * 0.001 * sampleRate);
    rmsWindowSize_ = std::max(rmsWindowSize_, 1);

    // 分配RMS检测器缓冲区
    for (int ch = 0; ch < 2; ++ch) {
        detectors_[ch].rmsBuffer.assign(rmsWindowSize_, 0.0f);
        detectors_[ch].writePos = 0;
        detectors_[ch].rmsSum = 0.0f;
        detectors_[ch].currentRms = 0.0f;
    }

    updateInternalParams();
}

void Compressor::releaseResources() {
    AudioNode::releaseResources();
}

// =============================================================================
// 更新内部参数 (根据归一化参数计算实际值)
// =============================================================================
void Compressor::updateInternalParams() {
    // 阈值: 0-1 → -60dB 至 0dB
    thresholdDb_ = AudioUtils::mapRange(threshold_, 0.0f, 1.0f, -60.0f, 0.0f);

    // 压缩比: 0-1 → 1:1 至 20:1
    ratioValue_ = AudioUtils::mapRange(ratio_, 0.0f, 1.0f, 1.0f, 20.0f);

    // 启动时间: 0-1 → 0.1ms 至 100ms
    attackTime_ = AudioUtils::mapRange(attack_, 0.0f, 1.0f, 0.1f, 100.0f);

    // 释放时间: 0-1 → 10ms 至 1000ms
    releaseTime_ = AudioUtils::mapRange(release_, 0.0f, 1.0f, 10.0f, 1000.0f);

    // 拐点宽度: 0-1 → 0dB 至 30dB
    kneeWidth_ = AudioUtils::mapRange(knee_, 0.0f, 1.0f, 0.0f, 30.0f);

    // 增益补偿: 0-1 → 0dB 至 30dB
    makeupGainDb_ = AudioUtils::mapRange(makeupGain_, 0.0f, 1.0f, 0.0f, 30.0f);

    // 计算启动/释放系数 (时间常数 → 每采样平滑系数)
    // coeff = exp(-1 / (time_seconds * sampleRate))
    float sr = static_cast<float>(sampleRate_);
    if (attackTime_ > 0.001f) {
        attackCoeff_ = std::exp(-1.0f / (attackTime_ * 0.001f * sr));
    } else {
        attackCoeff_ = 0.0f; // 即时响应
    }

    if (releaseTime_ > 0.001f) {
        releaseCoeff_ = std::exp(-1.0f / (releaseTime_ * 0.001f * sr));
    } else {
        releaseCoeff_ = 0.0f;
    }
}

// =============================================================================
// 计算增益衰减 (dB)
// 输入: levelDb - 当前RMS电平 (dB)
// 输出: 增益衰减量 (dB, 负值表示衰减)
// =============================================================================
float Compressor::calculateGainReduction(float levelDb) const {
    // 输入电平低于阈值 → 无压缩
    if (levelDb <= thresholdDb_ - kneeWidth_ * 0.5f) {
        return 0.0f;
    }

    // 软拐点区域: 在阈值附近使用二次插值平滑过渡
    float kneeHalf = kneeWidth_ * 0.5f;
    if (kneeWidth_ > 0.0f && levelDb < thresholdDb_ + kneeHalf) {
        // 软拐点: 二次函数平滑过渡
        float overshoot = levelDb - (thresholdDb_ - kneeHalf);
        float overshootNorm = overshoot / kneeWidth_;

        // 线性压缩量
        float linearGain = overshoot * (1.0f - 1.0f / ratioValue_);

        // 软拐点: 在拐点起始处增益为0，在拐点结束处达到全压缩
        // gain = overshoot² / (2 * kneeWidth) * (1 - 1/ratio)
        float softGain = (overshoot * overshoot) / (2.0f * kneeWidth_) * (1.0f - 1.0f / ratioValue_);

        // 在拐点区域内用二次平滑
        float t = overshootNorm; // 0 到 1
        float smoothed = softGain * t + linearGain * (1.0f - t) * t;
        return -smoothed;
    }

    // 硬拐点区域: gain = threshold + (level - threshold) / ratio
    float aboveThreshold = levelDb - thresholdDb_;
    float compressed = aboveThreshold / ratioValue_;
    float gainReduction = compressed - aboveThreshold;

    return gainReduction; // 负值
}

// =============================================================================
// 音频处理
// =============================================================================
void Compressor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    auto& input = getInputBuffer(0);
    auto& output = getOutputBuffer(0);

    int numSamples = buffer.getNumSamples();
    int numChannels = std::min(2, buffer.getNumChannels());

    // 更新内部参数（确保响应实时参数变化）
    updateInternalParams();

    // 增益补偿线性值
    float makeupGainLinear = AudioUtils::dbToAmplitude(makeupGainDb_);

    for (int i = 0; i < numSamples; ++i) {
        // =============================================================
        // 1. RMS包络检测 (立体声联动: 取L/R中RMS最大值)
        // =============================================================
        float maxRms = 0.0f;

        for (int ch = 0; ch < numChannels; ++ch) {
            const float* in = input.getReadPointer(ch);
            auto& det = detectors_[ch];

            float x = in[i];

            // 更新RMS滑动窗口
            float oldSample = det.rmsBuffer[det.writePos];
            float newSample = x * x; // 平方

            det.rmsSum += newSample - oldSample;
            det.rmsBuffer[det.writePos] = newSample;
            det.writePos = (det.writePos + 1) % rmsWindowSize_;

            // 确保RMS和不为负 (浮点精度)
            if (det.rmsSum < 0.0f) det.rmsSum = 0.0f;

            // 计算RMS值
            det.currentRms = std::sqrt(det.rmsSum / static_cast<float>(rmsWindowSize_));

            // 立体声联动: 取最大RMS
            if (det.currentRms > maxRms) {
                maxRms = det.currentRms;
            }
        }

        // 如果只有单声道，直接使用该声道RMS
        if (numChannels == 1) {
            maxRms = detectors_[0].currentRms;
        }

        // =============================================================
        // 2. 转换为dB域
        // =============================================================
        float levelDb = AudioUtils::amplitudeToDb(maxRms);

        // =============================================================
        // 3. 计算目标增益衰减
        // =============================================================
        float targetGainReduction = calculateGainReduction(levelDb);

        // =============================================================
        // 4. 启动/释放平滑
        // =============================================================
        if (targetGainReduction < smoothedGainReduction_) {
            // 启动阶段: 需要更多压缩 (更负的增益)
            smoothedGainReduction_ = targetGainReduction +
                attackCoeff_ * (smoothedGainReduction_ - targetGainReduction);
        } else {
            // 释放阶段: 需要更少压缩 (恢复到0)
            smoothedGainReduction_ = targetGainReduction +
                releaseCoeff_ * (smoothedGainReduction_ - targetGainReduction);
        }

        // =============================================================
        // 5. 应用增益到所有声道
        // =============================================================
        float gainLinear = AudioUtils::dbToAmplitude(smoothedGainReduction_) * makeupGainLinear;

        for (int ch = 0; ch < numChannels; ++ch) {
            const float* in = input.getReadPointer(ch);
            float* out = output.getWritePointer(ch);

            float dry = in[i];
            float wet = dry * gainLinear;

            // 干湿混合
            out[i] = AudioUtils::lerp(dry, wet, mix_);
        }
    }
}

// =============================================================================
// 参数设置
// =============================================================================
void Compressor::setThreshold(float value) {
    threshold_ = AudioUtils::clamp(value, 0.0f, 1.0f);
    updateInternalParams();
}

void Compressor::setRatio(float value) {
    ratio_ = AudioUtils::clamp(value, 0.0f, 1.0f);
    updateInternalParams();
}

void Compressor::setAttack(float value) {
    attack_ = AudioUtils::clamp(value, 0.0f, 1.0f);
    updateInternalParams();
}

void Compressor::setRelease(float value) {
    release_ = AudioUtils::clamp(value, 0.0f, 1.0f);
    updateInternalParams();
}

void Compressor::setKnee(float value) {
    knee_ = AudioUtils::clamp(value, 0.0f, 1.0f);
    updateInternalParams();
}

void Compressor::setMakeupGain(float value) {
    makeupGain_ = AudioUtils::clamp(value, 0.0f, 1.0f);
    updateInternalParams();
}

void Compressor::setMix(float value) {
    mix_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

// =============================================================================
// 参数接口
// =============================================================================
float Compressor::getParameter(int index) const {
    switch (index) {
        case 0: return threshold_;
        case 1: return ratio_;
        case 2: return attack_;
        case 3: return release_;
        case 4: return knee_;
        case 5: return makeupGain_;
        case 6: return mix_;
        default: return 0.0f;
    }
}

void Compressor::setParameter(int index, float value) {
    switch (index) {
        case 0: setThreshold(value); break;
        case 1: setRatio(value); break;
        case 2: setAttack(value); break;
        case 3: setRelease(value); break;
        case 4: setKnee(value); break;
        case 5: setMakeupGain(value); break;
        case 6: setMix(value); break;
        default: break;
    }
}

juce::String Compressor::getParameterName(int index) const {
    switch (index) {
        case 0: return "阈值";
        case 1: return "压缩比";
        case 2: return "启动时间";
        case 3: return "释放时间";
        case 4: return "拐点宽度";
        case 5: return "增益补偿";
        case 6: return "干湿比";
        default: return "未知";
    }
}

// =============================================================================
// 序列化
// =============================================================================
juce::var Compressor::toJson() const {
    auto json = AudioNode::toJson();
    if (auto* obj = json.getDynamicObject()) {
        obj->setProperty("threshold", threshold_);
        obj->setProperty("ratio", ratio_);
        obj->setProperty("attack", attack_);
        obj->setProperty("release", release_);
        obj->setProperty("knee", knee_);
        obj->setProperty("makeupGain", makeupGain_);
        obj->setProperty("mix", mix_);
    }
    return json;
}

void Compressor::fromJson(const juce::var& json) {
    AudioNode::fromJson(json);
    if (auto* obj = json.getDynamicObject()) {
        if (obj->hasProperty("threshold")) setThreshold(obj->getProperty("threshold"));
        if (obj->hasProperty("ratio")) setRatio(obj->getProperty("ratio"));
        if (obj->hasProperty("attack")) setAttack(obj->getProperty("attack"));
        if (obj->hasProperty("release")) setRelease(obj->getProperty("release"));
        if (obj->hasProperty("knee")) setKnee(obj->getProperty("knee"));
        if (obj->hasProperty("makeupGain")) setMakeupGain(obj->getProperty("makeupGain"));
        if (obj->hasProperty("mix")) setMix(obj->getProperty("mix"));
    }
}

} // namespace LianCore