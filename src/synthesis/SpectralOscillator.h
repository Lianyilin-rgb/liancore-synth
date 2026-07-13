// =============================================================================
// LianCore - SpectralOscillator 频谱振荡器 (对标 Vital)
// 基于FFT的频谱重合成、拉伸、移调、谐波混合
// =============================================================================
#pragma once

#include "../core/AudioNode.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <complex>

namespace LianCore {

// =============================================================================
// SpectralOscillator - 频谱振荡器
// =============================================================================
class SpectralOscillator : public AudioNode {
public:
    static constexpr int kMinFftSize = 256;
    static constexpr int kMaxFftSize = 4096;
    static constexpr int kMaxOverlap = 8;
    static constexpr int kMaxBlockSize = 512;

    SpectralOscillator(const juce::String& name = "频谱振荡器");

    // =========================================================================
    // 音频处理
    // =========================================================================
    void prepareToPlay(double sampleRate, int maxSamplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void releaseResources() override;

    // =========================================================================
    // FFT参数
    // =========================================================================
    void setFftSize(int size);      // 256-4096
    void setOverlap(int overlap);   // 2x, 4x, 8x

    // =========================================================================
    // 频谱操作
    // =========================================================================
    void setSpectralStretch(float amount);   // 0.5x - 4.0x 频谱拉伸
    void setSpectralShift(float semitones);  // ±24半音 频谱移调
    void setHarmonicBlend(float blend);      // 0.0 - 1.0 谐波混合

    // =========================================================================
    // 共振峰
    // =========================================================================
    void setFormantShift(float amount);      // ±12半音
    void setFormantPreset(int index);        // 0=A, 1=E, 2=I, 3=O, 4=U

    // =========================================================================
    // 音频参考
    // =========================================================================
    void loadReferenceSpectrum(const juce::AudioSampleBuffer& audio);
    void applyAiSpectralMapping(const std::vector<float>& harmonicParams);

    // =========================================================================
    // 播放控制
    // =========================================================================
    void setFrequency(float hz);
    void setVolume(float volume);

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
    // 处理频谱帧
    void processSpectralFrame();
    // 频谱拉伸
    void applySpectralStretch(std::vector<float>& magnitudes, float stretch);
    // 频谱移调
    void applySpectralShift(std::vector<float>& magnitudes, float shift);
    // 谐波混合
    void applyHarmonicBlend(std::vector<float>& magnitudes, float blend);
    // 共振峰滤波
    void applyFormantFilter(std::vector<float>& magnitudes, float shift);
    // IFFT重构
    void resynthesize(juce::AudioBuffer<float>& buffer, int numSamples);

    // FFT引擎
    std::unique_ptr<juce::dsp::FFT> fft_;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window_;
    
    int fftSize_ = 2048;
    int overlap_ = 4;
    int hopSize_ = 512;

    // 频谱数据
    std::vector<float> spectralMagnitudes_;
    std::vector<float> spectralPhases_;
    std::vector<float> harmonicTemplate_;     // AI谐波模板
    std::vector<float> referenceSpectrum_;    // 参考频谱

    // 播放参数
    float frequency_ = 440.0f;
    float volume_ = 1.0f;
    float spectralStretch_ = 1.0f;
    float spectralShift_ = 0.0f;
    float harmonicBlend_ = 0.0f;
    float formantShift_ = 0.0f;
    int formantPreset_ = 0;

    // 重叠-相加缓冲区
    std::vector<std::vector<float>> overlapBuffers_;
    int overlapWritePos_ = 0;

    // 相位累加器
    float phase_ = 0.0f;
    double sampleRate_ = 44100.0;
    int blockSize_ = 256;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralOscillator)
};

} // namespace LianCore