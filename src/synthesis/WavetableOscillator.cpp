// =============================================================================
// LianCore - WavetableOscillator 实现
// =============================================================================
#include "WavetableOscillator.h"
#include "../utils/AudioUtils.h"

namespace LianCore {

WavetableOscillator::WavetableOscillator(const juce::String& name)
    : AudioNode(NodeType::WavetableOscillator, name) {
    // 初始化波表为默认锯齿波
    wavetableA_.generateSawWave(64);
    wavetableB_.generateSineWave(1);

    // 添加输出端口
    PortDescriptor outputDesc;
    outputDesc.name = "音频输出";
    outputDesc.isAudio = true;
    outputDesc.defaultValue = 0.0f;
    outputDesc.minValue = -1.0f;
    outputDesc.maxValue = 1.0f;
    addOutputPort("音频输出", outputDesc);
}

// =============================================================================
// 音频处理
// =============================================================================
void WavetableOscillator::prepareToPlay(double sampleRate, int maxSamplesPerBlock) {
    AudioNode::prepareToPlay(sampleRate, maxSamplesPerBlock);
    sampleRate_ = sampleRate;
    blockSize_ = maxSamplesPerBlock;

    // 初始化所有声部相位
    for (auto& voice : voiceStates_) {
        voice.phaseA = 0.0f;
        voice.phaseB = 0.0f;
        voice.phaseIncrement = 0.0f;
    }
}

void WavetableOscillator::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    auto& output = getOutputBuffer(0);
    output.clear();

    int numSamples = buffer.getNumSamples();
    float* outL = output.getWritePointer(0);
    float* outR = output.getWritePointer(1);

    // 如果波表为空，静音
    if (wavetableA_.isEmpty()) {
        AudioUtils::clearBufferSIMD(outL, numSamples);
        AudioUtils::clearBufferSIMD(outR, numSamples);
        return;
    }

    // 处理MIDI消息
    for (const auto& msg : midi) {
        auto message = msg.getMessage();
        if (message.isNoteOn()) {
            setFrequency(AudioUtils::midiNoteToFrequency(message.getNoteNumber()));
        }
    }

    // 为每个Unison声部生成音频
    if (unisonVoices_ == 1) {
        // 单声部: 直接生成
        generateVoice(outL, numSamples, frequency_, 0.0f, 0.0f, volume_);
        std::copy_n(outL, numSamples, outR); // 复制到右声道
    } else {
        // 多声部Unison
        float panSpread = 1.0f / unisonVoices_;
        float voiceGain = 1.0f / std::sqrt(static_cast<float>(unisonVoices_));

        for (int v = 0; v < unisonVoices_; ++v) {
            float detune = 0.0f;
            if (v > 0) {
                // 线性分布detune
                float normalizedPos = static_cast<float>(v) / (unisonVoices_ - 1);
                detune = (normalizedPos * 2.0f - 1.0f) * unisonDetune_;
            }

            float pan = (v * 2.0f / (unisonVoices_ - 1) - 1.0f) * panSpread;
            if (unisonVoices_ == 1) pan = 0.0f;

            // 临时缓冲区
            juce::HeapBlock<float> voiceBuffer(numSamples);
            generateVoice(voiceBuffer.getData(), numSamples, frequency_, detune, pan, voiceGain);

            // 混合到输出
            float leftGain = voiceGain * (1.0f - pan) * 0.5f;
            float rightGain = voiceGain * (1.0f + pan) * 0.5f;

            for (int i = 0; i < numSamples; ++i) {
                outL[i] += voiceBuffer[i] * leftGain;
                outR[i] += voiceBuffer[i] * rightGain;
            }
        }
    }
}

void WavetableOscillator::releaseResources() {
    AudioNode::releaseResources();
}

// =============================================================================
// 生成单个声部
// =============================================================================
void WavetableOscillator::generateVoice(float* output, int numSamples,
                                         float frequency, float detune, float pan, float volume) {
    auto& voice = voiceStates_[0]; // 简化: 使用第一个声部状态

    float freqRatio = AudioUtils::centsToRatio(detune);
    float effectiveFreq = frequency * freqRatio;
    voice.phaseIncrement = AudioUtils::phaseIncrementPerSample(effectiveFreq, sampleRate_);

    // 预计算波表帧数据
    int frameSize = wavetableA_.getFrameSize();
    float frameScale = static_cast<float>(frameSize);

    for (int i = 0; i < numSamples; ++i) {
        // 处理Bend模式
        float bendMod = 0.0f;
        if (bendMode_ == BendMode::FM && bendAmount_ > 0.0f) {
            // FM: 频率调制(使用另一个振荡器频率)
            float modulatorFreq = frequency_ * AudioUtils::semitonesToRatio(bendAmount_ * 24.0f);
            float modPhase = voice.phaseA * modulatorFreq / frequency_;
            bendMod = std::sin(AudioUtils::kTwoPI * modPhase) * bendAmount_;
        }

        // 计算波表位置
        float phaseA = AudioUtils::wrapPhase(voice.phaseA + bendMod);
        float phaseB = AudioUtils::wrapPhase(voice.phaseB + bendMod);

        // 波表帧索引
        float frameA = AudioUtils::clamp(frameIndex_, 0.0f, 255.0f);
        float frameB = frameA;

        // 读取波表采样
        int sampleIdxA = static_cast<int>(phaseA * frameSize) % frameSize;
        const float* frameDataA = wavetableA_.getFrameData(static_cast<int>(frameA));
        float sampleA = frameDataA ? frameDataA[sampleIdxA] : 0.0f;

        float sampleB = 0.0f;
        if (wavetableB_.getNumFrames() > 0) {
            int sampleIdxB = static_cast<int>(phaseB * frameSize) % frameSize;
            const float* frameDataB = wavetableB_.getFrameData(static_cast<int>(frameB));
            sampleB = frameDataB ? frameDataB[sampleIdxB] : 0.0f;
        }

        // 混合A/B波表
        float sample = AudioUtils::lerp(sampleA, sampleB, morphAmount_);

        // AM处理
        if (bendMode_ == BendMode::AM && bendAmount_ > 0.0f) {
            float amMod = 0.5f + 0.5f * std::sin(AudioUtils::kTwoPI * voice.phaseA * 2.0f);
            sample *= AudioUtils::lerp(1.0f, amMod, bendAmount_);
        }

        // RM处理
        if (bendMode_ == BendMode::RM && bendAmount_ > 0.0f) {
            float carrierFreq = frequency_ * AudioUtils::semitonesToRatio(bendAmount_ * 24.0f);
            float carrierPhase = AudioUtils::wrapPhase(voice.phaseA * carrierFreq / frequency_);
            sample *= std::sin(AudioUtils::kTwoPI * carrierPhase);
        }

        output[i] = sample * volume;

        // 更新相位
        voice.phaseA += voice.phaseIncrement;
        voice.phaseB += voice.phaseIncrement;
    }
}

