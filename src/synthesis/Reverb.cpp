// =============================================================================
// LianCore - Reverb 实现
// Schroeder-Moorer算法: 4并行梳状滤波器 + 2串联全通滤波器
// 包含预延迟、早期反射、阻尼、立体声宽度处理
// =============================================================================
#include "Reverb.h"
#include "../utils/AudioUtils.h"

namespace LianCore {

// =============================================================================
// 构造/析构
// =============================================================================
Reverb::Reverb(const juce::String& name)
    : AudioNode(NodeType::Reverb, name) {}

// =============================================================================
// 生命周期
// =============================================================================
void Reverb::prepareToPlay(double sampleRate, int maxSamplesPerBlock) {
    AudioNode::prepareToPlay(sampleRate, maxSamplesPerBlock);
    allocateBuffers();
    updateInternalParams();
}

void Reverb::releaseResources() {
    AudioNode::releaseResources();
}

// =============================================================================
// 分配所有延迟缓冲区
// =============================================================================
void Reverb::allocateBuffers() {
    // 梳状滤波器缓冲区 (L/R)
    for (int i = 0; i < kNumCombs; ++i) {
        combL_[i].buffer.assign(kMaxCombDelay, 0.0f);
        combL_[i].writePos = 0;
        combL_[i].lastDamp = 0.0f;
        combR_[i].buffer.assign(kMaxCombDelay, 0.0f);
        combR_[i].writePos = 0;
        combR_[i].lastDamp = 0.0f;
    }

    // 全通滤波器缓冲区 (L/R)
    for (int i = 0; i < kNumAllPass; ++i) {
        apL_[i].buffer.assign(kMaxAllPassDelay, 0.0f);
        apL_[i].writePos = 0;
        apR_[i].buffer.assign(kMaxAllPassDelay, 0.0f);
        apR_[i].writePos = 0;
    }

    // 预延迟缓冲区
    for (int ch = 0; ch < 2; ++ch) {
        predelayChans_[ch].buffer.assign(kMaxPredelaySamples, 0.0f);
        predelayChans_[ch].writePos = 0;
    }

    // 早期反射缓冲区
    for (int ch = 0; ch < 2; ++ch) {
        earlyReflChans_[ch].buffer.assign(kMaxEarlyReflDelay, 0.0f);
        earlyReflChans_[ch].writePos = 0;
    }
}

// =============================================================================
// 更新内部参数 (根据归一化参数计算实际值)
// =============================================================================
void Reverb::updateInternalParams() {
    // 房间大小: 0-1 → 0.1-1.0 缩放系数
    roomSizeScaled_ = AudioUtils::mapRange(roomSize_, 0.0f, 1.0f, 0.1f, 1.0f);

    // 衰减时间: 0-1 → 0.1-30秒
    decayTime_ = AudioUtils::mapRange(decay_, 0.0f, 1.0f, 0.1f, 30.0f);

    // 阻尼频率: 0-1 → 20000Hz-500Hz (0=亮, 1=暗)
    dampingFreq_ = AudioUtils::mapRange(1.0f - damping_, 0.0f, 1.0f, 500.0f, 20000.0f);

    // 预延迟: 0-1 → 0-200ms
    float predelayMs = AudioUtils::mapRange(predelay_, 0.0f, 1.0f, 0.0f, 200.0f);
    predelaySamples_ = static_cast<int>(predelayMs * 0.001f * static_cast<float>(sampleRate_));
    predelaySamples_ = std::min(predelaySamples_, kMaxPredelaySamples - 1);

    // 更新梳状滤波器延迟时间 (按房间大小缩放)
    for (int i = 0; i < kNumCombs; ++i) {
        int delaySamples = static_cast<int>(
            baseCombDelaysMs_[i] * 0.001f * static_cast<float>(sampleRate_) * roomSizeScaled_);
        delaySamples = AudioUtils::clamp(delaySamples, 1, kMaxCombDelay - 1);
        combL_[i].delayLen = delaySamples;
        // 右声道使用略不同的延迟 (立体声加宽)
        int delayR = static_cast<int>(delaySamples * 0.97f);
        delayR = AudioUtils::clamp(delayR, 1, kMaxCombDelay - 1);
        combR_[i].delayLen = delayR;
    }

    // 更新全通滤波器延迟时间
    for (int i = 0; i < kNumAllPass; ++i) {
        int delaySamples = static_cast<int>(
            baseAllPassDelaysMs_[i] * 0.001f * static_cast<float>(sampleRate_) * roomSizeScaled_);
        delaySamples = AudioUtils::clamp(delaySamples, 1, kMaxAllPassDelay - 1);
        apL_[i].delayLen = delaySamples;
        int delayR = static_cast<int>(delaySamples * 0.96f);
        delayR = AudioUtils::clamp(delayR, 1, kMaxAllPassDelay - 1);
        apR_[i].delayLen = delayR;
    }
}

// =============================================================================
// 梳状滤波器: y[n] = x[n] + feedback * y[n-delay]
// 反馈路径中包含一阶低通阻尼
// =============================================================================
float Reverb::processComb(float input, float* buffer, int& writePos,
                           int delayLen, float feedback, float damping,
                           float& lastDamp) {
    // 读取延迟线输出
    int readPos = writePos - delayLen;
    if (readPos < 0) readPos += delayLen;
    float delayed = buffer[readPos];

    // 一阶低通滤波 (阻尼)
    lastDamp = delayed + damping * (lastDamp - delayed);

    // 梳状滤波: y = x + feedback * y_delayed_damped
    float output = input + feedback * lastDamp;

    // 写入延迟线
    buffer[writePos] = output;
    writePos = (writePos + 1) % delayLen;

    return output;
}

// =============================================================================
// 全通滤波器: y[n] = -g*x[n] + x[n-delay] + g*y[n-delay]
// =============================================================================
float Reverb::processAllPass(float input, float* buffer, int& writePos,
                              int delayLen, float gain) {
    int readPos = writePos - delayLen;
    if (readPos < 0) readPos += delayLen;
    float delayed = buffer[readPos];

    // 全通公式: y = -g*x + x_delayed + g*y_delayed
    float output = -gain * input + delayed + gain * delayed;

    buffer[writePos] = input + gain * delayed;
    writePos = (writePos + 1) % delayLen;

    return output;
}

// =============================================================================
// 音频处理
// =============================================================================
void Reverb::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    auto& input = getInputBuffer(0);
    auto& output = getOutputBuffer(0);

