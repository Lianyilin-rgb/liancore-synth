// =============================================================================
// LianCore - ConvolutionReverb 实现
// 基于指数衰减白噪声的简化卷积混响
// =============================================================================
#include "ConvolutionReverb.h"
#include "../utils/AudioUtils.h"

namespace LianCore {

ConvolutionReverb::ConvolutionReverb(const juce::String& name)
    : AudioNode(NodeType::ConvolutionReverb, name) {}

void ConvolutionReverb::prepareToPlay(double sampleRate, int maxSamplesPerBlock) {
    AudioNode::prepareToPlay(sampleRate, maxSamplesPerBlock);
    sampleRate_ = sampleRate;

    rebuildIR();

    for (int ch = 0; ch < 2; ++ch) {
        historyBuffer_[ch].assign(irLength_, 0.0f);
        writePos_[ch] = 0;
    }
}

void ConvolutionReverb::rebuildIR() {
    // 房间大小: 0.1s - 5.0s
    float durationSec = 0.1f + size_ * 4.9f;
    int newIRLength = static_cast<int>(durationSec * static_cast<float>(sampleRate_));
    newIRLength = std::min(newIRLength, kMaxIRLength);
    newIRLength = std::max(newIRLength, 1);

    impulseResponse_.resize(newIRLength);

    auto& rng = AudioUtils::getThreadLocalRNG();

    // 衰减系数: 控制指数衰减速度
    float decayRate = std::exp(-1.0f / (durationSec * static_cast<float>(sampleRate_) * (0.1f + decay_ * 0.9f)));

    // 阻尼系数: 高频衰减 (使用低通滤波平滑IR)
    float dampingLP = 0.1f + (1.0f - damping_) * 0.9f;

    float env = 1.0f;
    float prevSample = 0.0f;

    for (int i = 0; i < newIRLength; ++i) {
        // 白噪声 + 指数衰减包络
        float noise = rng.nextGaussian(0.0f, 0.3f);
        float sample = noise * env;

        // 高频衰减 (一阶低通)
        prevSample += dampingLP * (sample - prevSample);
        sample = prevSample;

        impulseResponse_[i] = sample;

        // 推进包络
        env *= decayRate;
    }

    // 归一化IR
    float maxAbs = 0.0f;
    for (int i = 0; i < newIRLength; ++i) {
        maxAbs = std::max(maxAbs, std::abs(impulseResponse_[i]));
    }
    if (maxAbs > 1e-6f) {
        float normFactor = 0.5f / maxAbs;
        for (int i = 0; i < newIRLength; ++i) {
            impulseResponse_[i] *= normFactor;
        }
    }

    // 重新分配历史缓冲区 (如果IR长度变化)
    if (newIRLength != irLength_) {
        for (int ch = 0; ch < 2; ++ch) {
            historyBuffer_[ch].assign(newIRLength, 0.0f);
            writePos_[ch] = 0;
        }
    }
    irLength_ = newIRLength;
}

void ConvolutionReverb::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    auto& input = getInputBuffer(0);
    auto& output = getOutputBuffer(0);

    int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i) {
        for (int ch = 0; ch < 2; ++ch) {
            float dry = input.getReadPointer(ch)[i];

            // 写入历史缓冲区 (环形)
            historyBuffer_[ch][static_cast<size_t>(writePos_[ch])] = dry;
            writePos_[ch]++;
            if (writePos_[ch] >= irLength_) writePos_[ch] = 0;

            // 直接卷积 (FIR)
            float wet = 0.0f;
            for (int j = 0; j < irLength_; ++j) {
                int readPos = writePos_[ch] - 1 - j;
                if (readPos < 0) readPos += irLength_;
                wet += historyBuffer_[ch][static_cast<size_t>(readPos)] * impulseResponse_[j];
            }

            output.getWritePointer(ch)[i] = AudioUtils::lerp(dry, wet, mix_);
        }
    }
}

void ConvolutionReverb::releaseResources() {
    AudioNode::releaseResources();
    impulseResponse_.clear();
    for (int ch = 0; ch < 2; ++ch) {
        historyBuffer_[ch].clear();
    }
}

void ConvolutionReverb::setSize(float value) {
    size_ = AudioUtils::clamp(value, 0.0f, 1.0f);
    if (sampleRate_ > 0) rebuildIR();
}

void ConvolutionReverb::setDecay(float value) {
    decay_ = AudioUtils::clamp(value, 0.0f, 1.0f);
    if (sampleRate_ > 0) rebuildIR();
}

void ConvolutionReverb::setDamping(float value) {
    damping_ = AudioUtils::clamp(value, 0.0f, 1.0f);
    if (sampleRate_ > 0) rebuildIR();
}

void ConvolutionReverb::setMix(float value) {
    mix_ = AudioUtils::clamp(value, 0.0f, 1.0f);
}

float ConvolutionReverb::getParameter(int index) const {
    switch (index) {
        case 0: return size_;
        case 1: return decay_;
        case 2: return damping_;
        case 3: return mix_;
        default: return 0.0f;
    }
}

void ConvolutionReverb::setParameter(int index, float value) {
    switch (index) {
        case 0: setSize(value); break;
        case 1: setDecay(value); break;
        case 2: setDamping(value); break;
        case 3: setMix(value); break;
        default: break;
    }
}

juce::String ConvolutionReverb::getParameterName(int index) const {
    switch (index) {
        case 0: return "房间大小";
        case 1: return "衰减";
        case 2: return "阻尼";
        case 3: return "干湿比";
        default: return "未知";
    }
}

juce::var ConvolutionReverb::toJson() const {
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

void ConvolutionReverb::fromJson(const juce::var& json) {
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