// =============================================================================
// LianCore - ChaoticEnvelope 实现
// =============================================================================
#include "ChaoticEnvelope.h"
#include "../utils/AudioUtils.h"

namespace LianCore {

ChaoticEnvelope::ChaoticEnvelope(const juce::String& name)
    : AudioNode(NodeType::ChaosEnvelope, name) {}

void ChaoticEnvelope::prepareToPlay(double sampleRate, int maxSamples) {
    AudioNode::prepareToPlay(sampleRate, maxSamples);
    currentValue_ = 0.0f;
    targetValue_ = 0.0f;
    triggered_ = false;
    released_ = false;
    phase_ = 0.0f;
    holdCounter_ = 0.0f;
}

void ChaoticEnvelope::releaseResources() {
    AudioNode::releaseResources();
    currentValue_ = 0.0f;
    targetValue_ = 0.0f;
    triggered_ = false;
    released_ = false;
    phase_ = 0.0f;
    holdCounter_ = 0.0f;
}

void ChaoticEnvelope::trigger() {
    triggered_ = true;
    released_ = false;
    phase_ = 0.0f;
    holdCounter_ = 0.0f;
    targetValue_ = nextChaoticValue();
}

void ChaoticEnvelope::release() {
    if (triggered_) {
        released_ = true;
    }
}

void ChaoticEnvelope::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    int numSamples = buffer.getNumSamples();

    // 处理MIDI触发
    for (const auto& msg : midi) {
        auto message = msg.getMessage();
        if (message.isNoteOn()) {
            trigger();
        } else if (message.isNoteOff()) {
            release();
        }
    }

    // 没有触发时输出0
    if (!triggered_) {
        currentValue_ = 0.0f;
        auto& output = getOutputBuffer(0);
        output.clear();
        return;
    }

    // 保持阶段
    if (holdSamples_ > 0 && holdCounter_ < static_cast<float>(holdSamples_)) {
        holdCounter_ += static_cast<float>(numSamples);
        auto& output = getOutputBuffer(0);
        output.clear();
        output.getWritePointer(0)[0] = currentValue_;
        return;
    }

    // 速度因子: 0→慢(0.001), 1→快(0.05) 每采样
    float speedFactor = 0.001f + speed_ * 0.049f;

    for (int i = 0; i < numSamples; ++i) {
        switch (mode_) {
            case ChaosEnvMode::ChaoticDecay: {
                // 混沌衰减: 每次触发产生带有随机扰动的指数衰减
                float attackTarget = 0.5f + chaosAmount_ * 0.5f; // 攻击目标水平
                if (!released_ && phase_ < 1.0f) {
                    // 攻击阶段
                    phase_ += speedFactor * 0.1f; // 慢速攻击
                    currentValue_ = std::pow(phase_, 0.3f + chaosAmount_ * 0.5f) * attackTarget;
                    currentValue_ = AudioUtils::clamp(currentValue_, 0.0f, 1.0f);
                } else {
                    // 衰减阶段
                    float decayRate = 0.001f + speedFactor * 0.08f;
                    float chaosPerturbation = (chaosAmount_ * 0.05f) *
                        std::sin(phase_ * 137.508f) * std::cos(phase_ * 99.73f);
                    currentValue_ = currentValue_ * (1.0f - decayRate) + chaosPerturbation;
                    if (currentValue_ < 0.001f) {
                        currentValue_ = 0.0f;
                        triggered_ = false;
                    }
                    phase_ += speedFactor * 0.01f;
                }
                break;
            }

            case ChaosEnvMode::BurstGenerator: {
                // 脉冲发生器: 混沌间隔产生脉冲
                phase_ += speedFactor * 0.1f;
                float burstInterval = 0.5f + (1.0f - chaosAmount_) * 2.0f;
                if (phase_ > burstInterval) {
                    phase_ = 0.0f;
                    currentValue_ = 1.0f;
                } else {
                    // 脉冲衰减
                    currentValue_ *= 0.9f;
                    float chaosJitter = chaosAmount_ * AudioUtils::getThreadLocalRNG().nextFloat(-0.05f, 0.05f);
                    currentValue_ += chaosJitter;
                    currentValue_ = AudioUtils::clamp(currentValue_, 0.0f, 1.0f);
                }
                if (released_) {
                    currentValue_ *= 0.95f;
                    if (currentValue_ < 0.001f) triggered_ = false;
                }
                break;
            }

            case ChaosEnvMode::RandomWalk: {
                // 随机游走: 混沌驱动的连续变化
                float step = speedFactor * 0.02f;
                float chaosStep = (chaosAmount_ * 2.0f - 1.0f) * step;
                // 使用混沌逻辑映射生成步长
                float logisticX = currentValue_ * 3.9f * (1.0f - currentValue_);
                float direction = (logisticX > 0.5f) ? 1.0f : -1.0f;
                currentValue_ += direction * chaosStep;
                currentValue_ = AudioUtils::clamp(currentValue_, -1.0f, 1.0f);
                if (released_) {
                    currentValue_ *= 0.98f;
                    if (std::abs(currentValue_) < 0.001f) triggered_ = false;
                }
                break;
            }

            case ChaosEnvMode::StrangeAttractor: {
                // 奇异吸引子映射: 使用 Lorenz 轨道
                phase_ += speedFactor * 0.05f;
                // 简化 Lorenz 在包络空间的映射
                float x = std::sin(phase_ * 10.0f * (1.0f + chaosAmount_));
                float y = std::cos(phase_ * 28.0f * (1.0f + chaosAmount_ * 0.5f));
                float z = std::sin(phase_ * 8.0f / 3.0f);
                currentValue_ = (x + y + z) / 3.0f;
                currentValue_ = AudioUtils::clamp(currentValue_, -1.0f, 1.0f);
                if (released_) {
                    currentValue_ *= 0.95f;
                    if (std::abs(currentValue_) < 0.001f) triggered_ = false;
                }
                break;
            }
        }
    }

