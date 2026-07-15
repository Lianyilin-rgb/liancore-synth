// =============================================================================
// LianCore - ChaoticLFO 实现
// =============================================================================
#include "ChaoticLFO.h"
#include "../utils/AudioUtils.h"

namespace LianCore {

ChaoticLFO::ChaoticLFO(const juce::String& name)
    : AudioNode(NodeType::ChaosLFO, name) {}

void ChaoticLFO::prepareToPlay(double sampleRate, int maxSamples) {
    AudioNode::prepareToPlay(sampleRate, maxSamples);
    resetChaosState();
}

void ChaoticLFO::releaseResources() {
    AudioNode::releaseResources();
    resetChaosState();
}

void ChaoticLFO::resetChaosState() {
    // 使用微小扰动确保不同实例产生不同轨道
    x_ = 0.5f + AudioUtils::getThreadLocalRNG().nextFloat(-0.01f, 0.01f);
    y_ = 0.5f;
    z_ = 0.5f;
    lastOutput_ = 0.0f;
    lastRaw_ = 0.0f;
    sampleCounter_ = 0;
}

void ChaoticLFO::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    int numSamples = buffer.getNumSamples();

    // 更新速率: rate_ 0→每100采样, 1→每采样
    updateInterval_ = std::max(1, static_cast<int>((1.0f - rate_) * 100.0f));

    for (int i = 0; i < numSamples; ++i) {
        // 每 updateInterval_ 个采样步进混沌系统
        if (sampleCounter_ <= 0) {
            float rawValue = 0.0f;
            switch (chaosMap_) {
                case ChaosMap::Logistic:  rawValue = stepLogistic(); break;
                case ChaosMap::Lorenz:    rawValue = stepLorenz(); break;
                case ChaosMap::Henon:     rawValue = stepHenon(); break;
                case ChaosMap::Tent:      rawValue = stepTent(); break;
                case ChaosMap::Rossler:   rawValue = stepRossler(); break;
            }
            lastRaw_ = rawValue;
            sampleCounter_ = updateInterval_;
        } else {
            sampleCounter_--;
        }

        // 平滑 + 深度
        if (smooth_ > 0.001f) {
            float smoothingFactor = std::exp(-1.0f / (smooth_ * 0.1f * static_cast<float>(sampleRate_)));
            lastOutput_ = lastOutput_ * smoothingFactor + lastRaw_ * depth_ * (1.0f - smoothingFactor);
        } else {
            lastOutput_ = lastRaw_ * depth_;
        }
    }

    // 输出到端口
    auto& output = getOutputBuffer(0);
    output.clear();
    output.getWritePointer(0)[0] = lastOutput_;
}

// =============================================================================
// 混沌映射实现
// =============================================================================

// Logistic Map: x_{n+1} = r * x_n * (1 - x_n)
// r ∈ [3.57, 4.0] 产生混沌，chaosAmount_ 映射到这个范围
float ChaoticLFO::stepLogistic() {
    float r = 3.57f + chaosAmount_ * 0.43f; // 3.57 → 4.0
    x_ = r * x_ * (1.0f - x_);
    // 归一化到 [-1, 1]: 原始输出在 [0, 1]，映射到 [-1, 1]
    return (x_ - 0.5f) * 2.0f;
}

// Lorenz Attractor (离散化):
// dx/dt = sigma * (y - x), dy/dt = x * (rho - z) - y, dz/dt = x*y - beta*z
// 经典参数: sigma=10, rho=28, beta=8/3
float ChaoticLFO::stepLorenz() {
    static constexpr float sigma = 10.0f;
    static constexpr float rho = 28.0f;
    static constexpr float beta = 8.0f / 3.0f;
    float dt = 0.005f; // 离散时间步长

    float dx = sigma * (y_ - x_);
    float dy = x_ * (rho - z_) - y_;
    float dz = x_ * y_ - beta * z_;

    // 混沌程度影响 rho 参数: 24→28
    float rhoMod = 24.0f + chaosAmount_ * 4.0f;
    dy = x_ * (rhoMod - z_) - y_;

    x_ += dx * dt;
    y_ += dy * dt;
    z_ += dz * dt;

    // 归一化: x 分量通常在 [-20, 20]
    return AudioUtils::clamp(x_ / 20.0f, -1.0f, 1.0f);
}