// =============================================================================
// 波表管理
// =============================================================================
void WavetableOscillator::loadWavetableA(const juce::File& wavFile) {
    wavetableA_.loadFromWavFile(wavFile);
}

void WavetableOscillator::loadWavetableB(const juce::File& wavFile) {
    wavetableB_.loadFromWavFile(wavFile);
}

void WavetableOscillator::saveWavetableA(const juce::File& wavFile) {
    wavetableA_.saveToWavFile(wavFile);
}

// =============================================================================
// 播放控制
// =============================================================================
void WavetableOscillator::setFrequency(float hz) {
    frequency_ = AudioUtils::clamp(hz, 1.0f, 20000.0f);
}

void WavetableOscillator::setFrameIndex(float frame) {
    frameIndex_ = AudioUtils::clamp(frame, 0.0f, 255.0f);
}

void WavetableOscillator::setMorphAmount(float amount) {
    morphAmount_ = AudioUtils::clamp(amount, 0.0f, 1.0f);
}

void WavetableOscillator::setVolume(float volume) {
    volume_ = AudioUtils::clamp(volume, 0.0f, 1.0f);
}

void WavetableOscillator::setUnisonVoices(int count) {
    unisonVoices_ = AudioUtils::clamp(count, 1, kMaxUnisonVoices);
}

void WavetableOscillator::setUnisonDetune(float cents) {
    unisonDetune_ = AudioUtils::clamp(cents, 0.0f, 50.0f);
}

void WavetableOscillator::setUnisonBlend(float blend) {
    unisonBlend_ = AudioUtils::clamp(blend, 0.0f, 1.0f);
}

void WavetableOscillator::setBendMode(BendMode mode) {
    bendMode_ = mode;
}

void WavetableOscillator::setBendAmount(float amount) {
    bendAmount_ = AudioUtils::clamp(amount, 0.0f, 1.0f);
}

// =============================================================================
// 参数接口
// =============================================================================
float WavetableOscillator::getParameter(int index) const {
    switch (index) {
        case 0: return frequency_ / 20000.0f;
        case 1: return frameIndex_ / 255.0f;
        case 2: return morphAmount_;
        case 3: return volume_;
        case 4: return static_cast<float>(unisonVoices_) / kMaxUnisonVoices;
        case 5: return unisonDetune_ / 50.0f;
        case 6: return unisonBlend_;
        case 7: return static_cast<float>(bendMode_) / 4.0f;
        case 8: return bendAmount_;
        case 9: return 0.0f; // 预留
        default: return 0.0f;
    }
}

void WavetableOscillator::setParameter(int index, float value) {
    switch (index) {
        case 0: setFrequency(value * 20000.0f); break;
        case 1: setFrameIndex(value * 255.0f); break;
        case 2: setMorphAmount(value); break;
        case 3: setVolume(value); break;
        case 4: setUnisonVoices(static_cast<int>(value * kMaxUnisonVoices)); break;
        case 5: setUnisonDetune(value * 50.0f); break;
        case 6: setUnisonBlend(value); break;
        case 7: setBendMode(static_cast<BendMode>(static_cast<int>(value * 4.0f))); break;
        case 8: setBendAmount(value); break;
        default: break;
    }
}

juce::String WavetableOscillator::getParameterName(int index) const {
    switch (index) {
        case 0: return "频率";
        case 1: return "波表帧";
        case 2: return "波表混合";
        case 3: return "音量";
        case 4: return "Unison声部";
        case 5: return "Unison失谐";
        case 6: return "Unison混合";
        case 7: return "Bend模式";
        case 8: return "Bend量";
        default: return "未知";
    }
}

// =============================================================================
// 序列化
// =============================================================================
juce::var WavetableOscillator::toJson() const {
    auto json = AudioNode::toJson();
    // 波表参数由AudioNode基类序列化
    return json;
}

void WavetableOscillator::fromJson(const juce::var& json) {
    AudioNode::fromJson(json);
}

} // namespace LianCore