// =============================================================================
// LianCore - EQ 多段参数均衡器 实现
// 8段串行双二阶滤波器，RBJ系数公式，转置直接II型结构
// =============================================================================
#include "EQ.h"
#include "../utils/AudioUtils.h"

namespace LianCore {

// =============================================================================
// 构造与析构
// =============================================================================
EQ::EQ(const juce::String& name)
    : AudioNode(NodeType::EQ, name)
{
    // 设置默认频段配置
    // 频段0: 低频搁架 @ 80Hz
    bands_[0].type = EQBandType::LowShelf;
    bands_[0].frequency = 80.0f;
    bands_[0].gain = 0.0f;
    bands_[0].q = 0.7f;
    bands_[0].bypass = false;

    // 频段1: 峰值 @ 250Hz
    bands_[1].type = EQBandType::Peaking;
    bands_[1].frequency = 250.0f;
    bands_[1].gain = 0.0f;
    bands_[1].q = 1.0f;
    bands_[1].bypass = false;

    // 频段2: 峰值 @ 500Hz
    bands_[2].type = EQBandType::Peaking;
    bands_[2].frequency = 500.0f;
    bands_[2].gain = 0.0f;
    bands_[2].q = 1.0f;
    bands_[2].bypass = false;

    // 频段3: 峰值 @ 1000Hz
    bands_[3].type = EQBandType::Peaking;
    bands_[3].frequency = 1000.0f;
    bands_[3].gain = 0.0f;
    bands_[3].q = 1.0f;
    bands_[3].bypass = false;

    // 频段4: 峰值 @ 2000Hz
    bands_[4].type = EQBandType::Peaking;
    bands_[4].frequency = 2000.0f;
    bands_[4].gain = 0.0f;
    bands_[4].q = 1.0f;
    bands_[4].bypass = false;

    // 频段5: 峰值 @ 4000Hz
    bands_[5].type = EQBandType::Peaking;
    bands_[5].frequency = 4000.0f;
    bands_[5].gain = 0.0f;
    bands_[5].q = 1.0f;
    bands_[5].bypass = false;

    // 频段6: 峰值 @ 8000Hz
    bands_[6].type = EQBandType::Peaking;
    bands_[6].frequency = 8000.0f;
    bands_[6].gain = 0.0f;
    bands_[6].q = 1.0f;
    bands_[6].bypass = false;

    // 频段7: 高频搁架 @ 12000Hz
    bands_[7].type = EQBandType::HighShelf;
    bands_[7].frequency = 12000.0f;
    bands_[7].gain = 0.0f;
    bands_[7].q = 0.7f;
    bands_[7].bypass = false;
}

// =============================================================================
// 生命周期
// =============================================================================
void EQ::prepareToPlay(double sampleRate, int maxSamplesPerBlock) {
    AudioNode::prepareToPlay(sampleRate, maxSamplesPerBlock);
    sampleRate_ = sampleRate;

    // 预计算所有频段的滤波器系数
    for (int band = 0; band < 8; ++band) {
        updateCoefficients(band);
    }

    // 清除所有频段的状态
    for (auto& band : bands_) {
        for (auto& ch : band.channels) {
            ch.w1 = 0.0f;
            ch.w2 = 0.0f;
        }
    }
}

void EQ::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    auto& input = getInputBuffer(0);
    auto& output = getOutputBuffer(0);

    int numSamples = buffer.getNumSamples();

    // 串行处理: 输入 -> 频段0 -> 频段1 -> ... -> 频段7 -> 输出增益 & 干湿比
    for (int ch = 0; ch < 2; ++ch) {
        const float* in = input.getReadPointer(ch);
        float* out = output.getWritePointer(ch);

        for (int i = 0; i < numSamples; ++i) {
            float x = in[i];
            float y = x;

            // 串行通过所有8个频段
            for (int band = 0; band < 8; ++band) {
                const auto& bandState = bands_[band];

                // 旁路跳过
                if (bandState.bypass) {
                    continue;
                }

                auto& state = bands_[band].channels[ch];

                // 转置直接II型双二阶滤波器
                // y[n] = b0*x[n] + w1[n-1]
                // w1[n] = b1*x[n] - a1*y[n] + w2[n-1]
                // w2[n] = b2*x[n] - a2*y[n]
                float filtered = state.b0 * y + state.w1;
                state.w1 = state.b1 * y - state.a1 * filtered + state.w2;
                state.w2 = state.b2 * y - state.a2 * filtered;

                y = filtered;
            }

            // 输出增益
            y *= outputGain_;

            // 干湿比混合
            out[i] = AudioUtils::lerp(x, y, mix_);
        }
    }
}

