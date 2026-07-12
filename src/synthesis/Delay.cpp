// =============================================================================
// LianCore - Delay 实现
// =============================================================================
#include "Delay.h"
#include "../utils/AudioUtils.h"

namespace LianCore {

// =============================================================================
// 构造/析构
// =============================================================================
Delay::Delay(const juce::String& name)
    : AudioNode(NodeType::Delay, name) {}

// =============================================================================
// 生命周期
// =============================================================================
void Delay::prepareToPlay(double sampleRate, int maxSamplesPerBlock) {
    AudioNode::prepareToPlay(sampleRate, maxSamplesPerBlock);
    sampleRate_ = sampleRate;

    // 分配延迟线环形缓冲区
    for (int ch = 0; ch < 2; ++ch) {
        delayLine_[ch].assign(kMaxDelaySamples, 0.0f);
        writePos_[ch] = 0;
    }

    // 重置滤波器状态
    lpState_[0] = 0.0f; lpState_[1] = 0.0f;
    hpState_[0] = 0.0f; hpState_[1] = 0.0f;

    // 初始延迟时间
    delaySamples_ = static_cast<int>(computeDelayTimeSamples());
    smoothDelaySamples_ = static_cast<float>(delaySamples_);
}

void Delay::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    auto& input = getInputBuffer(0);
    auto& output = getOutputBuffer(0);

    int numSamples = buffer.getNumSamples();
    int maxDelay = kMaxDelaySamples - 1;

    // 目标延迟采样数
    float targetDelaySamples = computeDelayTimeSamples();
    // 平滑过渡系数 (20ms平滑时间, 避免咔嗒声)
    float smoothFactor = 1.0f - std::exp(-2.0f * 3.14159f * 20.0f / static_cast<float>(sampleRate_));

    // 反馈滤波器系数
    float lpCoeff = computeLPCoefficient(lowPassCutoff_);
    float hpCoeff = computeLPCoefficient(highPassCutoff_);

    for (int i = 0; i < numSamples; ++i) {
        // 平滑延迟时间
        smoothDelaySamples_ += smoothFactor * (targetDelaySamples - smoothDelaySamples_);
        delaySamples_ = static_cast<int>(smoothDelaySamples_);
        delaySamples_ = AudioUtils::clamp(delaySamples_, 1, maxDelay);

        for (int ch = 0; ch < 2; ++ch) {
            float dry = input.getReadPointer(ch)[i];

            // 读取延迟线 (环形缓冲区)
            int readPos = writePos_[ch] - delaySamples_;
            if (readPos < 0) readPos += kMaxDelaySamples;
            float delayed = delayLine_[ch][static_cast<size_t>(readPos)];

            // 写入当前输入+反馈到延迟线
            float fbInput = delayed;
            if (pingPong_) {
                // Ping-Pong: 反馈交叉到另一个声道
                int otherCh = (ch == 0) ? 1 : 0;
                int otherReadPos = writePos_[otherCh] - delaySamples_;
                if (otherReadPos < 0) otherReadPos += kMaxDelaySamples;
                fbInput = delayLine_[otherCh][static_cast<size_t>(otherReadPos)];

                // 反馈路径: 使用对侧声道延迟 + 当前输入
                float fb = fbInput * feedback_ + dry;
                // 反馈滤波
                fb = lpState_[ch] + lpCoeff * (fb - lpState_[ch]);
                lpState_[ch] = fb;
                float hpFiltered = fb - hpState_[ch];
                hpState_[ch] += hpCoeff * hpFiltered;
                fb = hpFiltered;

                delayLine_[ch][writePos_[ch]] = fb;
            } else {
                // 标准立体声延迟: 同声道反馈
                float fb = fbInput * feedback_ + dry;
                // 反馈滤波
                fb = lpState_[ch] + lpCoeff * (fb - lpState_[ch]);
                lpState_[ch] = fb;
                float hpFiltered = fb - hpState_[ch];
                hpState_[ch] += hpCoeff * hpFiltered;
                fb = hpFiltered;

                delayLine_[ch][writePos_[ch]] = fb;
            }

            // Dry/Wet混合
            float wet = delayed;
            float mixed = AudioUtils::lerp(dry, wet, mix_);

            output.getWritePointer(ch)[i] = mixed;

            // 推进写指针
            writePos_[ch]++;
            if (writePos_[ch] >= kMaxDelaySamples) {
                writePos_[ch] = 0;
            }
        }
    }
}

void Delay::releaseResources() {
    AudioNode::releaseResources();
    for (int ch = 0; ch < 2; ++ch) {
        delayLine_[ch].clear();
    }
}

