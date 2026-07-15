// =============================================================================
// LianCore - RingMod 实现
// =============================================================================
#include "RingMod.h"
#include "../utils/AudioUtils.h"

namespace LianCore {

RingMod::RingMod(const juce::String& name)
    : AudioNode(NodeType::RingMod, name) {}

void RingMod::prepareToPlay(double sampleRate, int maxSamplesPerBlock) {
    AudioNode::prepareToPlay(sampleRate, maxSamplesPerBlock);
    sampleRate_ = sampleRate;
    carrierPhase_ = 0.0f;
}

void RingMod::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    auto& input = getInputBuffer(0);
    auto& output = getOutputBuffer(0);

    int numSamples = buffer.getNumSamples();

    // 载波频率: 20Hz - 5000Hz
    float carrierFreq = 20.0f + frequency_ * 4980.0f;
    float phaseIncrement = carrierFreq / static_cast<float>(sampleRate_);

    for (int i = 0; i < numSamples; ++i) {
        // 载波: 正弦波
        float carrier = std::sin(static_cast<float>(AudioUtils::kTwoPI) * carrierPhase_);

        for (int ch = 0; ch < 2; ++ch) {
            float dry = input.getReadPointer(ch)[i];
            float modulated = dry * carrier;
            output.getWritePointer(ch)[i] = AudioUtils::lerp(dry, modulated, mix_);
        }

        // 推进载波相位
        carrierPhase_ += phaseIncrement;
        if (carrierPhase_ >= 1.0f) carrierPhase_ -= 1.0f;
    }
}

void RingMod::releaseResources() {
    AudioNode::releaseResources();
}

void RingMod::setFrequency(float value) {
    frequency_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

void RingMod::setMix(float value) {
    mix_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

float RingMod::getParameter(int index) const {
    switch (index) {
        case 0: return frequency_;
        case 1: return mix_;
        default: return 0.0f;
    }
}

void RingMod::setParameter(int index, float value) {
    switch (index) {
        case 0: setFrequency(value); break;
        case 1: setMix(value); break;
        default: break;
    }
}

juce::String RingMod::getParameterName(int index) const {
    switch (index) {
        case 0: return "频率";
        case 1: return "干湿比";
        default: return "未知";
    }
}

juce::var RingMod::toJson() const {
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

void RingMod::fromJson(const juce::var& json) {
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