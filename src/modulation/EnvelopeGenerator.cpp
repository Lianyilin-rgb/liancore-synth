// =============================================================================
// LianCore - EnvelopeGenerator 实现
// =============================================================================
#include "EnvelopeGenerator.h"
#include "../utils/AudioUtils.h"

namespace LianCore {

EnvelopeGenerator::EnvelopeGenerator(const juce::String& name)
    : AudioNode(NodeType::Envelope, name) {}

void EnvelopeGenerator::prepareToPlay(double sampleRate, int) {
    AudioNode::prepareToPlay(sampleRate, 256);
    sampleRate_ = sampleRate;
}

void EnvelopeGenerator::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    int numSamples = buffer.getNumSamples();

    // 处理MIDI
    for (const auto& msg : midi) {
        auto message = msg.getMessage();
        if (message.isNoteOn()) {
            velocity_ = message.getFloatVelocity();
            noteOn();
        } else if (message.isNoteOff()) {
            noteOff();
        }
    }

    // 计算每个采样的包络值
    float stepSize = 1.0f / static_cast<float>(sampleRate_) * 1000.0f; // 转换为ms

    for (int i = 0; i < numSamples; ++i) {
        switch (stage_) {
            case EnvelopeStage::Idle:
                currentValue_ = 0.0f;
                break;

            case EnvelopeStage::Attack: {
                float attackRate = 1.0f / (attackMs_ + 0.001f);
                currentValue_ += attackRate * stepSize;
                if (currentValue_ >= 1.0f) {
                    currentValue_ = 1.0f;
                    stage_ = EnvelopeStage::Decay;
                }
                break;
            }

            case EnvelopeStage::Decay: {
                float decayRate = (1.0f - sustainLevel_) / (decayMs_ + 0.001f);
                currentValue_ -= decayRate * stepSize;
                if (currentValue_ <= sustainLevel_) {
                    currentValue_ = sustainLevel_;
                    stage_ = EnvelopeStage::Sustain;
                }
                break;
            }

            case EnvelopeStage::Sustain:
                currentValue_ = sustainLevel_;
                break;

            case EnvelopeStage::Release: {
                float releaseRate = sustainLevel_ / (releaseMs_ + 0.001f);
                currentValue_ -= releaseRate * stepSize;
                if (currentValue_ <= 0.0f) {
                    currentValue_ = 0.0f;
                    stage_ = EnvelopeStage::Idle;
                }
                break;
            }
        }
    }

    // 输出到调制端口
    auto& output = getOutputBuffer(0);
    output.clear();
    output.getWritePointer(0)[0] = currentValue_ * velocity_;
}

void EnvelopeGenerator::noteOn() {
    stage_ = EnvelopeStage::Attack;
}

void EnvelopeGenerator::noteOff() {
    stage_ = EnvelopeStage::Release;
}

void EnvelopeGenerator::setAttack(float ms) {
    attackMs_ = AudioUtils::clamp(ms, 0.1f, 10000.0f);
}

void EnvelopeGenerator::setDecay(float ms) {
    decayMs_ = AudioUtils::clamp(ms, 0.1f, 10000.0f);
}

void EnvelopeGenerator::setSustain(float level) {
    sustainLevel_ = AudioUtils::clamp(level, 0.0f, 1.0f);
}

void EnvelopeGenerator::setRelease(float ms) {
    releaseMs_ = AudioUtils::clamp(ms, 0.1f, 10000.0f);
}

void EnvelopeGenerator::setVelocitySensitivity(float amount) {
    velocitySensitivity_ = AudioUtils::clamp(amount, 0.0f, 1.0f);
}

// =============================================================================
// 参数接口
// =============================================================================
float EnvelopeGenerator::getParameter(int index) const {
    switch (index) {
        case 0: return attackMs_ / 10000.0f;
        case 1: return decayMs_ / 10000.0f;
        case 2: return sustainLevel_;
        case 3: return releaseMs_ / 10000.0f;
        case 4: return velocitySensitivity_;
        default: return 0.0f;
    }
}

void EnvelopeGenerator::setParameter(int index, float value) {
    switch (index) {
        case 0: setAttack(value * 10000.0f); break;
        case 1: setDecay(value * 10000.0f); break;
        case 2: setSustain(value); break;
        case 3: setRelease(value * 10000.0f); break;
        case 4: setVelocitySensitivity(value); break;
        default: break;
    }
}

juce::String EnvelopeGenerator::getParameterName(int index) const {
    switch (index) {
        case 0: return "Attack";
        case 1: return "Decay";
        case 2: return "Sustain";
        case 3: return "Release";
        case 4: return "力度灵敏度";
        default: return "未知";
    }
}

} // namespace LianCore