    auto& output = getOutputBuffer(0);
    output.clear();
    output.getWritePointer(0)[0] = currentValue_;
}

// =============================================================================
// 辅助方法
// =============================================================================
float ChaoticEnvelope::nextChaoticValue() {
    auto& rng = AudioUtils::getThreadLocalRNG();
    float baseValue = rng.nextFloat(0.3f, 1.0f);
    // 混沌扰动
    float chaos = chaosAmount_ * rng.nextFloat(-0.3f, 0.3f);
    return AudioUtils::clamp(baseValue + chaos, 0.0f, 1.0f);
}

void ChaoticEnvelope::resetPhase() {
    phase_ = 0.0f;
}

// =============================================================================
// 参数设置
// =============================================================================
void ChaoticEnvelope::setMode(ChaosEnvMode mode) {
    mode_ = mode;
    resetPhase();
}

void ChaoticEnvelope::setChaosAmount(float amount) {
    chaosAmount_ = AudioUtils::clamp(amount, 0.0f, 1.0f);
}

void ChaoticEnvelope::setSpeed(float speed) {
    speed_ = AudioUtils::clamp(speed, 0.0f, 1.0f);
}

void ChaoticEnvelope::setHold(float hold) {
    hold_ = AudioUtils::clamp(hold, 0.0f, 1.0f);
    holdSamples_ = static_cast<int>(hold_ * 48000.0f); // 0 → 0 采样, 1 → 48000 采样
}

// =============================================================================
// 参数接口
// =============================================================================
float ChaoticEnvelope::getParameter(int index) const {
    switch (index) {
        case 0: return static_cast<float>(mode_) / 3.0f;
        case 1: return chaosAmount_;
        case 2: return speed_;
        case 3: return hold_;
        default: return 0.0f;
    }
}

void ChaoticEnvelope::setParameter(int index, float value) {
    switch (index) {
        case 0: setMode(static_cast<ChaosEnvMode>(static_cast<int>(value * 3.0f))); break;
        case 1: setChaosAmount(value); break;
        case 2: setSpeed(value); break;
        case 3: setHold(value); break;
        default: break;
    }
}

juce::String ChaoticEnvelope::getParameterName(int index) const {
    switch (index) {
        case 0: return "模式";
        case 1: return "混沌程度";
        case 2: return "速度";
        case 3: return "保持";
        default: return "未知";
    }
}

// =============================================================================
// 序列化
// =============================================================================
juce::var ChaoticEnvelope::toJson() const {
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

void ChaoticEnvelope::fromJson(const juce::var& json) {
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