// =============================================================================
// 延迟时间计算
// =============================================================================
float Delay::computeDelayTimeSamples() const {
    if (tempoSync_) {
        // 节拍同步: 根据BPM和音符时值计算延迟时间
        double beatDuration = 60.0 / bpm_; // 一拍时长(秒)

        double noteDuration;
        switch (noteDivision_) {
            case NoteDivision::Whole:           noteDuration = beatDuration * 4.0;       break; // 1/1
            case NoteDivision::Half:            noteDuration = beatDuration * 2.0;       break; // 1/2
            case NoteDivision::Quarter:         noteDuration = beatDuration;              break; // 1/4
            case NoteDivision::Eighth:          noteDuration = beatDuration * 0.5;       break; // 1/8
            case NoteDivision::Sixteenth:       noteDuration = beatDuration * 0.25;      break; // 1/16
            case NoteDivision::ThirtySecond:    noteDuration = beatDuration * 0.125;     break; // 1/32
            case NoteDivision::DottedHalf:      noteDuration = beatDuration * 3.0;       break; // 附点1/2
            case NoteDivision::DottedQuarter:   noteDuration = beatDuration * 1.5;       break; // 附点1/4
            case NoteDivision::DottedEighth:    noteDuration = beatDuration * 0.75;      break; // 附点1/8
            case NoteDivision::TripletHalf:    noteDuration = beatDuration * 4.0 / 3.0;  break; // 三连音1/2
            case NoteDivision::TripletQuarter: noteDuration = beatDuration * 2.0 / 3.0;  break; // 三连音1/4
            case NoteDivision::TripletEighth:  noteDuration = beatDuration / 3.0;        break; // 三连音1/8
            default:                            noteDuration = beatDuration;              break;
        }

        // time_参数作为微调: 0.5=标准时值, 0→0.5倍, 1→2.0倍
        float fineTune = 0.5f + time_; // 0.5 → 1.5
        return static_cast<float>(noteDuration * fineTune * sampleRate_);
    } else {
        // 自由模式: time_ 0→1 映射到 1ms→2000ms
        float delayMs = 1.0f + time_ * 1999.0f;
        return delayMs * 0.001f * static_cast<float>(sampleRate_);
    }
}

// =============================================================================
// 一阶滤波器系数计算
// =============================================================================
float Delay::computeLPCoefficient(float cutoffNorm) const {
    // cutoffNorm: 0→1 映射到 20Hz→20000Hz
    float freq = 20.0f * std::pow(1000.0f, cutoffNorm);
    float dt = 1.0f / static_cast<float>(sampleRate_);
    float rc = 1.0f / (2.0f * 3.14159f * freq);
    return dt / (rc + dt);
}

// =============================================================================
// 参数设置
// =============================================================================
void Delay::setTime(float value) {
    time_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

void Delay::setFeedback(float value) {
    feedback_ = AudioUtils::clamp(value, 0.0f, 0.95f);
}

void Delay::setMix(float value) {
    mix_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

void Delay::setPingPong(bool enabled) {
    pingPong_ = enabled;
}

void Delay::setLowPassCutoff(float value) {
    lowPassCutoff_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

void Delay::setHighPassCutoff(float value) {
    highPassCutoff_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

void Delay::setTempoSync(bool enabled) {
    tempoSync_ = enabled;
}

void Delay::setNoteDivision(NoteDivision div) {
    noteDivision_ = div;
}

void Delay::setBPM(double bpm) {
    bpm_ = std::max(1.0, bpm);
}

// =============================================================================
// 参数接口
// =============================================================================
float Delay::getParameter(int index) const {
    switch (index) {
        case 0: return time_;
        case 1: return feedback_ / 0.95f;
        case 2: return mix_;
        case 3: return pingPong_ ? 1.0f : 0.0f;
        case 4: return lowPassCutoff_;
        case 5: return highPassCutoff_;
        case 6: return tempoSync_ ? 1.0f : 0.0f;
        default: return 0.0f;
    }
}

void Delay::setParameter(int index, float value) {
    switch (index) {
        case 0: setTime(value); break;
        case 1: setFeedback(value * 0.95f); break;
        case 2: setMix(value); break;
        case 3: setPingPong(value >= 0.5f); break;
        case 4: setLowPassCutoff(value); break;
        case 5: setHighPassCutoff(value); break;
        case 6: setTempoSync(value >= 0.5f); break;
        default: break;
    }
}

juce::String Delay::getParameterName(int index) const {
    switch (index) {
        case 0: return "延迟时间";
        case 1: return "反馈";
        case 2: return "干湿比";
        case 3: return "Ping-Pong";
        case 4: return "低通截止";
        case 5: return "高通截止";
        case 6: return "节拍同步";
        default: return "未知";
    }
}

// =============================================================================
// 序列化
// =============================================================================
juce::var Delay::toJson() const {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("nodeId", nodeId_);
    obj->setProperty("nodeType", nodeTypeToString(nodeType_));
    obj->setProperty("name", name_);

    // 序列化参数
    juce::Array<juce::var> params;
    for (int i = 0; i < getNumParameters(); ++i) {
        juce::DynamicObject::Ptr paramObj = new juce::DynamicObject();
        paramObj->setProperty("name", getParameterName(i));
        paramObj->setProperty("value", getParameter(i));
        params.add(paramObj.get());
    }
    obj->setProperty("parameters", params);

    // 序列化额外状态 (BPM, 音符时值)
    obj->setProperty("bpm", bpm_);
    obj->setProperty("noteDivision", static_cast<int>(noteDivision_));

    return juce::var(obj.get());
}

void Delay::fromJson(const juce::var& json) {
    if (auto* obj = json.getDynamicObject()) {
        name_ = obj->getProperty("name").toString();
        if (auto* params = obj->getProperty("parameters").getArray()) {
            for (int i = 0; i < params->size() && i < getNumParameters(); ++i) {
                if (auto* paramObj = (*params)[i].getDynamicObject()) {
                    setParameter(i, paramObj->getProperty("value"));
                }
            }
        }
        // 恢复额外状态
        bpm_ = obj->getProperty("bpm");
        noteDivision_ = static_cast<NoteDivision>(static_cast<int>(obj->getProperty("noteDivision")));
    }
}

} // namespace LianCore