// =============================================================================
// LianCore - GrainEnvelopeVisualizer 粒子包络可视化组件
// 实时绘制5种粒子包络形状：Hann, Hamming, Blackman, Gaussian, Triangle
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

namespace LianCore {

// =============================================================================
// 粒子包络类型 (扩展)
// =============================================================================
enum class GrainEnvelopeType {
    Hann = 0,         // 汉宁窗: cos²(π*(x-0.5))
    Hamming,          // 汉明窗: 0.54 - 0.46*cos(2πx)
    Triangle,         // 三角窗: 1 - |2x-1|
    Exponential,      // 指数衰减: exp(-x*5)
    Rectangular,      // 矩形窗: 1
    Blackman,         // 布莱克曼窗: 0.42 - 0.5*cos(2πx) + 0.08*cos(4πx)
    Gaussian,         // 高斯窗: exp(-0.5*((x-0.5)/0.15)²)
    Tukey,            // 塔基窗: cos锥形+平坦顶部
    Cosine,           // 余弦窗: sin(πx)
    Welch             // 韦尔奇窗: 1 - ((x-0.5)/0.5)²
};

inline const char* envelopeTypeToString(GrainEnvelopeType t) {
    switch (t) {
        case GrainEnvelopeType::Hann: return "Hann";
        case GrainEnvelopeType::Hamming: return "Hamming";
        case GrainEnvelopeType::Triangle: return "Triangle";
        case GrainEnvelopeType::Exponential: return "Exponential";
        case GrainEnvelopeType::Rectangular: return "Rectangular";
        case GrainEnvelopeType::Blackman: return "Blackman";
        case GrainEnvelopeType::Gaussian: return "Gaussian";
        case GrainEnvelopeType::Tukey: return "Tukey";
        case GrainEnvelopeType::Cosine: return "Cosine";
        case GrainEnvelopeType::Welch: return "Welch";
        default: return "Unknown";
    }
}

inline int envelopeTypeCount() { return 10; }

// =============================================================================
// 包络计算函数 (静态，可被GranularPlayer复用)
// =============================================================================
inline float computeEnvelope(GrainEnvelopeType type, float position) {
    // position: 0.0 ~ 1.0
    switch (type) {
        case GrainEnvelopeType::Hann: {
            float angle = juce::MathConstants<float>::pi * (position - 0.5f);
            float c = std::cos(angle);
            return c * c;
        }
        case GrainEnvelopeType::Hamming:
            return 0.54f - 0.46f * std::cos(2.0f * juce::MathConstants<float>::pi * position);
        case GrainEnvelopeType::Triangle:
            return 1.0f - std::abs(2.0f * position - 1.0f);
        case GrainEnvelopeType::Exponential:
            return std::exp(-position * 5.0f);
        case GrainEnvelopeType::Rectangular:
            return 1.0f;
        case GrainEnvelopeType::Blackman: {
            float a = 2.0f * juce::MathConstants<float>::pi * position;
            return 0.42f - 0.5f * std::cos(a) + 0.08f * std::cos(2.0f * a);
        }
        case GrainEnvelopeType::Gaussian: {
            float sigma = 0.15f;
            float x = (position - 0.5f) / sigma;
            return std::exp(-0.5f * x * x);
        }
        case GrainEnvelopeType::Tukey: {
            float alpha = 0.5f;
            if (position < alpha / 2.0f)
                return 0.5f * (1.0f + std::cos(juce::MathConstants<float>::pi * (2.0f * position / alpha - 1.0f)));
            else if (position > 1.0f - alpha / 2.0f)
                return 0.5f * (1.0f + std::cos(juce::MathConstants<float>::pi * (2.0f * (1.0f - position) / alpha - 1.0f)));
            return 1.0f;
        }
        case GrainEnvelopeType::Cosine:
            return std::sin(juce::MathConstants<float>::pi * position);
        case GrainEnvelopeType::Welch: {
            float x = (position - 0.5f) / 0.5f;
            return 1.0f - x * x;
        }
        default:
            return 0.0f;
    }
}

// =============================================================================
// GrainEnvelopeVisualizer - 粒子包络可视化组件
// =============================================================================
class GrainEnvelopeVisualizer : public juce::Component,
                                public juce::Timer {
public:
    GrainEnvelopeVisualizer();
    ~GrainEnvelopeVisualizer() override;

    void paint(juce::Graphics& g) override;
    void timerCallback() override;

    // 设置显示的包络类型
    void setEnvelopeType(GrainEnvelopeType type);
    GrainEnvelopeType getEnvelopeType() const { return currentType_; }

    // 设置粒子密度和大小 (用于动画)
    void setGrainDensity(float density);
    void setGrainSize(float sizeMs);

    // 启动/停止动画
    void startAnimation(int fps = 30);
    void stopAnimation();

private:
    void drawEnvelopeCurve(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawGrainParticles(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawEnvelopeLegend(juce::Graphics& g, juce::Rectangle<int> bounds);

    GrainEnvelopeType currentType_ = GrainEnvelopeType::Hann;
    float grainDensity_ = 30.0f;
    float grainSize_ = 50.0f;

    // 动画粒子
    struct AnimatedGrain {
        float x, y, alpha, speed;
    };
    std::vector<AnimatedGrain> animatedGrains_;
    int animationFps_ = 30;

    // 颜色
    juce::Colour bgColor_         = juce::Colour(0xff1a1a2e);
    juce::Colour curveColor_      = juce::Colour(0xff00d4ff);
    juce::Colour particleColor_   = juce::Colour(0xffff6b6b);
    juce::Colour gridColor_       = juce::Colour(0x30ffffff);
    juce::Colour textColor_       = juce::Colour(0xffe0e0e0);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GrainEnvelopeVisualizer)
};

} // namespace LianCore