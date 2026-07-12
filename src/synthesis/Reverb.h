// =============================================================================
// LianCore - Reverb Schroeder-Moorer算法混响器
// 4个并行梳状滤波器(Schroeder) + 2个串联全通滤波器(Moorer)
// 支持立体声处理、预延迟、早期反射、阻尼
// =============================================================================
#pragma once

#include "../core/AudioNode.h"
#include <JuceHeader.h>
#include <vector>
#include <array>

namespace LianCore {

class Reverb : public AudioNode {
public:
    Reverb(const juce::String& name = "混响器");

    void prepareToPlay(double sampleRate, int maxSamplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void releaseResources() override;

    // =========================================================================
    // 参数设置
    // =========================================================================
    void setRoomSize(float value);        // 0-1 映射到 0.1-1.0 缩放
    void setDecay(float value);           // 0-1 映射到 0.1-30s
    void setDamping(float value);         // 0-1 低通滤波
    void setMix(float value);             // 0-1 干湿比
    void setPredelay(float value);        // 0-1 映射到 0-200ms
    void setWidth(float value);           // 0-1 立体声宽度
    void setEarlyReflections(float value); // 0-1 早期反射量

    // =========================================================================
    // 参数接口
    // =========================================================================
    int getNumParameters() const override { return 7; }
    float getParameter(int index) const override;
    void setParameter(int index, float value) override;
    juce::String getParameterName(int index) const override;

    // =========================================================================
    // 序列化
    // =========================================================================
    juce::var toJson() const override;
    void fromJson(const juce::var& json) override;

private:
    // =========================================================================
    // 内部辅助方法
    // =========================================================================
    void allocateBuffers();
    void updateInternalParams();

    // 梳状滤波器处理 (单声道)
    static float processComb(float input, float* buffer, int& writePos,
                             int delayLen, float feedback, float damping,
                             float& lastDamp);

    // 全通滤波器处理 (单声道)
    static float processAllPass(float input, float* buffer, int& writePos,
                                int delayLen, float gain);

    // =========================================================================
    // 参数值 (归一化 0-1)
    // =========================================================================
    float roomSize_ = 0.5f;
    float decay_ = 0.5f;
    float damping_ = 0.3f;
    float mix_ = 0.3f;
    float predelay_ = 0.0f;
    float width_ = 0.5f;
    float earlyReflections_ = 0.3f;

    // =========================================================================
    // 内部导出参数
    // =========================================================================
    float roomSizeScaled_ = 0.5f;       // 0.1 - 1.0
    float decayTime_ = 2.0f;            // 秒
    float dampingFreq_ = 5000.0f;       // Hz
    int predelaySamples_ = 0;           // 采样数

    // =========================================================================
    // 梳状滤波器缓冲区 (立体声: 4个梳状 × 2声道)
    // =========================================================================
    static constexpr int kNumCombs = 4;
    static constexpr int kMaxCombDelay = 3000; // 最大延迟 ~68ms @ 44.1k

    struct CombChannel {
        std::vector<float> buffer;
        int writePos = 0;
        int delayLen = 0;
        float lastDamp = 0.0f;
    };
    std::array<CombChannel, kNumCombs> combL_;
    std::array<CombChannel, kNumCombs> combR_;

    // 梳状滤波器基础延迟 (样本数, 将在prepareToPlay中按采样率缩放)
    float baseCombDelaysMs_[kNumCombs] = { 29.7f, 37.1f, 41.1f, 43.7f };

    // =========================================================================
    // 全通滤波器缓冲区 (立体声: 2个全通 × 2声道)
    // =========================================================================
    static constexpr int kNumAllPass = 2;
    static constexpr int kMaxAllPassDelay = 500; // 最大延迟 ~11ms @ 44.1k

    struct AllPassChannel {
        std::vector<float> buffer;
        int writePos = 0;
        int delayLen = 0;
    };
    std::array<AllPassChannel, kNumAllPass> apL_;
    std::array<AllPassChannel, kNumAllPass> apR_;

    float baseAllPassDelaysMs_[kNumAllPass] = { 5.0f, 1.7f };

    // 全通滤波器增益 (g系数)
    float apGains_[kNumAllPass] = { 0.7f, 0.7f };

    // =========================================================================
    // 预延迟缓冲区
    // =========================================================================
    static constexpr int kMaxPredelaySamples = 9600; // ~200ms @ 48k
    struct PredelayChannel {
        std::vector<float> buffer;
        int writePos = 0;
    };
    std::array<PredelayChannel, 2> predelayChans_;

    // =========================================================================
    // 早期反射
    // =========================================================================
    static constexpr int kNumEarlyReflections = 4;
    static constexpr int kMaxEarlyReflDelay = 4800; // ~100ms

    struct EarlyReflChannel {
        std::vector<float> buffer;
        int writePos = 0;
    };
    std::array<EarlyReflChannel, 2> earlyReflChans_;

    // 早期反射延迟时间 (ms)
    float earlyReflDelaysMs_[kNumEarlyReflections] = { 4.3f, 8.7f, 13.1f, 19.3f };
    // 早期反射增益
    float earlyReflGains_[kNumEarlyReflections] = { 0.8f, 0.6f, 0.4f, 0.3f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Reverb)
};

} // namespace LianCore