// =============================================================================
// LianCore - GrainEnvelopeVisualizer 实现
// =============================================================================
#include "GrainEnvelopeVisualizer.h"

namespace LianCore {

GrainEnvelopeVisualizer::GrainEnvelopeVisualizer() {
    setOpaque(true);
    // 初始化动画粒子
    for (int i = 0; i < 20; ++i) {
        AnimatedGrain g;
        g.x = static_cast<float>(rand()) / RAND_MAX;
        g.y = 0.5f;
        g.alpha = 0.3f + 0.3f * static_cast<float>(rand()) / RAND_MAX;
        g.speed = 0.002f + 0.008f * static_cast<float>(rand()) / RAND_MAX;
        animatedGrains_.push_back(g);
    }
}

GrainEnvelopeVisualizer::~GrainEnvelopeVisualizer() {
    stopAnimation();
}

void GrainEnvelopeVisualizer::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(bgColor_);

    // 网格
    g.setColour(gridColor_);
    float gridStep = bounds.getWidth() / 10.0f;
    for (int i = 1; i < 10; ++i) {
        float x = i * gridStep;
        g.drawLine(x, 0, x, bounds.getHeight(), 0.5f);
    }
    float halfH = bounds.getHeight() / 2.0f;
    g.drawLine(0, halfH, bounds.getWidth(), halfH, 0.5f);

    // 标题
    g.setColour(textColor_.withAlpha(0.5f));
    g.setFont(11.0f);
    g.drawText("粒子包络可视化", bounds.removeFromTop(20).toNearestInt(),
               juce::Justification::centred, false);

    // 包络曲线
    auto curveBounds = bounds.reduced(10, 5);
    drawEnvelopeCurve(g, curveBounds);

    // 动画粒子
    drawGrainParticles(g, curveBounds);

    // 图例
    drawEnvelopeLegend(g, getLocalBounds());
}

void GrainEnvelopeVisualizer::timerCallback() {
    // 更新动画粒子位置
    for (auto& grain : animatedGrains_) {
        grain.x += grain.speed;
        if (grain.x > 1.0f) {
            grain.x = 0.0f;
            grain.y = 0.3f + 0.4f * static_cast<float>(rand()) / RAND_MAX;
        }
        // 在包络曲线附近振动
        float envVal = computeEnvelope(currentType_, grain.x);
        float targetY = 1.0f - envVal;
        grain.y += (targetY - grain.y) * 0.1f;
    }
    repaint();
}

void GrainEnvelopeVisualizer::setEnvelopeType(GrainEnvelopeType type) {
    currentType_ = type;
    repaint();
}

void GrainEnvelopeVisualizer::setGrainDensity(float density) {
    grainDensity_ = density;
    // 调整粒子数量
    int targetCount = juce::jlimit(5, 40, static_cast<int>(density / 2.0f));
    while (animatedGrains_.size() < static_cast<size_t>(targetCount))
        animatedGrains_.push_back({0.5f, 0.5f, 0.3f, 0.005f});
    while (animatedGrains_.size() > static_cast<size_t>(targetCount))
        animatedGrains_.pop_back();
}

void GrainEnvelopeVisualizer::setGrainSize(float sizeMs) {
    grainSize_ = sizeMs;
}

void GrainEnvelopeVisualizer::startAnimation(int fps) {
    animationFps_ = fps;
    startTimerHz(fps);
}

void GrainEnvelopeVisualizer::stopAnimation() {
    stopTimer();
}

void GrainEnvelopeVisualizer::drawEnvelopeCurve(juce::Graphics& g, juce::Rectangle<float> bounds) {
    const int numPoints = 256;
    juce::Path path;
    path.startNewSubPath(bounds.getX(), bounds.getBottom());

    for (int i = 0; i <= numPoints; ++i) {
        float t = static_cast<float>(i) / numPoints;
        float envVal = computeEnvelope(currentType_, t);
        float x = bounds.getX() + t * bounds.getWidth();
        float y = bounds.getBottom() - envVal * bounds.getHeight();
        path.lineTo(x, y);
    }

    g.setColour(curveColor_.withAlpha(0.3f));
    g.fillPath(path);

    g.setColour(curveColor_);
    g.strokePath(path, juce::PathStrokeType(2.0f));
}

void GrainEnvelopeVisualizer::drawGrainParticles(juce::Graphics& g, juce::Rectangle<float> bounds) {
    for (const auto& grain : animatedGrains_) {
        float x = bounds.getX() + grain.x * bounds.getWidth();
        float y = bounds.getBottom() - grain.y * bounds.getHeight();
        float size = 3.0f + grain.alpha * 4.0f;

        g.setColour(particleColor_.withAlpha(grain.alpha));
        g.fillEllipse(x - size / 2, y - size / 2, size, size);
    }
}

void GrainEnvelopeVisualizer::drawEnvelopeLegend(juce::Graphics& g, juce::Rectangle<int> bounds) {
    auto legendArea = bounds.removeFromBottom(20);
    g.setColour(textColor_.withAlpha(0.6f));
    g.setFont(10.0f);
    g.drawText(juce::String("包络: ") + envelopeTypeToString(currentType_) +
               " | 密度: " + juce::String(grainDensity_, 1) + "粒/秒",
               legendArea, juce::Justification::centred, true);
}

} // namespace LianCore