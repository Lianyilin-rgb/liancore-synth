// =============================================================================
// LianCore - MultiSampler 多采样播放器 (对标 VPS Avenger 2)
// 多采样键位/力度映射、AI自动映射、循环播放
// =============================================================================
#pragma once

#include "../core/AudioNode.h"
#include <vector>
#include <unordered_map>

namespace LianCore {

// =============================================================================
// 采样区域
// =============================================================================
struct SampleZone {
    juce::String samplePath;
    juce::AudioSampleBuffer buffer;
    int rootNote = 60;          // MIDI根音
    int lowNote = 0;            // 最低键位
    int highNote = 127;         // 最高键位
    int lowVelocity = 0;        // 最低力度
    int highVelocity = 127;     // 最高力度
    float volume = 1.0f;
    float pan = 0.0f;
    float tune = 0.0f;          // 微调(半音)
    bool loop = false;
    int loopStart = 0;
    int loopEnd = 0;
    bool active = true;
};

// =============================================================================
// 循环模式
// =============================================================================
enum class LoopMode {
    NoLoop,         // 不循环
    Forward,        // 正向循环
    PingPong,       // 乒乓循环
};

// =============================================================================
// MultiSampler - 多采样播放器
// =============================================================================
class MultiSampler : public AudioNode {
public:
    static constexpr int kMaxSampleZones = 128;
    static constexpr int kMaxVoices = 32;
    static constexpr int kMaxBlockSize = 512;

    MultiSampler(const juce::String& name = "多采样器");

    // =========================================================================
    // 音频处理
    // =========================================================================
    void prepareToPlay(double sampleRate, int maxSamplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void releaseResources() override;

    // =========================================================================
    // 采样管理
    // =========================================================================
    int addSample(const juce::File& file, int rootNote, int lowNote, int highNote);
    void removeSample(int index);
    void clearSamples();

    // =========================================================================
    // 键位映射
    // =========================================================================
    void setKeyRange(int sampleIndex, int lowNote, int highNote);
    void setVelocityRange(int sampleIndex, int lowVel, int highVel);
    void setRootNote(int sampleIndex, int note);

    // =========================================================================
    // AI自动映射
    // =========================================================================
    void autoMapSamples(const std::vector<juce::File>& files);
    void detectSlices(const juce::File& audioFile, float threshold);

    // =========================================================================
    // 播放控制
    // =========================================================================
    void setLoopMode(LoopMode mode);
    void setCrossfadeLength(int samples);
    void setVolume(float volume);

    // =========================================================================
    // 查找匹配的采样区域
    // =========================================================================
    const SampleZone* findZone(int midiNote, int velocity) const;

    // =========================================================================
    // 参数接口
    // =========================================================================
    int getNumParameters() const override { return 4; }
    float getParameter(int index) const override;
    void setParameter(int index, float value) override;
    juce::String getParameterName(int index) const override;

    // =========================================================================
    // 序列化
    // =========================================================================
    juce::var toJson() const override;
    void fromJson(const juce::var& json) override;

private:
    // 活跃声音结构
    struct Voice {
        int zoneIndex = -1;
        float position = 0.0f;
        float pitchRatio = 1.0f;
        float velocity = 1.0f;
        float pan = 0.0f;
        float volume = 1.0f;
        bool active = false;
        int midiNote = -1;
    };

    Voice& findFreeVoice();
    void processVoice(Voice& voice, juce::AudioBuffer<float>& buffer, int numSamples);
    float readSample(const SampleZone& zone, float position, int channel);

    std::vector<SampleZone> sampleZones_;
    std::array<Voice, kMaxVoices> voices_;
    LoopMode loopMode_ = LoopMode::NoLoop;
    int crossfadeLength_ = 256;
    float volume_ = 1.0f;

    double sampleRate_ = 44100.0;
    int blockSize_ = 256;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultiSampler)
};

// =============================================================================
// DrumSlicer 鼓切片触发器 (对标 VPS Avenger 2)
// =============================================================================
class DrumSlicer : public AudioNode {
public:
    static constexpr int kMaxSlices = 64;
    static constexpr int kMaxBlockSize = 512;

    DrumSlicer(const juce::String& name = "鼓切片器");

