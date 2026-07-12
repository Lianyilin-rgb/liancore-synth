// =============================================================================
// LianCore - WaveguideResonator 波导谐振器 (对标 Absynth)
// 改进的Karplus-Strong物理模型弦乐合成
// =============================================================================
#pragma once

#include "../core/AudioNode.h"

namespace LianCore {

// =============================================================================
// 激励类型
// =============================================================================
enum class ExcitationType {
    Noise,          // 白噪声激励
    Pulse,          // 脉冲激励
    Sample,         // 自定义采样激励
};

// =============================================================================
// WaveguideResonator - 波导谐振器
// =============================================================================
class WaveguideResonator : public AudioNode {
public:
    static constexpr int kMaxDelaySamples = 48000; // 1秒@48kHz
    static constexpr int kMaxBlockSize = 512;

    WaveguideResonator(const juce::String& name = "波导谐振器");

    // =========================================================================
    // 音频处理
    // =========================================================================
    void prepareToPlay(double sampleRate, int maxSamplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void releaseResources() override;

    // =========================================================================
    // Karplus-Strong参数
    // =========================================================================
    void setFrequency(float hz);
    void setDecay(float decay);         // 0.0-1.0 衰减
    void setDamping(float damping);     // 0.0-1.0 阻尼（低通滤波）
    void setBodyResonance(float amount); // 0.0-1.0 体共振

    // =========================================================================
    // 激励控制
    // =========================================================================
    void setExcitationType(ExcitationType type);
    void setExcitationSample(const juce::AudioSampleBuffer& sample);

    // =========================================================================
    // 物理参数
    // =========================================================================
    void setStringTension(float tension);         // 0.5-2.0 张力
    void setStringInharmonicity(float amount);    // 0.0-1.0 非线性
    void setPickupPosition(float pos);            // 0.0-1.0 拾音位置

    // =========================================================================
    // 播放控制
    // =========================================================================
    void setVolume(float volume);
    void triggerNote(float velocity = 1.0f);

    // =========================================================================
    // 参数接口
    // =========================================================================
    int getNumParameters() const override { return 9; }
    float getParameter(int index) const override;
    void setParameter(int index, float value) override;
    juce::String getParameterName(int index) const override;

    // =========================================================================
    // 序列化
    // =========================================================================
    juce::var toJson() const override;
    void fromJson(const juce::var& json) override;

private:
    // 激励信号生成
    void generateExcitation(float* output, int numSamples);
    // 全通滤波器（用于非线性）
    float processAllpass(float input);

    // 延迟线 (环形缓冲区)
    std::vector<float> delayLine_;
    int delayLineSize_ = 0;
    int writePos_ = 0;

    // 阻尼滤波器状态
    float dampingState_ = 0.0f;

    // 全通滤波器状态
    float allpassState_ = 0.0f;

    // Karplus-Strong参数
    float frequency_ = 440.0f;
    float decay_ = 0.5f;
    float damping_ = 0.3f;
    float bodyResonance_ = 0.0f;

    // 激励参数
    ExcitationType excitationType_ = ExcitationType::Noise;
    juce::AudioSampleBuffer excitationSample_;

    // 物理参数
    float stringTension_ = 1.0f;
    float stringInharmonicity_ = 0.0f;
    float pickupPosition_ = 0.5f;

    // 播放控制
    float volume_ = 1.0f;
    bool noteTriggered_ = false;
    float velocity_ = 1.0f;

    double sampleRate_ = 44100.0;
    int blockSize_ = 256;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveguideResonator)
};

} // namespace LianCore