    int numSamples = buffer.getNumSamples();
    int numChannels = std::min(2, buffer.getNumChannels());

    // 更新内部参数（确保响应实时参数变化）
    updateInternalParams();

    // 计算梳状滤波器反馈系数
    // feedback ≈ 0.001^(delay/decayTime) 确保RT60 ≈ decayTime
    float avgCombDelay = 0.0f;
    for (int i = 0; i < kNumCombs; ++i) {
        avgCombDelay += static_cast<float>(combL_[i].delayLen);
    }
    avgCombDelay /= (kNumCombs * static_cast<float>(sampleRate_));

    float feedbackBase = std::pow(0.001f, avgCombDelay / decayTime_);
    feedbackBase = AudioUtils::clamp(feedbackBase, 0.0f, 0.999f);

    // 阻尼系数 (一阶低通)
    float dampCoeff = 1.0f - std::exp(-2.0f * 3.14159265f * dampingFreq_ / static_cast<float>(sampleRate_));
    dampCoeff = AudioUtils::clamp(dampCoeff, 0.0f, 1.0f);

    // 立体声宽度系数
    float widthFactor = width_;

    // 预延迟读取索引
    int predelayReadBase = predelaySamples_;

    for (int ch = 0; ch < numChannels; ++ch) {
        const float* in = input.getReadPointer(ch);
        float* out = output.getWritePointer(ch);

        auto& predelayChan = predelayChans_[ch];
        auto& earlyReflChan = earlyReflChans_[ch];

        // 选择当前声道对应的梳状/全通滤波器组
        auto& combs = (ch == 0) ? combL_ : combR_;
        auto& aps = (ch == 0) ? apL_ : apR_;

        for (int i = 0; i < numSamples; ++i) {
            float dry = in[i];

            // =============================================================
            // 1. 预延迟 - 将输入写入预延迟缓冲，读取延迟后的信号
            // =============================================================
            predelayChan.buffer[predelayChan.writePos] = dry;
            int predelayReadPos = predelayChan.writePos - predelayReadBase;
            if (predelayReadPos < 0) predelayReadPos += kMaxPredelaySamples;
            float predelayed = predelayChan.buffer[predelayReadPos];
            predelayChan.writePos = (predelayChan.writePos + 1) % kMaxPredelaySamples;

            // =============================================================
            // 2. 早期反射 - 4个抽头延迟混合
            // =============================================================
            earlyReflChan.buffer[earlyReflChan.writePos] = predelayed;
            float earlyReflOut = 0.0f;
            for (int er = 0; er < kNumEarlyReflections; ++er) {
                int erDelay = static_cast<int>(
                    earlyReflDelaysMs_[er] * 0.001f * static_cast<float>(sampleRate_));
                erDelay = AudioUtils::clamp(erDelay, 1, kMaxEarlyReflDelay - 1);
                int erReadPos = earlyReflChan.writePos - erDelay;
                if (erReadPos < 0) erReadPos += kMaxEarlyReflDelay;
                earlyReflOut += earlyReflChan.buffer[erReadPos] * earlyReflGains_[er];
            }
            earlyReflChan.writePos = (earlyReflChan.writePos + 1) % kMaxEarlyReflDelay;

            // 混合预延迟信号和早期反射
            float reverbInput = AudioUtils::lerp(predelayed, earlyReflOut, earlyReflections_);

            // =============================================================
            // 3. Schroeder梳状滤波器组 (并行)
            // =============================================================
            float combSum = 0.0f;
            for (int c = 0; c < kNumCombs; ++c) {
                combSum += processComb(reverbInput,
                                       combs[c].buffer.data(),
                                       combs[c].writePos,
                                       combs[c].delayLen,
                                       feedbackBase,
                                       dampCoeff,
                                       combs[c].lastDamp);
            }

            // 平均梳状输出
            float combOut = combSum / static_cast<float>(kNumCombs);

            // =============================================================
            // 4. Moorer全通滤波器组 (串联)
            // =============================================================
            float apOut = combOut;
            for (int a = 0; a < kNumAllPass; ++a) {
                apOut = processAllPass(apOut,
                                       aps[a].buffer.data(),
                                       aps[a].writePos,
                                       aps[a].delayLen,
                                       apGains_[a]);
            }

            // =============================================================
            // 5. 立体声宽度处理
            // =============================================================
            float wet = apOut;
            if (ch == 0) {
                // 左声道: 保留部分原始信号 + 混响
                wet = (1.0f - widthFactor * 0.5f) * apOut;
            } else {
                // 右声道: 使用略不同的延迟自然产生立体声宽度
                // 宽度参数控制左右声道的差异
                wet = (1.0f - widthFactor * 0.3f) * apOut;
            }

            // =============================================================
            // 6. 干湿混合
            // =============================================================
            out[i] = AudioUtils::lerp(dry, wet, mix_);
        }
    }
}

