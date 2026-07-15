// =============================================================================
// LianCore - Phaser 实现
// =============================================================================
#include "Phaser.h"
#include "../utils/AudioUtils.h"

namespace LianCore {

Phaser::Phaser(const juce::String& name)
    : AudioNode(NodeType::Phaser, name) {}

void Phaser::prepareToPlay(double sampleRate, int maxSamplesPerBlock) {
    AudioNode::prepareToPlay(sampleRate, maxSamplesPerBlock);
    sampleRate_ = sampleRate;

    for (int ch = 0; ch < 2; ++ch) {
        channels_[ch].lfoPhase = 0.0f;
        for (int s = 0; s < kMaxStages; ++s) {
            channels_[ch].stages[s] = AllPassStage{};
        }
    }
}

void Phaser::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    auto& input = getInputBuffer(0);
    auto& output = getOutputBuffer(0);

    int numSamples = buffer.getNumSamples();

    // LFO频率: 0.05Hz - 5Hz
    float lfoFreq = 0.05f + rate_ * 4.95f;
    float lfoIncrement = lfoFreq / static_cast<float>(sampleRate_);

    for (int i = 0; i < numSamples; ++i) {
        for (int ch = 0; ch < 2; ++ch) {
            auto& chan = channels_[ch];
            float dry = input.getReadPointer(ch)[i];

            // LFO: 0.0~1.0 (中心频率: 200Hz~2000Hz)
            float lfo = 0.5f + 0.5f * std::sin(static_cast<float>(AudioUtils::kTwoPI) * chan.lfoPhase);
            float centerFreq = 200.0f + lfo * depth_ * 1800.0f;

            // 全通滤波器系数
            float w0 = AudioUtils::kTwoPI * centerFreq / static_cast<float>(sampleRate_);
            float alpha = std::sin(w0) * 0.5f;
            float cosW0 = std::cos(w0);

            // 全通: b0 = 1-alpha, b1 = -2*cosW0, b2 = 1+alpha
            //       a1 = -2*cosW0, a2 = 1-alpha
            float b0 = 1.0f - alpha;
            float b1 = -2.0f * cosW0;
            float b2 = 1.0f + alpha;
            float a1 = b1;
            float a2 = b0;

            // 绕过全通滤波器级联
            float sig = dry + wetFeedback_ * feedback_;
            for (int s = 0; s < stages_; ++s) {
                auto& st = chan.stages[s];
                float y = b0 * sig + b1 * st.x1 + b2 * st.x2 - a1 * st.y1 - a2 * st.y2;
                st.x2 = st.x1;
                st.x1 = sig;
                st.y2 = st.y1;
                st.y1 = y;
                sig = y;
            }
            wetFeedback_ = sig;

            output.getWritePointer(ch)[i] = AudioUtils::lerp(dry, sig, mix_);

            // 推进LFO
            chan.lfoPhase += lfoIncrement;
            if (chan.lfoPhase >= 1.0f) chan.lfoPhase -= 1.0f;
        }
    }
}

void Phaser::releaseResources() {
    AudioNode::releaseResources();
}

void Phaser::setRate(float value) {
    rate_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

void Phaser::setDepth(float value) {
    depth_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

void Phaser::setFeedback(float value) {
    feedback_ = AudioUtils::clamp(value, 0.0f, 0.95f);
}

void Phaser::setMix(float value) {
    mix_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

void Phaser::setStages(int count) {
    stages_ = AudioUtils::clamp(count, 2, kMaxStages);
}

float Phaser::getParameter(int index) const {
    switch (index) {
        case 0: return rate_;
        case 1: return depth_;
        case 2: return feedback_ / 0.95f;
        case 3: return mix_;
        case 4: return (static_cast<float>(stages_) - 2.0f) / static_cast<float>(kMaxStages - 2);
        default: return 0.0f;
    }
}

void Phaser::setParameter(int index, float value) {
    switch (index) {
        case 0: setRate(value); break;
        case 1: setDepth(value); break;
        case 2: setFeedback(value * 0.95f); break;
        case 3: setMix(value); break;
        case 4: setStages(2 + static_cast<int>(value * static_cast<float>(kMaxStages - 2) + 0.5f)); break;
        default: break;
    }
}

juce::String Phaser::getParameterName(int index) const {
    switch (index) {
        case 0: return "速率";
        case 1: return "深度";
        case 2: return "反馈";
        case 3: return "干湿比";
        case 4: return "级数";
        default: return "未知";
    }
}

juce::var Phaser::toJson() const {
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
    obj->setProperty("stages", stages_);
    return juce::var(obj.get());
}

void Phaser::fromJson(const juce::var& json) {
    if (auto* obj = json.getDynamicObject()) {
        name_ = obj->getProperty("name").toString();
        if (auto* params = obj->getProperty("parameters").getArray()) {
            for (int i = 0; i < params->size() && i < getNumParameters(); ++i) {
                if (auto* paramObj = (*params)[i].getDynamicObject()) {
                    setParameter(i, paramObj->getProperty("value"));
                }
            }
        }
        stages_ = static_cast<int>(obj->getProperty("stages"));
    }
}

} // namespace LianCore