void EQ::releaseResources() {
    // 清除所有频段的状态
    for (auto& band : bands_) {
        for (auto& ch : band.channels) {
            ch.w1 = 0.0f;
            ch.w2 = 0.0f;
        }
    }

    AudioNode::releaseResources();
}

// =============================================================================
// 频率转换辅助方法
// =============================================================================

// 归一化值 [0, 1] -> 频率 [20Hz, 20000Hz] 对数刻度
float EQ::frequencyToHz(float normalized) const {
    static const float kMinFreq = 20.0f;
    static const float kMaxFreq = 20000.0f;
    static const float kLogRange = std::log(kMaxFreq / kMinFreq); // ln(1000)

    float clamped = AudioUtils::clamp(normalized, 0.0f, 1.0f);
    return kMinFreq * std::exp(clamped * kLogRange);
}

// 频率 [20Hz, 20000Hz] -> 归一化值 [0, 1] 对数刻度
float EQ::hzToFrequency(float hz) const {
    static const float kMinFreq = 20.0f;
    static const float kMaxFreq = 20000.0f;
    static const float kLogRange = std::log(kMaxFreq / kMinFreq); // ln(1000)

    float clamped = AudioUtils::clamp(hz, kMinFreq, kMaxFreq);
    return std::log(clamped / kMinFreq) / kLogRange;
}

