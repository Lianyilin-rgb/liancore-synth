// =============================================================================
// LianCore - LFOGenerator 实现
// =============================================================================
#include "LFOGenerator.h"
#include "../utils/AudioUtils.h"

namespace LianCore {

LFOGenerator::LFOGenerator(const juce::String& name)
    : AudioNode(NodeType::LFO, name) {}

void LFOGenerator::prepareToPlay(double sampleRate, int) {
    AudioNode::prepareToPlay(sampleRate, 256);
    sampleRate_ = sampleRate;
}

void LFOGenerator::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    int numSamples = buffer.getNumSamples();
    float phaseIncrement = AudioUtils::phaseIncrementPerSample(frequency_, sampleRate_);

    // 按键触发
    for (const auto& msg : midi) {
        if (msg.getMessage().isNoteOn() && syncMode_ == LFOSyncMode::KeyTrigger) {
            phase_ = 0.0f;
        }
    }

    for (int i = 0; i < numSamples; ++i) {
        float rawValue = generateValue(phase_);

        // 平滑处理
        if (smooth_ > 0.001f) {
            float smoothingFactor = std::exp(-1.0f / (smooth_ * 0.01f * static_cast<float>(sampleRate_)));
            currentValue_ = currentValue_ * smoothingFactor + rawValue * depth_ * (1.0f - smoothingFactor);
        } else {
            currentValue_ = rawValue * depth_;
        }

        phase_ += phaseIncrement;
        phase_ = AudioUtils::wrapPhase(phase_);
    }

    auto& output = getOutputBuffer(0);
    output.clear();
    output.getWritePointer(0)[0] = currentValue_;
}

float LFOGenerator::generateValue(float phase) {
    float p = AudioUtils::wrapPhase(phase + phaseOffset_);

    switch (waveform_) {
        case LFOWaveform::Sine:
            return std::sin(static_cast<float>(AudioUtils::kTwoPI) * p);

        case LFOWaveform::Triangle:
            return 2.0f * std::abs(2.0f * p - 1.0f) - 1.0f;

        case LFOWaveform::Saw:
            return 2.0f * p - 1.0f;

        case LFOWaveform::Square:
            return p < 0.5f ? 1.0f : -1.0f;

        case LFOWaveform::Random:
        case LFOWaveform::SampleAndHold: {
            // 使用噪声作为随机源
            auto& rng = AudioUtils::getThreadLocalRNG();
            return rng.nextFloat(-1.0f, 1.0f);
        }

        default:
            return 0.0f;
    }
}

// =============================================================================
// 参数设置
// =============================================================================
void LFOGenerator::setFrequency(float hz) {
    frequency_ = AudioUtils::clamp(hz, 0.01f, 100.0f);
}

void LFOGenerator::setWaveform(LFOWaveform waveform) {
    waveform_ = waveform;
}

void LFOGenerator::setSyncMode(LFOSyncMode mode) {
    syncMode_ = mode;
}

void LFOGenerator::setPhase(float phase) {
    phaseOffset_ = phase;
}

void LFOGenerator::setSmooth(float amount) {
    smooth_ = AudioUtils::clamp(amount, 0.0f, 1.0f);
}

void LFOGenerator::setDepth(float depth) {
    depth_ = AudioUtils::clamp(depth, 0.0f, 1.0f);
}

// =============================================================================
// 参数接口
// =============================================================================
float LFOGenerator::getParameter(int index) const {
    switch (index) {
        case 0: return frequency_ / 100.0f;
        case 1: return static_cast<float>(waveform_) / 5.0f;
        case 2: return static_cast<float>(syncMode_) / 2.0f;
        case 3: return phaseOffset_;
        case 4: return smooth_;
        case 5: return depth_;
        default: return 0.0f;
    }
}

void LFOGenerator::setParameter(int index, float value) {
    switch (index) {
        case 0: setFrequency(value * 100.0f); break;
        case 1: setWaveform(static_cast<LFOWaveform>(static_cast<int>(value * 5.0f))); break;
        case 2: setSyncMode(static_cast<LFOSyncMode>(static_cast<int>(value * 2.0f))); break;
        case 3: setPhase(value); break;
        case 4: setSmooth(value); break;
        case 5: setDepth(value); break;
        default: break;
    }
}

juce::String LFOGenerator::getParameterName(int index) const {
    switch (index) {
        case 0: return "频率";
        case 1: return "波形";
        case 2: return "同步模式";
        case 3: return "相位";
        case 4: return "平滑";
        case 5: return "深度";
        default: return "未知";
    }
}

} // namespace LianCore