// =============================================================================
// LianCore - NoiseGenerator 实现
// =============================================================================
#include "NoiseGenerator.h"
#include "../utils/AudioUtils.h"

namespace LianCore {

NoiseGenerator::NoiseGenerator(const juce::String& name)
    : AudioNode(NodeType::NoiseGenerator, name) {}

void NoiseGenerator::prepareToPlay(double sampleRate, int maxSamplesPerBlock) {
    AudioNode::prepareToPlay(sampleRate, maxSamplesPerBlock);
    sampleRate_ = sampleRate;
    holdInterval_ = static_cast<int>(sampleRate / sampleHoldRate_);
    if (holdInterval_ < 1) holdInterval_ = 1;
}

void NoiseGenerator::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    auto& output = getOutputBuffer(0);
    output.clear();

    int numSamples = buffer.getNumSamples();
    float* outL = output.getWritePointer(0);
    float* outR = output.getWritePointer(1);

    for (int i = 0; i < numSamples; ++i) {
        float sample = 0.0f;

        switch (noiseType_) {
            case NoiseType::White:
                sample = generateWhiteNoise();
                break;
            case NoiseType::Pink:
                sample = generatePinkNoise();
                break;
            case NoiseType::Brown:
                sample = generateBrownNoise();
                break;
            case NoiseType::SampleHold:
                sample = generateSampleHold();
                break;
        }

        outL[i] = sample * volume_;
        outR[i] = outL[i];
    }
}

void NoiseGenerator::releaseResources() {
    AudioNode::releaseResources();
}

// =============================================================================
// 噪声生成
// =============================================================================
float NoiseGenerator::generateWhiteNoise() {
    // 均匀分布白噪声: [-1, 1]
    auto& rng = AudioUtils::getThreadLocalRNG();
    return rng.nextFloat(-1.0f, 1.0f);
}

float NoiseGenerator::generatePinkNoise() {
    // 使用Voss-McCartney算法生成粉红噪声
    auto& rng = AudioUtils::getThreadLocalRNG();
    float white = rng.nextFloat(-1.0f, 1.0f);

    pinkB0_ = 0.99886f * pinkB0_ + white * 0.0555179f;
    pinkB1_ = 0.99332f * pinkB1_ + white * 0.0750759f;
    pinkB2_ = 0.96900f * pinkB2_ + white * 0.1538520f;
    pinkB3_ = 0.86650f * pinkB3_ + white * 0.3104856f;
    pinkB4_ = 0.55000f * pinkB4_ + white * 0.5329522f;
    pinkB5_ = -0.7616f * pinkB5_ - white * 0.0168980f;

    float pink = (pinkB0_ + pinkB1_ + pinkB2_ + pinkB3_ + pinkB4_ + pinkB5_ + white * 0.5362f) * 0.11f;
    return AudioUtils::clamp(pink, -1.0f, 1.0f);
}

float NoiseGenerator::generateBrownNoise() {
    // 积分白噪声: 棕色噪声
    auto& rng = AudioUtils::getThreadLocalRNG();
    float white = rng.nextFloat(-1.0f, 1.0f) * 0.02f; // 小步长防止漂移

    brownLast_ += white;
    brownLast_ *= 0.999f; // 轻微衰减防止漂移

    return AudioUtils::clamp(brownLast_, -1.0f, 1.0f);
}

float NoiseGenerator::generateSampleHold() {
    holdCounter_++;
    if (holdCounter_ >= holdInterval_) {
        holdCounter_ = 0;
        auto& rng = AudioUtils::getThreadLocalRNG();
        holdValue_ = rng.nextFloat(-1.0f, 1.0f);
    }
    return holdValue_;
}

// =============================================================================
// 参数设置
// =============================================================================
void NoiseGenerator::setNoiseType(NoiseType type) {
    noiseType_ = type;
}

void NoiseGenerator::setVolume(float volume) {
    volume_ = AudioUtils::clamp(volume, 0.0f, 1.0f);
}

void NoiseGenerator::setSampleHoldRate(float hz) {
    sampleHoldRate_ = AudioUtils::clamp(hz, 1.0f, 1000.0f);
    holdInterval_ = static_cast<int>(sampleRate_ / sampleHoldRate_);
    if (holdInterval_ < 1) holdInterval_ = 1;
}

// =============================================================================
// 参数接口
// =============================================================================
float NoiseGenerator::getParameter(int index) const {
    switch (index) {
        case 0: return static_cast<float>(noiseType_) / 3.0f;
        case 1: return volume_;
        case 2: return sampleHoldRate_ / 1000.0f;
        default: return 0.0f;
    }
}

void NoiseGenerator::setParameter(int index, float value) {
    switch (index) {
        case 0: setNoiseType(static_cast<NoiseType>(static_cast<int>(value * 3.0f))); break;
        case 1: setVolume(value); break;
        case 2: setSampleHoldRate(value * 1000.0f); break;
        default: break;
    }
}

juce::String NoiseGenerator::getParameterName(int index) const {
    switch (index) {
        case 0: return "噪声类型";
        case 1: return "音量";
        case 2: return "采样保持频率";
        default: return "未知";
    }
}

} // namespace LianCore