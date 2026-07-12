// =============================================================================
// LianCore - Distortion 实现
// =============================================================================
#include "Distortion.h"
#include "../utils/AudioUtils.h"

namespace LianCore {

// =============================================================================
// 构造/析构
// =============================================================================
Distortion::Distortion(const juce::String& name)
    : AudioNode(NodeType::Distortion, name) {}

// =============================================================================
// 生命周期
// =============================================================================
void Distortion::prepareToPlay(double sampleRate, int maxSamplesPerBlock) {
    AudioNode::prepareToPlay(sampleRate, maxSamplesPerBlock);
    sampleRate_ = sampleRate;

    // 重置比特粉碎状态
    bitcrushHold_ = 0.0f;
    bitcrushCounter_ = 0;
    bitcrushStep_ = 1;

    // 重置音色滤波器
    toneLP_[0] = 0.0f;
    toneLP_[1] = 0.0f;
}

void Distortion::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    auto& input = getInputBuffer(0);
    auto& output = getOutputBuffer(0);

    int numSamples = buffer.getNumSamples();
    float driveGain = 1.0f + drive_ * 40.0f;          // drive: 0→1 映射到 1→41倍增益
    float outputGain = output_ * 2.0f;                  // output: 0→1 映射到 0→2倍
    float biasShift = (bias_ - 0.5f) * 2.0f;            // bias: 0→1 映射到 -1→1

    // 音色滤波器系数 (一阶低通, 20Hz → 20kHz)
    float toneFreq = 20.0f * std::pow(1000.0f, tone_);
    float toneAlpha = toneFreq / (toneFreq + static_cast<float>(sampleRate_));
    toneAlpha = AudioUtils::clamp(toneAlpha, 0.0f, 1.0f);

    // 比特粉碎参数计算
    if (type_ == DistortionType::Bitcrush) {
        float crushAmount = drive_ * 0.99f + 0.01f;
        bitcrushStep_ = std::max(1, static_cast<int>(16.0f + (1.0f - crushAmount) * 440.0f));
    }

    for (int ch = 0; ch < 2; ++ch) {
        const float* in = input.getReadPointer(ch);
        float* out = output.getWritePointer(ch);

        for (int i = 0; i < numSamples; ++i) {
            float x = in[i];

            // 直流偏置
            x += biasShift;

            // 根据失真类型处理
            float wet;
            switch (type_) {
                case DistortionType::SoftClip:  wet = processSoftClip(x);  break;
                case DistortionType::HardClip:  wet = processHardClip(x);  break;
                case DistortionType::Tube:      wet = processTube(x);      break;
                case DistortionType::Fuzz:      wet = processFuzz(x);      break;
                case DistortionType::Bitcrush:  wet = processBitcrush(x);  break;
                case DistortionType::Foldback:  wet = processFoldback(x);  break;
                default:                        wet = x;                   break;
            }

            // 输出增益
            wet *= outputGain;

            // 音色滤波 (后置一阶低通)
            toneLP_[ch] += toneAlpha * (wet - toneLP_[ch]);
            wet = toneLP_[ch];

            // 去偏置 (恢复DC偏移)
            wet -= biasShift * outputGain;

            // Dry/Wet混合
            out[i] = AudioUtils::lerp(x - biasShift, wet, mix_);
        }
    }
}

void Distortion::releaseResources() {
    AudioNode::releaseResources();
}

// =============================================================================
// 失真处理函数
// =============================================================================

// 软限幅: tanh饱和
float Distortion::processSoftClip(float x) const {
    float gain = 1.0f + drive_ * 40.0f;
    return std::tanh(x * gain);
}

// 硬限幅: 在±1.0处截断
float Distortion::processHardClip(float x) const {
    float gain = 1.0f + drive_ * 20.0f;
    float y = x * gain;
    return AudioUtils::clamp(y, -1.0f, 1.0f);
}

