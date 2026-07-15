// =============================================================================
// LianCore - Chorus 实现
// =============================================================================
#include "Chorus.h"
#include "../utils/AudioUtils.h"

namespace LianCore {

Chorus::Chorus(const juce::String& name)
    : AudioNode(NodeType::Chorus, name) {}

void Chorus::prepareToPlay(double sampleRate, int maxSamplesPerBlock) {
    AudioNode::prepareToPlay(sampleRate, maxSamplesPerBlock);
    sampleRate_ = sampleRate;

    for (int ch = 0; ch < 2; ++ch) {
        for (int v = 0; v < kMaxVoices; ++v) {
            voiceStates_[ch][v].delayLine.assign(kMaxDelaySamples, 0.0f);
            voiceStates_[ch][v].writePos = 0;
            voiceStates_[ch][v].lfoPhase = static_cast<float>(v) * 0.25f; // 相位偏移
            voiceStates_[ch][v].lfoPhaseOffset = static_cast<float>(v) * 0.25f;
        }
    }
}

void Chorus::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    auto& input = getInputBuffer(0);
    auto& output = getOutputBuffer(0);

    int numSamples = buffer.getNumSamples();

    // LFO频率: 0.1Hz - 5Hz
    float lfoFreq = 0.1f + rate_ * 4.9f;
    float lfoIncrement = lfoFreq / static_cast<float>(sampleRate_);

    // 调制深度: 0.5ms - 15ms
    float modDepthSamples = (0.5f + depth_ * 14.5f) * 0.001f * static_cast<float>(sampleRate_);

    // 基础延迟 (中心): 10ms
    float baseDelaySamples = 10.0f * 0.001f * static_cast<float>(sampleRate_);

    for (int i = 0; i < numSamples; ++i) {
        for (int ch = 0; ch < 2; ++ch) {
            float dry = input.getReadPointer(ch)[i];
            float wetSum = 0.0f;

            for (int v = 0; v < voices_; ++v) {
                auto& voice = voiceStates_[ch][v];

                // LFO正弦调制
                float lfo = std::sin(static_cast<float>(AudioUtils::kTwoPI) * voice.lfoPhase);
                float delaySamples = baseDelaySamples + lfo * modDepthSamples;
                int delayInt = static_cast<int>(delaySamples);
                delayInt = AudioUtils::clamp(delayInt, 1, kMaxDelaySamples - 1);

                // 从延迟线读取
                int readPos = voice.writePos - delayInt;
                if (readPos < 0) readPos += kMaxDelaySamples;
                float delayed = voice.delayLine[static_cast<size_t>(readPos)];

                // 写入延迟线
                voice.delayLine[static_cast<size_t>(voice.writePos)] = dry;
                voice.writePos++;
                if (voice.writePos >= kMaxDelaySamples) voice.writePos = 0;

                wetSum += delayed;

                // 推进LFO相位
                voice.lfoPhase += lfoIncrement;
                if (voice.lfoPhase >= 1.0f) voice.lfoPhase -= 1.0f;
            }

            float wet = wetSum / static_cast<float>(voices_);
            output.getWritePointer(ch)[i] = AudioUtils::lerp(dry, wet, mix_);
        }
    }
}

void Chorus::releaseResources() {
    AudioNode::releaseResources();
    for (int ch = 0; ch < 2; ++ch) {
        for (int v = 0; v < kMaxVoices; ++v) {
            voiceStates_[ch][v].delayLine.clear();
        }
    }
}

void Chorus::setRate(float value) {
    rate_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

void Chorus::setDepth(float value) {
    depth_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

void Chorus::setMix(float value) {
    mix_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

void Chorus::setVoices(int count) {
    voices_ = AudioUtils::clamp(count, 1, kMaxVoices);
}

float Chorus::getParameter(int index) const {
    switch (index) {
        case 0: return rate_;
        case 1: return depth_;
        case 2: return mix_;
        case 3: return (static_cast<float>(voices_) - 1.0f) / static_cast<float>(kMaxVoices - 1);
        default: return 0.0f;
    }
}

void Chorus::setParameter(int index, float value) {
    switch (index) {
        case 0: setRate(value); break;
        case 1: setDepth(value); break;
        case 2: setMix(value); break;
        case 3: setVoices(1 + static_cast<int>(value * static_cast<float>(kMaxVoices - 1) + 0.5f)); break;
        default: break;
    }
}

juce::String Chorus::getParameterName(int index) const {
    switch (index) {
        case 0: return "速率";
        case 1: return "深度";
        case 2: return "干湿比";
        case 3: return "合声数";
        default: return "未知";
    }
}

juce::var Chorus::toJson() const {
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
    obj->setProperty("voices", voices_);
    return juce::var(obj.get());
}

void Chorus::fromJson(const juce::var& json) {
    if (auto* obj = json.getDynamicObject()) {
        name_ = obj->getProperty("name").toString();
        if (auto* params = obj->getProperty("parameters").getArray()) {
            for (int i = 0; i < params->size() && i < getNumParameters(); ++i) {
                if (auto* paramObj = (*params)[i].getDynamicObject()) {
                    setParameter(i, paramObj->getProperty("value"));
                }
            }
        }
        voices_ = static_cast<int>(obj->getProperty("voices"));
    }
}

} // namespace LianCore