    // =========================================================================
    // 音频处理
    // =========================================================================
    void prepareToPlay(double sampleRate, int maxSamplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void releaseResources() override;

    // =========================================================================
    // 切片管理
    // =========================================================================
    void loadLoop(const juce::File& file);
    void detectSlices(float threshold, int minSliceLength);
    void addManualSlice(int samplePos);
    void removeSlice(int index);

    // =========================================================================
    // 切片触发
    // =========================================================================
    void triggerSlice(int index, float velocity);
    void setSlicePitch(int index, float semitones);
    void setSliceDirection(int index, bool reverse);

    // =========================================================================
    // MIDI映射
    // =========================================================================
    void setSliceNote(int index, int midiNote);
    void autoMapToMidi();

    // =========================================================================
    // 播放控制
    // =========================================================================
    void setVolume(float volume);

    // =========================================================================
    // 参数接口
    // =========================================================================
    int getNumParameters() const override { return 1; }
    float getParameter(int index) const override;
    void setParameter(int index, float value) override;
    juce::String getParameterName(int index) const override;

    // =========================================================================
    // 序列化
    // =========================================================================
    juce::var toJson() const override;
    void fromJson(const juce::var& json) override;

private:
    struct Slice {
        int startSample = 0;
        int endSample = 0;
        int midiNote = 60;
        float pitch = 0.0f;     // 半音
        bool reverse = false;
    };

    struct SlicePlayer {
        int sliceIndex = -1;
        float position = 0.0f;
        float pitchRatio = 1.0f;
        float velocity = 1.0f;
        float pan = 0.0f;
        bool active = false;
    };

    float readLoopSample(float position, int channel) const;

    juce::AudioSampleBuffer loopBuffer_;
    bool loopLoaded_ = false;
    std::vector<Slice> slices_;
    std::array<SlicePlayer, kMaxSlices> slicePlayers_;
    float volume_ = 1.0f;

    double sampleRate_ = 44100.0;
    int blockSize_ = 256;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumSlicer)
};

// =============================================================================
// StepSequencer 步进音序器 (对标 VPS Avenger 2)
// =============================================================================
class StepSequencer : public AudioNode {
public:
    static constexpr int kMaxSteps = 64;
    static constexpr int kMaxBlockSize = 512;

    StepSequencer(const juce::String& name = "步进音序器");

    // =========================================================================
    // 音频处理
    // =========================================================================
    void prepareToPlay(double sampleRate, int maxSamplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void releaseResources() override;

    // =========================================================================
    // 序列管理
    // =========================================================================
    void setStepCount(int count);           // 1-64
    void setStepValue(int step, float value); // 0.0-1.0
    void setStepGate(int step, bool gate);
    void setStepTie(int step, bool tie);
    float getStepValue(int step) const;

    // =========================================================================
    // 播放控制
    // =========================================================================
    void setRate(float rate);               // 0.25x - 4.0x (相对BPM)
    void setSwing(float amount);            // 0.0-1.0
    void setDirection(bool forward);        // true=正向, false=反向
    void setRandomMode(bool enabled);
    void setBPM(double bpm);

    // =========================================================================
    // 输出控制
    // =========================================================================
    void setOutputRange(float min, float max);
    void setVolume(float volume);

    // =========================================================================
    // 重置
    // =========================================================================
    void reset();

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
    void advanceStep();

    int stepCount_ = 16;
    std::array<float, kMaxSteps> stepValues_;
    std::array<bool, kMaxSteps> stepGates_;
    std::array<bool, kMaxSteps> stepTies_;
    int currentStep_ = 0;

    float rate_ = 1.0f;
    float swing_ = 0.0f;
    bool forward_ = true;
    bool randomMode_ = false;
    double bpm_ = 120.0;
    float outputMin_ = 0.0f;
    float outputMax_ = 1.0f;
    float volume_ = 1.0f;

    double sampleRate_ = 44100.0;
    int blockSize_ = 256;
    double samplesPerStep_ = 0.0;
    double sampleCounter_ = 0.0;
    float currentOutputValue_ = 0.0f;
    float targetOutputValue_ = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StepSequencer)
};

} // namespace LianCore