// =============================================================================
// LianCore - WavetableOscillator 双波表振荡器 (对标 Serum 2)
// 支持双波表混合、帧间插值、Unison、Bend模式(FM/AM/RM/Sync)
// =============================================================================
#pragma once

#include "../core/AudioNode.h"
#include "WavetableBank.h"
#include <array>

namespace LianCore {

// =============================================================================
// Bend模式枚举
// =============================================================================
enum class BendMode {
    Off,        // 无Bend
    FM,         // 频率调制
    AM,         // 幅度调制
    RM,         // 环形调制
    Sync,       // 硬同步
};

// =============================================================================
// WavetableOscillator - 双波表振荡器
// =============================================================================
class WavetableOscillator : public AudioNode {
public:
    static constexpr int kMaxUnisonVoices = 16;
    static constexpr int kMaxBlockSize = 512;

    WavetableOscillator(const juce::String& name = "波表振荡器");

    // =========================================================================
    // 音频处理
    // =========================================================================
    void prepareToPlay(double sampleRate, int maxSamplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void releaseResources() override;

    // =========================================================================
    // 波表管理
    // =========================================================================
    WavetableBank& getWavetableA() { return wavetableA_; }
    WavetableBank& getWavetableB() { return wavetableB_; }
    const WavetableBank& getWavetableA() const { return wavetableA_; }
    const WavetableBank& getWavetableB() const { return wavetableB_; }

    void loadWavetableA(const juce::File& wavFile);
    void loadWavetableB(const juce::File& wavFile);
    void saveWavetableA(const juce::File& wavFile);

    // =========================================================================
    // 播放控制
    // =========================================================================
    void setFrequency(float hz);
    void setFrameIndex(float frame); // 0.0 - 255.0
    void setMorphAmount(float amount); // 0.0 - 1.0 (A↔B混合)
    void setVolume(float volume);

    // Unison
    void setUnisonVoices(int count);
    void setUnisonDetune(float cents);
    void setUnisonBlend(float blend);

    // Bend
    void setBendMode(BendMode mode);
    void setBendAmount(float amount);

    // =========================================================================
    // 参数接口
    // =========================================================================
    int getNumParameters() const override { return 10; }
    float getParameter(int index) const override;
    void setParameter(int index, float value) override;
    juce::String getParameterName(int index) const override;

    // =========================================================================
    // 序列化
    // =========================================================================
    juce::var toJson() const override;
    void fromJson(const juce::var& json) override;

private:
    // 生成单个Unison声部的音频
    void generateVoice(float* output, int numSamples, float frequency, float detune,
                       float pan, float volume);

    // 波表数据
    WavetableBank wavetableA_;
    WavetableBank wavetableB_;

    // 播放参数
    float frequency_ = 440.0f;
    float frameIndex_ = 0.0f;
    float morphAmount_ = 0.0f;
    float volume_ = 1.0f;

    // Unison参数
    int unisonVoices_ = 1;
    float unisonDetune_ = 0.0f;
    float unisonBlend_ = 0.5f;

    // Bend参数
    BendMode bendMode_ = BendMode::Off;
    float bendAmount_ = 0.0f;

    // 内部状态
    struct VoiceState {
        float phaseA = 0.0f;
        float phaseB = 0.0f;
        float phaseIncrement = 0.0f;
    };
    std::array<VoiceState, kMaxUnisonVoices> voiceStates_;

    double sampleRate_ = 44100.0;
    int blockSize_ = 256;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WavetableOscillator)
};

} // namespace LianCore