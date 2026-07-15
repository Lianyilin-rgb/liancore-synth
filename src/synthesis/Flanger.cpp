// =============================================================================
// LianCore - Flanger 实现
// =============================================================================
#include "Flanger.h"
#include "../utils/AudioUtils.h"

namespace LianCore {

Flanger::Flanger(const juce::String& name)
    : AudioNode(NodeType::Flanger, name) {}

void Flanger::prepareToPlay(double sampleRate, int maxSamplesPerBlock) {
    AudioNode::prepareToPlay(sampleRate, maxSamplesPerBlock);
    sampleRate_ = sampleRate;

    for (int ch = 0; ch < 2; ++ch) {
        channels_[ch].delayLine.assign(kMaxDelaySamples, 0.0f);
        channels_[ch].writePos = 0;
        channels_[ch].lfoPhase = 0.0f;
    }
}

void Flanger::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    auto& input = getInputBuffer(0);
    auto& output = getOutputBuffer(0);

    int numSamples = buffer.getNumSamples();

    // LFO频率: 0.05Hz - 5Hz
    float lfoFreq = 0.05f + rate_ * 4.95f;
    float lfoIncrement = lfoFreq / static_cast<float>(sampleRate_);

    // 调制深度: 0.1ms - 5ms
    float modDepthSamples = (0.1f + depth_ * 4.9f) * 0.001f * static_cast<float>(sampleRate_);

    // 基础延迟 (中心): 2ms
    float baseDelaySamples = 2.0f * 0.001f * static_cast<float>(sampleRate_);

    for (int i = 0; i < numSamples; ++i) {
        for (int ch = 0; ch < 2; ++ch) {
            auto& chan = channels_[ch];
            float dry = input.getReadPointer(ch)[i];

            // LFO正弦调制
            float lfo = std::sin(static_cast<float>(AudioUtils::kTwoPI) * chan.lfoPhase);
            float delaySamples = baseDelaySamples + lfo * modDepthSamples;
            int delayInt = static_cast<int>(delaySamples);
            delayInt = AudioUtils::clamp(delayInt, 1, kMaxDelaySamples - 1);

            // 从延迟线读取
            int readPos = chan.writePos - delayInt;
            if (readPos < 0) readPos += kMaxDelaySamples;
            float delayed = chan.delayLine[static_cast<size_t>(readPos)];

            // 写入延迟线 (输入 + 反馈)
            float fbInput = dry + delayed * feedback_;
            chan.delayLine[static_cast<size_t>(chan.writePos)] = fbInput;
            chan.writePos++;
            if (chan.writePos >= kMaxDelaySamples) chan.writePos = 0;

            output.getWritePointer(ch)[i] = AudioUtils::lerp(dry, delayed, mix_);

            // 推进LFO相位
            chan.lfoPhase += lfoIncrement;
            if (chan.lfoPhase >= 1.0f) chan.lfoPhase -= 1.0f;
        }
    }
}

void Flanger::releaseResources() {
    AudioNode::releaseResources();
    for (int ch = 0; ch < 2; ++ch) {
        channels_[ch].delayLine.clear();
    }
}

void Flanger::setRate(float value) {
    rate_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

void Flanger::setDepth(float value) {
    depth_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

void Flanger::setFeedback(float value) {
    feedback_ = AudioUtils::clamp(value, 0.0f, 0.95f);
}

void Flanger::setMix(float value) {
    mix_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

float Flanger::getParameter(int index) const {
    switch (index) {
        case 0: return rate_;
        case 1: return depth_;
        case 2: return feedback_ / 0.95f;
        case 3: return mix_;
        default: return 0.0f;
    }
}

void Flanger::setParameter(int index, float value) {
    switch (index) {
        case 0: setRate(value); break;
        case 1: setDepth(value); break;
        case 2: setFeedback(value * 0.95f); break;
        case 3: setMix(value); break;
        default: break;
    }
}

juce::String Flanger::getParameterName(int index) const {
    switch (index) {
        case 0: return "速率";
        case 1: return "深度";
        case 2: return "反馈";
        case 3: return "干湿比";
        default: return "未知";
    }
}

juce::var Flanger::toJson() const {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("nodeId", nodeId_);
    obj->setProperty("nodeType", nodeTypeToString(nodeType_));
    obj->setProperty("name", name_);
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

void Flanger::fromJson(const juce::var& json) {
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