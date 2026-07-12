// =============================================================================
// LianCore - WavetableBank 波表数据银行
// 管理256帧波表数据，支持WAV导入/导出、帧间插值
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <string>

namespace LianCore {

// =============================================================================
// WavetableBank - 波表数据存储与管理
// =============================================================================
class WavetableBank {
public:
    static constexpr int kMaxFrames = 256;
    static constexpr int kFrameSize = 2048;
    static constexpr int kTotalSamples = kMaxFrames * kFrameSize; // 524,288 samples

    WavetableBank();
    ~WavetableBank() = default;

    // =========================================================================
    // 数据访问
    // =========================================================================
    int getNumFrames() const { return numFrames_; }
    int getFrameSize() const { return kFrameSize; }
    int getTotalSamples() const { return numFrames_ * kFrameSize; }

    // 获取指定帧的波形数据
    const float* getFrameData(int frameIndex) const;
    float* getFrameData(int frameIndex);

    // 获取插值后的波形数据 (用于平滑帧过渡)
    void getInterpolatedFrame(float framePosition, float* output, int numSamples);

    // =========================================================================
    // 波表管理
    // =========================================================================
    void clear();
    void setNumFrames(int numFrames);
    bool isEmpty() const { return numFrames_ == 0; }

    // 设置整个波表数据
    void setData(const float* data, int numFrames);

    // 设置单帧数据
    void setFrameData(int frameIndex, const float* data, int size);

    // =========================================================================
    // 文件 I/O
    // =========================================================================
    bool loadFromWavFile(const juce::File& file);
    bool saveToWavFile(const juce::File& file) const;

    // =========================================================================
    // 波形生成
    // =========================================================================
    void generateSineWave(int numHarmonics = 1);
    void generateSawWave(int numHarmonics = 64);
    void generateSquareWave(int numHarmonics = 64);
    void generateTriangleWave(int numHarmonics = 64);

    // 从谐波序列生成波表 (每帧对应不同谐波含量)
    void generateFromHarmonics(const std::vector<std::vector<float>>& harmonicAmplitudes);

    // =========================================================================
    // AI波表生成接口
    // =========================================================================
    void generateFromAIParams(const std::vector<float>& aiParams);

    // =========================================================================
    // 信息
    // =========================================================================
    juce::String getDescription() const;
    size_t getMemoryUsage() const;

private:
    // 波表数据: 256帧 × 2048采样 = 524,288个float (~2MB)
    juce::AudioBuffer<float> wavetableData_;
    int numFrames_ = 0;

    // 归一化波表
    void normalizeFrame(int frameIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WavetableBank)
};

} // namespace LianCore