// Henon Map:
// x_{n+1} = 1 - a * x_n^2 + y_n
// y_{n+1} = b * x_n
// 经典参数: a=1.4, b=0.3
float ChaoticLFO::stepHenon() {
    float a = 1.0f + chaosAmount_ * 0.4f; // 1.0 → 1.4
    float b = 0.3f;
    float newX = 1.0f - a * x_ * x_ + y_;
    float newY = b * x_;
    x_ = newX;
    y_ = newY;

    // 归一化: x 通常在 [-1.5, 1.5]
    return AudioUtils::clamp(x_ / 1.5f, -1.0f, 1.0f);
}

// Tent Map:
// x_{n+1} = mu * min(x_n, 1 - x_n), mu ∈ [1, 2]
float ChaoticLFO::stepTent() {
    float mu = 1.0f + chaosAmount_; // 1.0 → 2.0
    x_ = mu * std::min(x_, 1.0f - x_);
    return (x_ - 0.5f) * 2.0f;
}

// Rossler Attractor:
// dx/dt = -y - z, dy/dt = x + a*y, dz/dt = b + z*(x - c)
float ChaoticLFO::stepRossler() {
    float a = 0.2f;
    float b = 0.2f;
    float c = 5.7f + chaosAmount_ * 4.3f; // 5.7 → 10.0 (混沌程度越高)
    float dt = 0.01f;

    float dx = -y_ - z_;
    float dy = x_ + a * y_;
    float dz = b + z_ * (x_ - c);

    x_ += dx * dt;
    y_ += dy * dt;
    z_ += dz * dt;

    // 归一化: x 通常在 [-15, 15]
    return AudioUtils::clamp(x_ / 15.0f, -1.0f, 1.0f);
}

// =============================================================================
// 参数设置
// =============================================================================
void ChaoticLFO::setChaosMap(ChaosMap map) {
    if (chaosMap_ != map) {
        chaosMap_ = map;
        resetChaosState();
    }
}

void ChaoticLFO::setChaosAmount(float amount) {
    chaosAmount_ = AudioUtils::clamp(amount, 0.0f, 1.0f);
}

void ChaoticLFO::setRate(float rate) {
    rate_ = AudioUtils::clamp(rate, 0.0f, 1.0f);
}

void ChaoticLFO::setSmooth(float amount) {
    smooth_ = AudioUtils::clamp(amount, 0.0f, 1.0f);
}

void ChaoticLFO::setDepth(float depth) {
    depth_ = AudioUtils::clamp(depth, 0.0f, 1.0f);
}

// =============================================================================
// 参数接口
// =============================================================================
float ChaoticLFO::getParameter(int index) const {
    switch (index) {
        case 0: return static_cast<float>(chaosMap_) / 4.0f;
        case 1: return chaosAmount_;
        case 2: return rate_;
        case 3: return smooth_;
        case 4: return depth_;
        default: return 0.0f;
    }
}

void ChaoticLFO::setParameter(int index, float value) {
    switch (index) {
        case 0: setChaosMap(static_cast<ChaosMap>(static_cast<int>(value * 4.0f))); break;
        case 1: setChaosAmount(value); break;
        case 2: setRate(value); break;
        case 3: setSmooth(value); break;
        case 4: setDepth(value); break;
        default: break;
    }
}

juce::String ChaoticLFO::getParameterName(int index) const {
    switch (index) {
        case 0: return "混沌映射";
        case 1: return "混沌程度";
        case 2: return "速率";
        case 3: return "平滑";
        case 4: return "深度";
        default: return "未知";
    }
}

// =============================================================================
// 序列化
// =============================================================================
juce::var ChaoticLFO::toJson() const {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("nodeId", nodeId_);
    obj->setProperty("nodeType", nodeTypeToString(nodeType_));
    obj->setProperty("name", name_);
    juce::Array<juce::var> params;
    for (int i = 0; i < getNumParameters(); ++i) {
        juce::DynamicObject::Ptr paramObj = new juce::DynamicObject();
        paramObj->setProperty("name", getParameterName(i));
        paramObj->setProperty("value", getParameter(i));
        params.add(paramObj.get());
    }
    obj->setProperty("parameters", params);
    return juce::var(obj.get());
}

void ChaoticLFO::fromJson(const juce::var& json) {
    if (auto* obj = json.getDynamicObject()) {
        name_ = obj->getProperty("name").toString();
        if (auto* params = obj->getProperty("parameters").getArray()) {
            for (int i = 0; i < params->size() && i < getNumParameters(); ++i) {
                if (auto* paramObj = (*params)[i].getDynamicObject()) {
                    setParameter(i, paramObj->getProperty("value"));
                }
            }
        }
    }
}

} // namespace LianCore