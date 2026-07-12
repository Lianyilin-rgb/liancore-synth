// =============================================================================
// LianCore - FilterProcessor 实现
// =============================================================================
#include "FilterProcessor.h"
#include "../utils/AudioUtils.h"

namespace LianCore {

FilterProcessor::FilterProcessor(const juce::String& name)
    : AudioNode(NodeType::Filter, name) {}

void FilterProcessor::prepareToPlay(double sampleRate, int maxSamplesPerBlock) {
    AudioNode::prepareToPlay(sampleRate, maxSamplesPerBlock);
    sampleRate_ = sampleRate;
    updateFilterCoefficients();
}

void FilterProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    auto& input = getInputBuffer(0);
    auto& output = getOutputBuffer(0);

    int numSamples = buffer.getNumSamples();

    for (int ch = 0; ch < 2; ++ch) {
        const float* in = input.getReadPointer(ch);
        float* out = output.getWritePointer(ch);
        auto& state = states_[ch];

        for (int i = 0; i < numSamples; ++i) {
            float x = in[i];

            // 过载驱动
            if (drive_ > 0.001f) {
                float driveGain = 1.0f + drive_ * 10.0f;
                x *= driveGain;
                x = AudioUtils::clamp(x, -1.0f, 1.0f);
                x = std::tanh(x); // 软限幅
            }

            // 双二阶滤波器
            float y = state.b0 * x + state.b1 * state.x1 + state.b2 * state.x2
                    - state.a1 * state.y1 - state.a2 * state.y2;

            // 更新状态
            state.x2 = state.x1;
            state.x1 = x;
            state.y2 = state.y1;
            state.y1 = y;

            // Dry/Wet混合
            out[i] = AudioUtils::lerp(x, y, mix_);
        }
    }
}

void FilterProcessor::releaseResources() {
    AudioNode::releaseResources();
}

// =============================================================================
// 滤波器系数计算
// =============================================================================
void FilterProcessor::updateFilterCoefficients() {
    float freq = AudioUtils::clamp(cutoff_, 20.0f, static_cast<float>(sampleRate_ * 0.49));
    float w0 = static_cast<float>(AudioUtils::kTwoPI) * freq / static_cast<float>(sampleRate_);
    float cosW0 = std::cos(w0);
    float sinW0 = std::sin(w0);

    float q = 0.5f + resonance_ * 20.0f; // 共振 -> Q值映射
    float alpha = sinW0 / (2.0f * q);

    for (auto& state : states_) {
        switch (filterMode_) {
            case FilterMode::LowPass: {
                float b = (1.0f - cosW0) / 2.0f;
                state.b0 = b;
                state.b1 = 1.0f - cosW0;
                state.b2 = b;
                float a0 = 1.0f + alpha;
                state.b0 /= a0; state.b1 /= a0; state.b2 /= a0;
                state.a1 = (-2.0f * cosW0) / a0;
                state.a2 = (1.0f - alpha) / a0;
                break;
            }
            case FilterMode::HighPass: {
                float b = (1.0f + cosW0) / 2.0f;
                state.b0 = b;
                state.b1 = -(1.0f + cosW0);
                state.b2 = b;
                float a0 = 1.0f + alpha;
                state.b0 /= a0; state.b1 /= a0; state.b2 /= a0;
                state.a1 = (-2.0f * cosW0) / a0;
                state.a2 = (1.0f - alpha) / a0;
                break;
            }
            case FilterMode::BandPass: {
                state.b0 = alpha;
                state.b1 = 0.0f;
                state.b2 = -alpha;
                float a0 = 1.0f + alpha;
                state.b0 /= a0; state.b1 /= a0; state.b2 /= a0;
                state.a1 = (-2.0f * cosW0) / a0;
                state.a2 = (1.0f - alpha) / a0;
                break;
            }
            case FilterMode::BandReject: {
                state.b0 = 1.0f;
                state.b1 = -2.0f * cosW0;
                state.b2 = 1.0f;
                float a0 = 1.0f + alpha;
                state.b0 /= a0; state.b1 /= a0; state.b2 /= a0;
                state.a1 = (-2.0f * cosW0) / a0;
                state.a2 = (1.0f - alpha) / a0;
                break;
            }
            case FilterMode::Peak: {
                float A = std::pow(10.0f, resonance_ * 2.0f); // 增益
                float alphaPeak = sinW0 * std::sinh(std::log(2.0f) / 2.0f * q * w0 / sinW0);
                state.b0 = 1.0f + alphaPeak * A;
                state.b1 = -2.0f * cosW0;
                state.b2 = 1.0f - alphaPeak * A;
                float a0 = 1.0f + alphaPeak / A;
                state.b0 /= a0; state.b1 /= a0; state.b2 /= a0;
                state.a1 = (-2.0f * cosW0) / a0;
                state.a2 = (1.0f - alphaPeak / A) / a0;
                break;
            }
        }
    }
}

// =============================================================================
// 参数设置
// =============================================================================
void FilterProcessor::setFilterMode(FilterMode mode) {
    filterMode_ = mode;
    updateFilterCoefficients();
}

void FilterProcessor::setCutoff(float hz) {
    cutoff_ = hz;
    updateFilterCoefficients();
}

void FilterProcessor::setResonance(float q) {
    resonance_ = AudioUtils::clamp(q, 0.0f, 1.0f);
    updateFilterCoefficients();
}

void FilterProcessor::setDrive(float drive) {
    drive_ = AudioUtils::clamp(drive, 0.0f, 1.0f);
}

void FilterProcessor::setMix(float mix) {
    mix_ = AudioUtils::clamp(mix, 0.0f, 1.0f);
}

// =============================================================================
// 参数接口
// =============================================================================
float FilterProcessor::getParameter(int index) const {
    switch (index) {
        case 0: return static_cast<float>(filterMode_) / 4.0f;
        case 1: return cutoff_ / 20000.0f;
        case 2: return resonance_;
        case 3: return drive_;
        case 4: return mix_;
        default: return 0.0f;
    }
}

void FilterProcessor::setParameter(int index, float value) {
    switch (index) {
        case 0: setFilterMode(static_cast<FilterMode>(static_cast<int>(value * 4.0f))); break;
        case 1: setCutoff(value * 20000.0f); break;
        case 2: setResonance(value); break;
        case 3: setDrive(value); break;
        case 4: setMix(value); break;
        default: break;
    }
}

juce::String FilterProcessor::getParameterName(int index) const {
    switch (index) {
        case 0: return "滤波器类型";
        case 1: return "截止频率";
        case 2: return "共振";
        case 3: return "驱动";
        case 4: return "干湿比";
        default: return "未知";
    }
}

} // namespace LianCore