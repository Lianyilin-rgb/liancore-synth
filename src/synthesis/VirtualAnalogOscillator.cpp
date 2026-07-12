// =============================================================================
// LianCore - VirtualAnalogOscillator 实现
// =============================================================================
#include "VirtualAnalogOscillator.h"
#include "../utils/AudioUtils.h"

namespace LianCore {

VirtualAnalogOscillator::VirtualAnalogOscillator(const juce::String& name)
    : AudioNode(NodeType::VirtualAnalogOscillator, name) {}

void VirtualAnalogOscillator::prepareToPlay(double sampleRate, int maxSamplesPerBlock) {
    AudioNode::prepareToPlay(sampleRate, maxSamplesPerBlock);
    sampleRate_ = sampleRate;
    phase_ = 0.0f;
}

void VirtualAnalogOscillator::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    auto& output = getOutputBuffer(0);
    output.clear();

    int numSamples = buffer.getNumSamples();
    float* outL = output.getWritePointer(0);
    float* outR = output.getWritePointer(1);

    // 处理MIDI
    for (const auto& msg : midi) {
        auto message = msg.getMessage();
        if (message.isNoteOn()) {
            setFrequency(AudioUtils::midiNoteToFrequency(message.getNoteNumber()));
        }
    }

    float phaseIncrement = AudioUtils::phaseIncrementPerSample(frequency_, sampleRate_);

    for (int i = 0; i < numSamples; ++i) {
        float sample = generateSample(phase_);
        outL[i] = sample * volume_;
        outR[i] = outL[i];

        phase_ += phaseIncrement;
        phase_ = AudioUtils::wrapPhase(phase_);
    }
}

void VirtualAnalogOscillator::releaseResources() {
    AudioNode::releaseResources();
}

float VirtualAnalogOscillator::generateSample(float phase) {
    float p = AudioUtils::wrapPhase(phase + phaseOffset_);

    switch (waveform_) {
        case VAWaveform::Sine:
            return std::sin(static_cast<float>(AudioUtils::kTwoPI) * p);

        case VAWaveform::Triangle: {
            // 三角波: 2*|2*p-1| - 1
            float t = 2.0f * std::abs(2.0f * p - 1.0f) - 1.0f;
            // 简单抗混叠: 多项式修正
            return t - 0.065f * t * t * t;
        }

        case VAWaveform::Saw: {
            // 锯齿波: 2*p - 1
            float saw = 2.0f * p - 1.0f;
            // 多项式抗混叠
            return saw - saw * saw * saw * 0.1666667f;
        }

        case VAWaveform::Square: {
            // 可变脉宽方波
            return p < pulseWidth_ ? 1.0f : -1.0f;
        }

        case VAWaveform::Pulse: {
            // 窄脉冲
            float pw = 0.1f;
            return p < pw ? 1.0f : (p > 1.0f - pw ? -1.0f : 0.0f);
        }

        default:
            return 0.0f;
    }
}

// =============================================================================
// 参数设置
// =============================================================================
void VirtualAnalogOscillator::setFrequency(float hz) {
    frequency_ = AudioUtils::clamp(hz, 1.0f, 20000.0f);
}

void VirtualAnalogOscillator::setWaveform(VAWaveform waveform) {
    waveform_ = waveform;
}

void VirtualAnalogOscillator::setPulseWidth(float pw) {
    pulseWidth_ = AudioUtils::clamp(pw, 0.01f, 0.99f);
}

void VirtualAnalogOscillator::setVolume(float volume) {
    volume_ = AudioUtils::clamp(volume, 0.0f, 1.0f);
}

void VirtualAnalogOscillator::setPhaseOffset(float offset) {
    phaseOffset_ = offset;
}

// =============================================================================
// 参数接口
// =============================================================================
float VirtualAnalogOscillator::getParameter(int index) const {
    switch (index) {
        case 0: return frequency_ / 20000.0f;
        case 1: return static_cast<float>(waveform_) / 4.0f;
        case 2: return pulseWidth_;
        case 3: return volume_;
        default: return 0.0f;
    }
}

void VirtualAnalogOscillator::setParameter(int index, float value) {
    switch (index) {
        case 0: setFrequency(value * 20000.0f); break;
        case 1: setWaveform(static_cast<VAWaveform>(static_cast<int>(value * 4.0f))); break;
        case 2: setPulseWidth(value); break;
        case 3: setVolume(value); break;
        default: break;
    }
}

juce::String VirtualAnalogOscillator::getParameterName(int index) const {
    switch (index) {
        case 0: return "频率";
        case 1: return "波形";
        case 2: return "脉宽";
        case 3: return "音量";
        default: return "未知";
    }
}

} // namespace LianCore