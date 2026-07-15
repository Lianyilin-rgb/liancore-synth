// =============================================================================
// LianCore - BitCrusher 实现
// =============================================================================
#include "BitCrusher.h"
#include "../utils/AudioUtils.h"

namespace LianCore {

BitCrusher::BitCrusher(const juce::String& name)
    : AudioNode(NodeType::BitCrusher, name) {}

void BitCrusher::prepareToPlay(double sampleRate, int maxSamplesPerBlock) {
    AudioNode::prepareToPlay(sampleRate, maxSamplesPerBlock);
    sampleRate_ = sampleRate;
    holdSample_[0] = 0.0f;
    holdSample_[1] = 0.0f;
    sampleCounter_[0] = 0;
    sampleCounter_[1] = 0;
}

void BitCrusher::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    auto& input = getInputBuffer(0);
    auto& output = getOutputBuffer(0);

    int numSamples = buffer.getNumSamples();

    // 位深度: 1bit - 16bit
    int bits = 1 + static_cast<int>(bitDepth_ * 15.0f);
    float levels = static_cast<float>((1 << bits) - 1);

    // 降采样因子: 1(无) - 100(极强)
    holdLength_ = 1 + static_cast<int>(sampleRateReduction_ * 99.0f);

    for (int i = 0; i < numSamples; ++i) {
        for (int ch = 0; ch < 2; ++ch) {
            float dry = input.getReadPointer(ch)[i];

            // 降采样: 采样保持
            float reduced;
            if (sampleCounter_[ch] <= 0) {
                // 量化位深度
                float normalized = (dry + 1.0f) * 0.5f; // -1..1 → 0..1
                float quantized = std::round(normalized * levels) / levels;
                holdSample_[ch] = quantized * 2.0f - 1.0f; // 0..1 → -1..1
                sampleCounter_[ch] = holdLength_;
            } else {
                sampleCounter_[ch]--;
            }
            reduced = holdSample_[ch];

            output.getWritePointer(ch)[i] = AudioUtils::lerp(dry, reduced, mix_);
        }
    }
}

void BitCrusher::releaseResources() {
    AudioNode::releaseResources();
}

void BitCrusher::setBitDepth(float value) {
    bitDepth_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

void BitCrusher::setSampleRateReduction(float value) {
    sampleRateReduction_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

void BitCrusher::setMix(float value) {
    mix_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

float BitCrusher::getParameter(int index) const {
    switch (index) {
        case 0: return bitDepth_;
        case 1: return sampleRateReduction_;
        case 2: return mix_;
        default: return 0.0f;
    }
}

void BitCrusher::setParameter(int index, float value) {
    switch (index) {
        case 0: setBitDepth(value); break;
        case 1: setSampleRateReduction(value); break;
        case 2: setMix(value); break;
        default: break;
    }
}

juce::String BitCrusher::getParameterName(int index) const {
    switch (index) {
        case 0: return "位深度";
        case 1: return "降采样";
        case 2: return "干湿比";
        default: return "未知";
    }
}

juce::var BitCrusher::toJson() const {
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

void BitCrusher::fromJson(const juce::var& json) {
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