// =============================================================================
// LianCore - GranularPlayer 粒子播放器 (增强版)
// 新增: 10种包络类型, AI密度优化, 粒子方向控制, 散射控制
// =============================================================================
#pragma once

#include "../core/AudioNode.h"
#include "../ui/GrainEnvelopeVisualizer.h"
#include <vector>
#include <deque>
#include <array>

namespace LianCore {

// =============================================================================
// 粒子方向类型
// =============================================================================
enum class GrainDirection {
    Forward,      // 正向
    Reverse,       // 反向
    Bidirectional  // 双向 (弹跳)
};

// =============================================================================
// 粒子结构 (增强)
// =============================================================================
struct Grain {
    float position       = 0.0f;   // 在源音频中的位置 (采样)
    float pitch          = 1.0f;   // 音高倍率
    float pan            = 0.0f;   // 声像 (-1.0 to 1.0)
    float envelopePos    = 0.0f;   // 包络位置 (0.0 to 1.0)
    float envelopeSpeed  = 0.0f;   // 包络推进速度
    float age            = 0.0f;   // 粒子已存在时间 (采样)
    float lifetime       = 0.0f;   // 粒子总生存时间 (采样)
    bool active          = false;
    int direction        = 0;      // 方向: 0=正向, 1=反向, 2=双向
    float directionPhase = 0.0f;   // 双向模式的相位
};

// =============================================================================
// GranularPlayer - 粒子播放器 (增强版)
// =============================================================================
class GranularPlayer : public AudioNode {
public:
    static constexpr int kMaxGrains = 512;
    static constexpr float kMinGrainSize = 1.0f;
    static constexpr float kMaxGrainSize = 500.0f;
    static constexpr float kMinGrainDensity = 1.0f;
    static constexpr float kMaxGrainDensity = 200.0f;

    GranularPlayer(const juce::String& name = "粒子播放器");

    void prepareToPlay(double sampleRate, int maxSamplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void releaseResources() override;

    // 源音频
    void loadSourceBuffer(const juce::AudioSampleBuffer& buffer);
    void loadSourceFile(const juce::File& file);
    const juce::AudioSampleBuffer& getSourceBuffer() const { return sourceBuffer_; }
    bool isSourceLoaded() const { return sourceLoaded_; }

    // 粒子参数
    void setGrainSize(float ms);
    void setGrainDensity(float perSec);
    void setGrainPosition(float normPos);
    void setGrainPitchRandom(float semitones);
    void setGrainPanRandom(float amount);
    void setGrainScatter(float amount);  // 新增: 散射量

    // 粒子包络 (扩展)
    void setGrainEnvelopeType(GrainEnvelopeType type);
    void setGrainEnvelopeLength(float ms);
    GrainEnvelopeType getGrainEnvelopeType() const { return envelopeType_; }

    // 粒子方向 (新增)
    void setGrainDirection(GrainDirection dir);
    GrainDirection getGrainDirection() const { return grainDirection_; }
    void setDirectionBias(float bias);  // 0=全正向, 1=全反向, 0.5=混合

    // AI增强 (实现)
    void enableAiDensityControl(bool enabled);
    void setAiDenoiseLevel(float level);
    bool isAiDensityControlEnabled() const { return aiDensityControl_; }
    float getCurrentOptimalDensity() const { return aiOptimalDensity_; }

    // 播放控制
    void setVolume(float volume);
    void setPitchShift(float semitones);

    // 统计
    int getActiveGrainCount() const { return activeGrainCount_; }
    float getCurrentCpuLoad() const { return currentCpuLoad_; }

    // 参数接口
    int getNumParameters() const override { return 13; }
    float getParameter(int index) const override;
    void setParameter(int index, float value) override;
    juce::String getParameterName(int index) const override;

    // 序列化
    juce::var toJson() const override;
    void fromJson(const juce::var& json) override;

private:
    void spawnGrain();
    float getGrainEnvelope(float position) const;
    float readSourceSample(float position, int channel) const;
    void cleanupDeadGrains();
    void updateAI();
    float estimateSignalComplexity() const;

    // 源音频
    juce::AudioSampleBuffer sourceBuffer_;
    bool sourceLoaded_ = false;

    // 粒子参数
    float grainSize_ = 50.0f;
    float grainDensity_ = 30.0f;
    float grainPosition_ = 0.5f;
    float grainPitchRandom_ = 0.0f;
    float grainPanRandom_ = 0.0f;
    float grainScatter_ = 0.5f;       // 新增

    // 包络参数
    GrainEnvelopeType envelopeType_ = GrainEnvelopeType::Hann;
    float envelopeLength_ = 50.0f;

    // 方向参数 (新增)
    GrainDirection grainDirection_ = GrainDirection::Forward;
    float directionBias_ = 0.0f;       // 0=全正向, 1=全反向

    // AI参数
    bool aiDensityControl_ = false;
    float aiDenoiseLevel_ = 0.0f;
    float aiOptimalDensity_ = 30.0f;   // AI计算的最优密度
    float currentCpuLoad_ = 0.0f;      // 当前CPU负载
    float signalComplexity_ = 0.0f;    // 信号复杂度

    // 播放参数
    float volume_ = 1.0f;
    float pitchShift_ = 0.0f;

    // 粒子池
    std::array<Grain, kMaxGrains> grains_;
    int activeGrainCount_ = 0;

    // 密度控制
    float samplesPerGrain_ = 0.0f;
    float samplesSinceLastGrain_ = 0.0f;

    // 随机数
    float nextRandomFloat();
    uint32_t randomSeed_ = 42;

    double sampleRate_ = 44100.0;
    int blockSize_ = 256;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GranularPlayer)
};

} // namespace LianCore