// =============================================================================
// 双二阶滤波器系数计算 (RBJ Audio EQ Cookbook 公式)
// =============================================================================
void EQ::updateCoefficients(int band) {
    if (band < 0 || band >= 8) return;

    auto& bandState = bands_[band];
    float freq = AudioUtils::clamp(bandState.frequency, 20.0f,
                                   static_cast<float>(sampleRate_ * 0.49));
    float q = AudioUtils::clamp(bandState.q, 0.1f, 10.0f);
    float gainDb = AudioUtils::clamp(bandState.gain, -18.0f, 18.0f);

    float omega = static_cast<float>(AudioUtils::kTwoPI) * freq / static_cast<float>(sampleRate_);
    float sinW = std::sin(omega);
    float cosW = std::cos(omega);
    float alpha = sinW / (2.0f * q);

    float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
    float a0 = 1.0f, a1 = 0.0f, a2 = 0.0f;

    switch (bandState.type) {
        // =====================================================================
        // 峰值/钟形滤波器 (Peaking / Bell)
        // A = 10^(gain/40), alpha = sinW/(2*Q)
        // b0 = 1 + alpha*A, b1 = -2*cosW, b2 = 1 - alpha*A
        // a0 = 1 + alpha/A, a1 = -2*cosW, a2 = 1 - alpha/A
        // =====================================================================
        case EQBandType::Peaking: {
            float A = std::pow(10.0f, gainDb / 40.0f);
            float alphaA = alpha * A;
            b0 = 1.0f + alphaA;
            b1 = -2.0f * cosW;
            b2 = 1.0f - alphaA;
            a0 = 1.0f + alpha / A;
            a1 = -2.0f * cosW;
            a2 = 1.0f - alpha / A;
            break;
        }

        // =====================================================================
        // 低频搁架滤波器 (Low Shelf)
        // A = 10^(gain/40), beta = 2*sqrt(A)*alpha
        // b0 = A*((A+1) - (A-1)*cosW + beta)
        // b1 = 2*A*((A-1) - (A+1)*cosW)
        // b2 = A*((A+1) - (A-1)*cosW - beta)
        // a0 = (A+1) + (A-1)*cosW + beta
        // a1 = -2*((A-1) + (A+1)*cosW)
        // a2 = (A+1) + (A-1)*cosW - beta
        // =====================================================================
        case EQBandType::LowShelf: {
            float A = std::pow(10.0f, gainDb / 40.0f);
            float beta = 2.0f * std::sqrt(A) * alpha;
            float Ap1 = A + 1.0f;
            float Am1 = A - 1.0f;
            float Am1_cosW = Am1 * cosW;
            float Ap1_cosW = Ap1 * cosW;

            b0 = A * (Ap1 - Am1_cosW + beta);
            b1 = 2.0f * A * (Am1 - Ap1_cosW);
            b2 = A * (Ap1 - Am1_cosW - beta);
            a0 = Ap1 + Am1_cosW + beta;
            a1 = -2.0f * (Am1 + Ap1_cosW);
            a2 = Ap1 + Am1_cosW - beta;
            break;
        }

        // =====================================================================
        // 高频搁架滤波器 (High Shelf)
        // A = 10^(gain/40), beta = 2*sqrt(A)*alpha
        // b0 = A*((A+1) + (A-1)*cosW + beta)
        // b1 = -2*A*((A-1) + (A+1)*cosW)
        // b2 = A*((A+1) + (A-1)*cosW - beta)
        // a0 = (A+1) - (A-1)*cosW + beta
        // a1 = 2*((A-1) - (A+1)*cosW)
        // a2 = (A+1) - (A-1)*cosW - beta
        // =====================================================================
        case EQBandType::HighShelf: {
            float A = std::pow(10.0f, gainDb / 40.0f);
            float beta = 2.0f * std::sqrt(A) * alpha;
            float Ap1 = A + 1.0f;
            float Am1 = A - 1.0f;
            float Am1_cosW = Am1 * cosW;
            float Ap1_cosW = Ap1 * cosW;

            b0 = A * (Ap1 + Am1_cosW + beta);
            b1 = -2.0f * A * (Am1 + Ap1_cosW);
            b2 = A * (Ap1 + Am1_cosW - beta);
            a0 = Ap1 - Am1_cosW + beta;
            a1 = 2.0f * (Am1 - Ap1_cosW);
            a2 = Ap1 - Am1_cosW - beta;
            break;
        }

        // =====================================================================
        // 低通滤波器 12dB/oct (Butterworth)
        // b0 = (1-cosW)/2, b1 = 1-cosW, b2 = (1-cosW)/2
        // a0 = 1+alpha, a1 = -2*cosW, a2 = 1-alpha
        // =====================================================================
        case EQBandType::LowPass: {
            float oneMinusCosW = 1.0f - cosW;
            b0 = oneMinusCosW / 2.0f;
            b1 = oneMinusCosW;
            b2 = oneMinusCosW / 2.0f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cosW;
            a2 = 1.0f - alpha;
            break;
        }

        // =====================================================================
        // 高通滤波器 12dB/oct (Butterworth)
        // b0 = (1+cosW)/2, b1 = -(1+cosW), b2 = (1+cosW)/2
        // a0 = 1+alpha, a1 = -2*cosW, a2 = 1-alpha
        // =====================================================================
        case EQBandType::HighPass: {
            float onePlusCosW = 1.0f + cosW;
            b0 = onePlusCosW / 2.0f;
            b1 = -onePlusCosW;
            b2 = onePlusCosW / 2.0f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cosW;
            a2 = 1.0f - alpha;
            break;
        }

        // =====================================================================
        // 陷波滤波器 (Notch / 窄带带阻)
        // b0 = 1, b1 = -2*cosW, b2 = 1
        // a0 = 1+alpha, a1 = -2*cosW, a2 = 1-alpha
        // =====================================================================
        case EQBandType::Notch: {
            b0 = 1.0f;
            b1 = -2.0f * cosW;
            b2 = 1.0f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cosW;
            a2 = 1.0f - alpha;
            break;
        }

        // =====================================================================
        // 带通滤波器 (恒定裙边增益, 峰值增益 = Q)
        // b0 = alpha, b1 = 0, b2 = -alpha
        // a0 = 1+alpha, a1 = -2*cosW, a2 = 1-alpha
        // =====================================================================
        case EQBandType::BandPass: {
            b0 = alpha;
            b1 = 0.0f;
            b2 = -alpha;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cosW;
            a2 = 1.0f - alpha;
            break;
        }
    }

    // 归一化系数 (除以 a0)
    float invA0 = 1.0f / a0;
    b0 *= invA0;
    b1 *= invA0;
    b2 *= invA0;
    a1 *= invA0;
    a2 *= invA0;

    // 写入双声道系数
    for (int ch = 0; ch < 2; ++ch) {
        auto& state = bandState.channels[ch];
        state.b0 = b0;
        state.b1 = b1;
        state.b2 = b2;
        state.a1 = a1;
        state.a2 = a2;
        // 系数变化时不清除状态，保持连续性
    }
}

// =============================================================================
// 参数接口实现 (扁平参数方案)
// 42个参数: 频段0(0-4) ... 频段7(35-39) + 输出增益(40) + 干湿比(41)
// =============================================================================

