// =============================================================================
// LianCore - GranularPlayer 粒子播放器 (对标 Absynth)
// 粒子合成引擎：将音频分割为微小粒子重组
// =============================================================================
#pragma once

#include "../core/AudioNode.h"
#include <vector>
#include <deque>

namespace LianCore {

// =============================================================================
// 粒子包络类型
// =============================================================================
enum class GrainEnvelopeType {
    Hann,           // 汉宁窗
    Hamming,        // 汉明窗
    Triangle,       // 三角窗
    Exponential,    // 指数衰减
    Rectangular,    // 矩形窗
};

// =============================================================================
// 粒子结构
// =============================================================================
struct Grain {
    float position;       // 在源音频中的位置 (采样)
    float pitch;          // 音高倍率
    float pan;            // 声像 (-1.0 to 1.0)
    float envelopePos;    // 包络位置 (0.0 to 1.0)
    float envelopeSpeed;  // 包络推进速度
    float age;            // 粒子已存在时间 (采样)
    float lifetime;       // 粒子总生存时间 (采样)
    bool active;
};

// =============================================================================
// GranularPlayer - 粒子播放器
// =============================================================================
class GranularPlayer : public AudioNode {
public:
    static constexpr int kMaxGrains = 512;
    static constexpr float kMinGrainSize = 1.0f;    // ms
    static constexpr float kMaxGrainSize = 500.0f;  // ms
    static constexpr float kMinGrainDensity = 1.0f; // 粒/秒
    static constexpr float kMaxGrainDensity = 200.0f;

    GranularPlayer(const juce::String& name = "粒子播放器");

    // =========================================================================
    // 音频处理
    // =========================================================================
    void prepareToPlay(double sampleRate, int maxSamplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void releaseResources() override;

    // =========================================================================
    // 源音频加载
    // =========================================================================
    void loadSourceBuffer(const juce::AudioSampleBuffer& buffer);
    void loadSourceFile(const juce::File& file);
    const juce::AudioSampleBuffer& getSourceBuffer() const { return sourceBuffer_; }

    // =========================================================================
    // 粒子参数
    // =========================================================================
    void setGrainSize(float ms);           // 1-500ms
    void setGrainDensity(float perSec);    // 1-200粒/秒
    void setGrainPosition(float normPos);  // 0.0-1.0 归一化位置
    void setGrainPitchRandom(float semitones); // 音高随机偏移
    void setGrainPanRandom(float amount);      // 声像随机偏移

    // =========================================================================
    // 粒子包络
    // =========================================================================
    void setGrainEnvelopeType(GrainEnvelopeType type);
    void setGrainEnvelopeLength(float ms);

    // =========================================================================
    // AI增强
    // =========================================================================
    void enableAiDensityControl(bool enabled);
    void setAiDenoiseLevel(float level);

    // =========================================================================
    // 播放控制
    // =========================================================================
    void setVolume(float volume);
    void setPitchShift(float semitones);

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
    // 生成新粒子
    void spawnGrain();
    // 获取粒子包络值
    float getGrainEnvelope(float position) const;
    // 读取源音频样本（带插值）
    float readSourceSample(float position, int channel) const;
    // 清理过期粒子
    void cleanupDeadGrains();

    // 源音频
    juce::AudioSampleBuffer sourceBuffer_;
    bool sourceLoaded_ = false;

    // 粒子参数
    float grainSize_ = 50.0f;       // ms
    float grainDensity_ = 30.0f;    // 粒/秒
    float grainPosition_ = 0.5f;    // 归一化位置
    float grainPitchRandom_ = 0.0f; // 半音
    float grainPanRandom_ = 0.0f;   // 0.0-1.0

    // 包络参数
    GrainEnvelopeType envelopeType_ = GrainEnvelopeType::Hann;
    float envelopeLength_ = 50.0f;  // ms

    // AI参数
    bool aiDensityControl_ = false;
    float aiDenoiseLevel_ = 0.0f;

    // 播放参数
    float volume_ = 1.0f;
    float pitchShift_ = 0.0f;       // 半音

    // 粒子池
    std::array<Grain, kMaxGrains> grains_;
    int activeGrainCount_ = 0;

    // 密度控制
    float samplesPerGrain_ = 0.0f;
    float samplesSinceLastGrain_ = 0.0f;

    // 随机数
    float nextRandomFloat();

    double sampleRate_ = 44100.0;
    int blockSize_ = 256;
    uint32_t randomSeed_ = 42;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GranularPlayer)
};

} // namespace LianCore