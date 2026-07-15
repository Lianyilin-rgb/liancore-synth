// =============================================================================
// LianCore - SpectralWarper 频谱变形模块 (对标 Vital 频谱变形)
// 5种变形模式: Stretch, Shift, Symmetrize, Quantize, Fold
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <cmath>

namespace LianCore {

// =============================================================================
// SpectralWarper - 频谱变形处理器
// =============================================================================
class SpectralWarper {
public:
    // 变形模式
    enum class Mode {
        Stretch = 0,    // 拉伸/压缩频谱 (0.5x-4.0x)
        Shift,          // 频移 (±24半音)
        Symmetrize,     // 对称化频谱镜像
        Quantize,       // 量化频谱幅度
        Fold            // 波形折叠 (高频回折)
    };

    static constexpr int kNumModes = 5;

    SpectralWarper();

    // 模式与参数
    void setMode(Mode mode);
    Mode getMode() const { return mode_; }
    static juce::String getModeName(Mode mode);
    static juce::String getModeName(int modeIndex);

    void setAmount(float amount);       // 0.0-1.0 归一化
    float getAmount() const { return amount_; }

    // 直接设置原始参数值
    void setStretchAmount(float stretch);   // 0.5-4.0
    void setShiftAmount(float semitones);   // ±24
    void setSymmetrizeAmount(float amount); // 0.0-1.0
    void setQuantizeAmount(float amount);   // 0.0-1.0
    void setFoldAmount(float amount);       // 0.0-1.0

    // 核心处理: 对频谱幅度数组进行变形
    void process(std::vector<float>& magnitudes, double sampleRate, int fftSize);

private:
    // 频谱拉伸 (已有算法, 重映射bin索引)
    void applyStretch(std::vector<float>& magnitudes, float stretch);

    // 频谱移调 (已有算法, 平移bin)
    void applyShift(std::vector<float>& magnitudes, float semitones,
                    double sampleRate, int fftSize);

    // 对称化: 将频谱关于中心频率镜像
    // amount=0: 无变化, amount=1: 完全对称
    void applySymmetrize(std::vector<float>& magnitudes, float amount,
                         double sampleRate, int fftSize);

    // 量化: 将频谱幅度离散化到指定数量的等级
    // amount=0: 2级(开/关), amount=1: 64级(接近原始)
    void applyQuantize(std::vector<float>& magnitudes, float amount);

    // 折叠: 将超过奈奎斯特频率的高频能量折叠回低频
    // amount=0: 无折叠(奈奎斯特), amount=1: 最大折叠(0Hz)
    void applyFold(std::vector<float>& magnitudes, float amount,
                   double sampleRate, int fftSize);

    // 专用参数
    Mode mode_ = Mode::Stretch;
    float amount_ = 0.0f;
    float stretchAmount_ = 1.0f;
    float shiftAmount_ = 0.0f;
    float symmetrizeAmount_ = 0.0f;
    float quantizeAmount_ = 0.0f;
    float foldAmount_ = 0.0f;
};

} // namespace LianCore