float EQ::getParameter(int index) const {
    if (index >= 0 && index < 40) {
        int band = index / 5;
        int sub = index % 5;
        const auto& bs = bands_[band];

        switch (sub) {
            case 0: return static_cast<float>(bs.type) / 6.0f;     // 类型 0-5 -> 0-1
            case 1: return hzToFrequency(bs.frequency);             // 频率 归一化
            case 2: return (bs.gain + 18.0f) / 36.0f;              // 增益 -18..+18 -> 0-1
            case 3: return (bs.q - 0.1f) / 9.9f;                   // Q值 0.1-10 -> 0-1
            case 4: return bs.bypass ? 1.0f : 0.0f;                // 旁路 0/1
            default: return 0.0f;
        }
    }

    switch (index) {
        case 40: return AudioUtils::amplitudeToDb(outputGain_) / 24.0f + 0.5f; // 输出增益
        case 41: return mix_;                                                    // 干湿比
        default: return 0.0f;
    }
}

void EQ::setParameter(int index, float value) {
    if (index >= 0 && index < 40) {
        int band = index / 5;
        int sub = index % 5;
        auto& bs = bands_[band];

        switch (sub) {
            case 0: // 类型
                bs.type = static_cast<EQBandType>(
                    AudioUtils::clamp(static_cast<int>(value * 6.0f + 0.5f), 0, 6));
                break;
            case 1: // 频率
                bs.frequency = frequencyToHz(value);
                break;
            case 2: // 增益 (-18 到 +18 dB)
                bs.gain = value * 36.0f - 18.0f;
                break;
            case 3: // Q值
                bs.q = value * 9.9f + 0.1f;
                break;
            case 4: // 旁路
                bs.bypass = value >= 0.5f;
                break;
            default: break;
        }

        updateCoefficients(band);
        return;
    }

    switch (index) {
        case 40: // 输出增益 (-12 到 +12 dB)
            outputGain_ = AudioUtils::dbToAmplitude(value * 24.0f - 12.0f);
            break;
        case 41: // 干湿比
            mix_ = AudioUtils::clamp(value, 0.0f, 1.0f);
            break;
        default: break;
    }
}

juce::String EQ::getParameterName(int index) const {
    if (index >= 0 && index < 40) {
        int band = index / 5;
        int sub = index % 5;
        juce::String bandName = "频段" + juce::String(band + 1);

        switch (sub) {
            case 0: return bandName + " 类型";
            case 1: return bandName + " 频率";
            case 2: return bandName + " 增益";
            case 3: return bandName + " Q值";
            case 4: return bandName + " 旁路";
            default: return "未知";
        }
    }

    switch (index) {
        case 40: return "输出增益";
        case 41: return "干湿比";
        default: return "未知";
    }
}

juce::String EQ::getParameterText(int index) const {
    if (index >= 0 && index < 40) {
        int band = index / 5;
        int sub = index % 5;
        const auto& bs = bands_[band];

        switch (sub) {
            case 0: { // 类型名称
                switch (bs.type) {
                    case EQBandType::Peaking:   return "峰值";
                    case EQBandType::LowShelf:  return "低频搁架";
                    case EQBandType::HighShelf: return "高频搁架";
                    case EQBandType::LowPass:   return "低通";
                    case EQBandType::HighPass:  return "高通";
                    case EQBandType::Notch:     return "陷波";
                    case EQBandType::BandPass:  return "带通";
                    default: return "未知";
                }
            }
            case 1: return juce::String(static_cast<int>(bs.frequency)) + " Hz";
            case 2: return juce::String(bs.gain, 1) + " dB";
            case 3: return juce::String(bs.q, 2);
            case 4: return bs.bypass ? "旁路" : "启用";
            default: return "";
        }
    }

    switch (index) {
        case 40: return juce::String(AudioUtils::amplitudeToDb(outputGain_), 1) + " dB";
        case 41: return juce::String(static_cast<int>(mix_ * 100.0f)) + " %";
        default: return "";
    }
}

// =============================================================================
// 便捷访问接口
// =============================================================================

void EQ::setBandType(int band, EQBandType type) {
    if (band < 0 || band >= 8) return;
    bands_[band].type = type;
    updateCoefficients(band);
}

void EQ::setBandFrequency(int band, float hz) {
    if (band < 0 || band >= 8) return;
    bands_[band].frequency = AudioUtils::clamp(hz, 20.0f, 20000.0f);
    updateCoefficients(band);
}

void EQ::setBandGain(int band, float db) {
    if (band < 0 || band >= 8) return;
    bands_[band].gain = AudioUtils::clamp(db, -18.0f, 18.0f);
    updateCoefficients(band);
}