// =============================================================================
// 参数设置
// =============================================================================
void Reverb::setRoomSize(float value) {
    roomSize_ = AudioUtils::clamp(value, 0.0f, 1.0f);
    updateInternalParams();
}

void Reverb::setDecay(float value) {
    decay_ = AudioUtils::clamp(value, 0.0f, 1.0f);
    updateInternalParams();
}

void Reverb::setDamping(float value) {
    damping_ = AudioUtils::clamp(value, 0.0f, 1.0f);
    updateInternalParams();
}

void Reverb::setMix(float value) {
    mix_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

void Reverb::setPredelay(float value) {
    predelay_ = AudioUtils::clamp(value, 0.0f, 1.0f);
    updateInternalParams();
}

void Reverb::setWidth(float value) {
    width_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

void Reverb::setEarlyReflections(float value) {
    earlyReflections_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

// =============================================================================
// 参数接口
// =============================================================================
float Reverb::getParameter(int index) const {
    switch (index) {
        case 0: return roomSize_;
        case 1: return decay_;
        case 2: return damping_;
        case 3: return mix_;
        case 4: return predelay_;
        case 5: return width_;
        case 6: return earlyReflections_;
        default: return 0.0f;
    }
}

void Reverb::setParameter(int index, float value) {
    switch (index) {
        case 0: setRoomSize(value); break;
        case 1: setDecay(value); break;
        case 2: setDamping(value); break;
        case 3: setMix(value); break;
        case 4: setPredelay(value); break;
        case 5: setWidth(value); break;
        case 6: setEarlyReflections(value); break;
        default: break;
    }
}

juce::String Reverb::getParameterName(int index) const {
    switch (index) {
        case 0: return "房间大小";
        case 1: return "衰减时间";
        case 2: return "阻尼";
        case 3: return "干湿比";
        case 4: return "预延迟";
        case 5: return "立体声宽度";
        case 6: return "早期反射";
        default: return "未知";
    }
}

// =============================================================================
// 序列化
// =============================================================================
juce::var Reverb::toJson() const {
    auto json = AudioNode::toJson();
    if (auto* obj = json.getDynamicObject()) {
        obj->setProperty("roomSize", roomSize_);
        obj->setProperty("decay", decay_);
        obj->setProperty("damping", damping_);
        obj->setProperty("mix", mix_);
        obj->setProperty("predelay", predelay_);
        obj->setProperty("width", width_);
        obj->setProperty("earlyReflections", earlyReflections_);
    }
    return json;
}

void Reverb::fromJson(const juce::var& json) {
    AudioNode::fromJson(json);
    if (auto* obj = json.getDynamicObject()) {
        if (obj->hasProperty("roomSize")) setRoomSize(obj->getProperty("roomSize"));
        if (obj->hasProperty("decay")) setDecay(obj->getProperty("decay"));
        if (obj->hasProperty("damping")) setDamping(obj->getProperty("damping"));
        if (obj->hasProperty("mix")) setMix(obj->getProperty("mix"));
        if (obj->hasProperty("predelay")) setPredelay(obj->getProperty("predelay"));
        if (obj->hasProperty("width")) setWidth(obj->getProperty("width"));
        if (obj->hasProperty("earlyReflections")) setEarlyReflections(obj->getProperty("earlyReflections"));
    }
}

} // namespace LianCore