// 电子管: 非对称tanh产生偶次谐波
float Distortion::processTube(float x) const {
    float gain = 1.0f + drive_ * 30.0f;
    float sg = x * gain;
    // 非对称处理: 正半周和负半周使用不同的饱和曲线
    float asym = std::tanh(sg) * 0.8f + std::tanh(sg * 0.4f) * 0.2f;
    // 加入少量直流偏移模拟电子管特性
    return asym + 0.05f * drive_;
}

// 法兹: 极高增益+硬限幅
float Distortion::processFuzz(float x) const {
    float gain = 1.0f + drive_ * 80.0f;
    float y = x * gain;
    return AudioUtils::clamp(y, -1.0f, 1.0f);
}

// 比特粉碎: 量化+降采样率
float Distortion::processBitcrush(float x) {
    float gain = 1.0f + drive_ * 10.0f;
    float y = x * gain;

    // 降采样率 (sample & hold)
    bitcrushCounter_++;
    if (bitcrushCounter_ >= bitcrushStep_) {
        bitcrushCounter_ = 0;
        bitcrushHold_ = y;
    }

    // 量化 (bit reduction)
    float bits = 2.0f + (1.0f - drive_) * 14.0f; // 2-16 bits
    float levels = std::pow(2.0f, bits);
    float q = std::round(bitcrushHold_ * levels) / levels;

    return q;
}

// 折返失真: 超过阈值后反射信号
float Distortion::processFoldback(float x) const {
    float gain = 1.0f + drive_ * 30.0f;
    float threshold = 1.0f - drive_ * 0.9f; // 阈值随drive降低
    threshold = AudioUtils::clamp(threshold, 0.1f, 1.0f);

    float y = x * gain;
    // Foldback算法: 当|y|>threshold时, 反射折返
    while (y > threshold || y < -threshold) {
        if (y > threshold) {
            y = threshold - (y - threshold);
        }
        if (y < -threshold) {
            y = -threshold - (y + threshold);
        }
    }
    return y;
}

// =============================================================================
// 参数设置
// =============================================================================
void Distortion::setDrive(float value) {
    drive_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

void Distortion::setType(DistortionType type) {
    type_ = type;
    // 切换类型时重置比特粉碎状态
    if (type == DistortionType::Bitcrush) {
        bitcrushCounter_ = 0;
        bitcrushHold_ = 0.0f;
    }
}

void Distortion::setOutput(float value) {
    output_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

void Distortion::setMix(float value) {
    mix_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

void Distortion::setBias(float value) {
    bias_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

void Distortion::setTone(float value) {
    tone_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

// =============================================================================
// 参数接口
// =============================================================================
float Distortion::getParameter(int index) const {
    switch (index) {
        case 0: return drive_;
        case 1: return static_cast<float>(type_) / 5.0f;
        case 2: return output_;
        case 3: return mix_;
        case 4: return bias_;
        case 5: return tone_;
        default: return 0.0f;
    }
}

void Distortion::setParameter(int index, float value) {
    switch (index) {
        case 0: setDrive(value); break;
        case 1: setType(static_cast<DistortionType>(static_cast<int>(value * 5.0f + 0.5f))); break;
        case 2: setOutput(value); break;
        case 3: setMix(value); break;
        case 4: setBias(value); break;
        case 5: setTone(value); break;
        default: break;
    }
}

juce::String Distortion::getParameterName(int index) const {
    switch (index) {
        case 0: return "驱动量";
        case 1: return "失真类型";
        case 2: return "输出音量";
        case 3: return "干湿比";
        case 4: return "直流偏置";
        case 5: return "音色";
        default: return "未知";
    }
}

// =============================================================================
// 序列化
// =============================================================================
juce::var Distortion::toJson() const {
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

    return juce::var(obj.get());
}

void Distortion::fromJson(const juce::var& json) {
    if (auto* obj = json.getDynamicObject()) {
        name_ = obj->getProperty("name").toString();
        if (auto* params = obj->getProperty("parameters").getArray()) {
            for (int i = 0; i < params->size() && i < getNumParameters(); ++i) {
                if (auto* paramObj = (*params)[i].getDynamicObject()) {
                    setParameter(i, paramObj->getProperty("value"));
                }
            }
        }
    }
}

} // namespace LianCore