void EQ::setBandQ(int band, float q) {
    if (band < 0 || band >= 8) return;
    bands_[band].q = AudioUtils::clamp(q, 0.1f, 10.0f);
    updateCoefficients(band);
}

void EQ::setBandBypass(int band, bool bypass) {
    if (band < 0 || band >= 8) return;
    bands_[band].bypass = bypass;
}

void EQ::setOutputGain(float gain) {
    outputGain_ = AudioUtils::dbToAmplitude(AudioUtils::clamp(gain, -12.0f, 12.0f));
}

void EQ::setMix(float mix) {
    mix_ = AudioUtils::clamp(mix, 0.0f, 1.0f);
}

EQBandType EQ::getBandType(int band) const {
    if (band < 0 || band >= 8) return EQBandType::Peaking;
    return bands_[band].type;
}

float EQ::getBandFrequency(int band) const {
    if (band < 0 || band >= 8) return 1000.0f;
    return bands_[band].frequency;
}

float EQ::getBandGain(int band) const {
    if (band < 0 || band >= 8) return 0.0f;
    return bands_[band].gain;
}

float EQ::getBandQ(int band) const {
    if (band < 0 || band >= 8) return 1.0f;
    return bands_[band].q;
}

bool EQ::getBandBypass(int band) const {
    if (band < 0 || band >= 8) return false;
    return bands_[band].bypass;
}

float EQ::getOutputGain() const {
    return AudioUtils::amplitudeToDb(outputGain_);
}

float EQ::getMix() const {
    return mix_;
}

// =============================================================================
// 序列化 (toJson / fromJson)
// =============================================================================
juce::var EQ::toJson() const {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();

    obj->setProperty("nodeId", nodeId_);
    obj->setProperty("name", name_);
    obj->setProperty("nodeType", nodeTypeToString(nodeType_));

    // 序列化输出增益和干湿比
    obj->setProperty("outputGain", AudioUtils::amplitudeToDb(outputGain_));
    obj->setProperty("mix", mix_);

    // 序列化所有8个频段
    juce::Array<juce::var> bandsArray;
    for (int i = 0; i < 8; ++i) {
        const auto& bs = bands_[i];
        juce::DynamicObject::Ptr bandObj = new juce::DynamicObject();

        bandObj->setProperty("index", i);
        bandObj->setProperty("type", static_cast<int>(bs.type));
        bandObj->setProperty("frequency", bs.frequency);
        bandObj->setProperty("gain", bs.gain);
        bandObj->setProperty("q", bs.q);
        bandObj->setProperty("bypass", bs.bypass);

        bandsArray.add(juce::var(bandObj.get()));
    }
    obj->setProperty("bands", bandsArray);

    return juce::var(obj.get());
}

void EQ::fromJson(const juce::var& json) {
    if (json.isVoid()) return;

    auto* obj = json.getDynamicObject();
    if (!obj) return;

    // 恢复节点标识
    if (obj->hasProperty("nodeId"))
        nodeId_ = obj->getProperty("nodeId").toString();
    if (obj->hasProperty("name"))
        name_ = obj->getProperty("name").toString();

    // 恢复输出增益和干湿比
    if (obj->hasProperty("outputGain"))
        outputGain_ = AudioUtils::dbToAmplitude(static_cast<float>(obj->getProperty("outputGain")));
    if (obj->hasProperty("mix"))
        mix_ = static_cast<float>(obj->getProperty("mix"));

    // 恢复所有频段
    if (obj->hasProperty("bands")) {
        auto bandsArray = obj->getProperty("bands");
        if (auto* bandsArr = bandsArray.getArray()) {
            for (const auto& bandVar : *bandsArr) {
                auto* bandObj = bandVar.getDynamicObject();
                if (!bandObj) continue;

                int index = static_cast<int>(bandObj->getProperty("index"));
                if (index < 0 || index >= 8) continue;

                auto& bs = bands_[index];
                bs.type = static_cast<EQBandType>(
                    static_cast<int>(bandObj->getProperty("type")));
                bs.frequency = static_cast<float>(bandObj->getProperty("frequency"));
                bs.gain = static_cast<float>(bandObj->getProperty("gain"));
                bs.q = static_cast<float>(bandObj->getProperty("q"));
                bs.bypass = static_cast<bool>(bandObj->getProperty("bypass"));

                // 重新计算该频段的系数
                updateCoefficients(index);
            }
        }
    }
}

} // namespace LianCore