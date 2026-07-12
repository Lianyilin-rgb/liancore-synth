// =============================================================================
// LianCore - StepSequencer 步进音序器 实现
// 步进式调制信号发生器，支持摆动、随机、正反向等模式
// =============================================================================
#include "MultiSampler.h"
#include "../utils/AudioUtils.h"

namespace LianCore {

// =============================================================================
// 构造 / 析构
// =============================================================================
StepSequencer::StepSequencer(const juce::String& name)
    : AudioNode(NodeType::StepSequencer, name) {
    // 初始化所有步进值为0、门控为false、连音为false
    for (int i = 0; i < kMaxSteps; ++i) {
        stepValues_[i] = 0.0f;
        stepGates_[i] = false;
        stepTies_[i] = false;
    }
    currentStep_ = 0;
    samplesPerStep_ = 0.0;
    sampleCounter_ = 0.0;
    currentOutputValue_ = 0.0f;
    targetOutputValue_ = 0.0f;
}

// =============================================================================
// 音频处理
// =============================================================================
void StepSequencer::prepareToPlay(double sampleRate, int maxSamplesPerBlock) {
    AudioNode::prepareToPlay(sampleRate, maxSamplesPerBlock);
    sampleRate_ = sampleRate;
    blockSize_ = maxSamplesPerBlock;

    // 根据BPM计算每步采样数
    // 一拍 = 四分之一音符，一步 = 1/16 音符（默认）
    // samplesPerStep = (60 / BPM) * sampleRate / 4  (每拍采样数 / 4 = 16分音符)
    if (bpm_ > 0.0) {
        samplesPerStep_ = (60.0 / bpm_) * sampleRate_ / 4.0;
    } else {
        samplesPerStep_ = sampleRate_ * 0.25; // 默认约250ms @ 44.1kHz
    }
    samplesPerStep_ = std::max(samplesPerStep_, 1.0);

    sampleCounter_ = 0.0;
}

void StepSequencer::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midi*/) {
    auto& output = getOutputBuffer(0);
    output.clear();

    int numSamples = buffer.getNumSamples();
    float* outL = output.getWritePointer(0);
    float* outR = output.getWritePointer(1);

    if (stepCount_ <= 0 || samplesPerStep_ <= 0.0) {
        AudioUtils::clearBufferSIMD(outL, numSamples);
        AudioUtils::clearBufferSIMD(outR, numSamples);
        return;
    }

    // 平滑系数：一阶低通指数平滑，约10ms过渡时间
    float smoothingCoeff = std::exp(-1.0f / (static_cast<float>(sampleRate_) * 0.01f));
    smoothingCoeff = AudioUtils::clamp(smoothingCoeff, 0.0f, 1.0f);

    // 逐采样处理
    for (int i = 0; i < numSamples; ++i) {
        // 步进计数器累加
        sampleCounter_ += 1.0;

        // 到达步进边界时切换到下一步
        if (sampleCounter_ >= samplesPerStep_) {
            sampleCounter_ -= samplesPerStep_;
            advanceStep();
        }

        // 当前步的目标值（仅门控开启时有效）
        int step = currentStep_;
        if (step >= 0 && step < stepCount_) {
            if (stepGates_[step]) {
                // 检查连音：如果当前步有连音且上一步门控开启，沿用上一步的值
                if (stepTies_[step] && step > 0 && stepGates_[step - 1]) {
                    targetOutputValue_ = stepValues_[step - 1];
                } else {
                    targetOutputValue_ = stepValues_[step];
                }
            } else {
                // 门控关闭：输出0（静音）
                targetOutputValue_ = 0.0f;
            }
        }

        // 指数平滑过渡到目标值
        currentOutputValue_ = smoothingCoeff * currentOutputValue_ + (1.0f - smoothingCoeff) * targetOutputValue_;

        // 映射到输出范围
        float mappedValue = AudioUtils::mapRange(currentOutputValue_, 0.0f, 1.0f, outputMin_, outputMax_);

        // 写入输出缓冲区
        outL[i] = mappedValue * volume_;
        outR[i] = mappedValue * volume_;
    }
}

void StepSequencer::releaseResources() {
    AudioNode::releaseResources();
    currentStep_ = 0;
    sampleCounter_ = 0.0;
    currentOutputValue_ = 0.0f;
    targetOutputValue_ = 0.0f;
}

// =============================================================================
// 步进推进
// =============================================================================
void StepSequencer::advanceStep() {
    if (stepCount_ <= 0) return;

    if (randomMode_) {
        // 随机模式：随机选择一步
        auto& rng = AudioUtils::getThreadLocalRNG();
        currentStep_ = static_cast<int>(rng.nextFloat() * static_cast<float>(stepCount_));
        if (currentStep_ >= stepCount_) currentStep_ = stepCount_ - 1;
    } else {
        if (forward_) {
            // 正向：递增步进
            currentStep_++;
            if (currentStep_ >= stepCount_) {
                currentStep_ = 0; // 循环回第0步
            }
        } else {
            // 反向：递减步进
            currentStep_--;
            if (currentStep_ < 0) {
                currentStep_ = stepCount_ - 1; // 循环回最后一步
            }
        }
    }

    // 摆动时间偏移：在偶数步（0, 2, 4...）时延长步进时间
    // swing_ = 0 无摆动，swing_ = 1 最大摆动
    if (swing_ > 0.0f && !randomMode_) {
        double baseStep = (60.0 / bpm_) * sampleRate_ / 4.0;
        if (currentStep_ % 2 == 0) {
            // 偶数步：延长
            samplesPerStep_ = baseStep * (1.0 + swing_ * 0.5);
        } else {
            // 奇数步：缩短
            samplesPerStep_ = baseStep * (1.0 - swing_ * 0.5);
        }
        samplesPerStep_ = std::max(samplesPerStep_, 1.0);
    }
}

// =============================================================================
// 序列管理
// =============================================================================
void StepSequencer::setStepCount(int count) {
    stepCount_ = AudioUtils::clamp(static_cast<float>(count), 1.0f, static_cast<float>(kMaxSteps));
    // 重置当前步进（如果超出范围）
    if (currentStep_ >= stepCount_) {
        currentStep_ = 0;
    }
}

void StepSequencer::setStepValue(int step, float value) {
    if (step < 0 || step >= kMaxSteps) return;
    stepValues_[step] = AudioUtils::clamp(value, 0.0f, 1.0f);
}

void StepSequencer::setStepGate(int step, bool gate) {
    if (step < 0 || step >= kMaxSteps) return;
    stepGates_[step] = gate;
}

void StepSequencer::setStepTie(int step, bool tie) {
    if (step < 0 || step >= kMaxSteps) return;
    stepTies_[step] = tie;
}

float StepSequencer::getStepValue(int step) const {
    if (step < 0 || step >= kMaxSteps) return 0.0f;
    return stepValues_[step];
}

// =============================================================================
// 播放控制
// =============================================================================
void StepSequencer::setRate(float rate) {
    rate_ = AudioUtils::clamp(rate, 0.25f, 4.0f);
    // 重新计算每步采样数
    if (bpm_ > 0.0 && sampleRate_ > 0.0) {
        samplesPerStep_ = (60.0 / bpm_) * sampleRate_ / 4.0 / static_cast<double>(rate_);
        samplesPerStep_ = std::max(samplesPerStep_, 1.0);
    }
}

void StepSequencer::setSwing(float amount) {
    swing_ = AudioUtils::clamp(amount, 0.0f, 1.0f);
}

void StepSequencer::setDirection(bool forward) {
    forward_ = forward;
}

void StepSequencer::setRandomMode(bool enabled) {
    randomMode_ = enabled;
}

void StepSequencer::setBPM(double bpm) {
    bpm_ = std::max(bpm, 1.0);
    // 重新计算每步采样数
    if (sampleRate_ > 0.0) {
        samplesPerStep_ = (60.0 / bpm_) * sampleRate_ / 4.0 / static_cast<double>(rate_);
        samplesPerStep_ = std::max(samplesPerStep_, 1.0);
    }
}

// =============================================================================
// 输出控制
// =============================================================================
void StepSequencer::setOutputRange(float min, float max) {
    outputMin_ = min;
    outputMax_ = max;
    // 确保min <= max
    if (outputMin_ > outputMax_) {
        std::swap(outputMin_, outputMax_);
    }
}

void StepSequencer::setVolume(float volume) {
    volume_ = AudioUtils::clamp(volume, 0.0f, 1.0f);
}

// =============================================================================
// 重置
// =============================================================================
void StepSequencer::reset() {
    currentStep_ = 0;
    sampleCounter_ = 0.0;
    currentOutputValue_ = 0.0f;
    targetOutputValue_ = 0.0f;
}

// =============================================================================
// 参数接口 (7个参数)
// =============================================================================
float StepSequencer::getParameter(int index) const {
    switch (index) {
        case 0: return (static_cast<float>(stepCount_) - 1.0f) / (static_cast<float>(kMaxSteps) - 1.0f); // 步数
        case 1: return (rate_ - 0.25f) / (4.0f - 0.25f);                                                // 速率
        case 2: return swing_;                                                                           // 摆动
        case 3: return forward_ ? 1.0f : 0.0f;                                                           // 方向
        case 4: return randomMode_ ? 1.0f : 0.0f;                                                        // 随机模式
        case 5: return (outputMin_ + 1.0f) / 2.0f;                                                       // 输出最小值 (-1~1 → 0~1)
        case 6: return (outputMax_ + 1.0f) / 2.0f;                                                       // 输出最大值 (-1~1 → 0~1)
        default: return 0.0f;
    }
}

void StepSequencer::setParameter(int index, float value) {
    switch (index) {
        case 0: setStepCount(static_cast<int>(1.0f + value * (static_cast<float>(kMaxSteps) - 1.0f))); break;
        case 1: setRate(0.25f + value * (4.0f - 0.25f)); break;
        case 2: setSwing(value); break;
        case 3: setDirection(value > 0.5f); break;
        case 4: setRandomMode(value > 0.5f); break;
        case 5: setOutputRange(value * 2.0f - 1.0f, outputMax_); break;
        case 6: setOutputRange(outputMin_, value * 2.0f - 1.0f); break;
        default: break;
    }
}

juce::String StepSequencer::getParameterName(int index) const {
    switch (index) {
        case 0: return "步数";
        case 1: return "速率";
        case 2: return "摆动";
        case 3: return "方向";
        case 4: return "随机模式";
        case 5: return "输出最小值";
        case 6: return "输出最大值";
        default: return "未知";
    }
}

// =============================================================================
// JSON 序列化
// =============================================================================
juce::var StepSequencer::toJson() const {
    auto json = AudioNode::toJson();

    // 序列参数
    json.getDynamicObject()->setProperty("stepCount", stepCount_);
    json.getDynamicObject()->setProperty("rate", rate_);
    json.getDynamicObject()->setProperty("swing", swing_);
    json.getDynamicObject()->setProperty("forward", forward_);
    json.getDynamicObject()->setProperty("randomMode", randomMode_);
    json.getDynamicObject()->setProperty("bpm", bpm_);
    json.getDynamicObject()->setProperty("outputMin", outputMin_);
    json.getDynamicObject()->setProperty("outputMax", outputMax_);
    json.getDynamicObject()->setProperty("volume", volume_);

    // 步进数据：值、门控、连音
    juce::Array<juce::var> stepArray;
    for (int i = 0; i < kMaxSteps; ++i) {
        juce::DynamicObject::Ptr stepObj = new juce::DynamicObject();
        stepObj->setProperty("value", stepValues_[i]);
        stepObj->setProperty("gate", stepGates_[i]);
        stepObj->setProperty("tie", stepTies_[i]);
        stepArray.add(juce::var(stepObj.get()));
    }
    json.getDynamicObject()->setProperty("steps", stepArray);

    return json;
}

void StepSequencer::fromJson(const juce::var& json) {
    AudioNode::fromJson(json);

    if (auto* obj = json.getDynamicObject()) {
        // 序列参数
        stepCount_ = static_cast<int>(obj->getProperty("stepCount").operator double());
        rate_ = static_cast<float>(obj->getProperty("rate").operator double());
        swing_ = static_cast<float>(obj->getProperty("swing").operator double());
        forward_ = static_cast<bool>(obj->getProperty("forward").operator double());
        randomMode_ = static_cast<bool>(obj->getProperty("randomMode").operator double());
        bpm_ = obj->getProperty("bpm").operator double();
        outputMin_ = static_cast<float>(obj->getProperty("outputMin").operator double());
        outputMax_ = static_cast<float>(obj->getProperty("outputMax").operator double());
        volume_ = static_cast<float>(obj->getProperty("volume").operator double());

        // 步进数据
        if (auto* stepProp = obj->getProperty("steps").getArray()) {
            int numSteps = std::min(static_cast<int>(stepProp->size()), kMaxSteps);
            for (int i = 0; i < numSteps; ++i) {
                if (auto* stepObj = (*stepProp)[i].getDynamicObject()) {
                    stepValues_[i] = static_cast<float>(stepObj->getProperty("value").operator double());
                    stepGates_[i] = static_cast<bool>(stepObj->getProperty("gate").operator double());
                    stepTies_[i] = static_cast<bool>(stepObj->getProperty("tie").operator double());
                }
            }
            // 填充剩余步进为默认值
            for (int i = numSteps; i < kMaxSteps; ++i) {
                stepValues_[i] = 0.0f;
                stepGates_[i] = false;
                stepTies_[i] = false;
            }
        }

        // 裁剪步数
        stepCount_ = AudioUtils::clamp(static_cast<float>(stepCount_), 1.0f, static_cast<float>(kMaxSteps));
    }

    // 重新计算每步采样数
    if (sampleRate_ > 0.0 && bpm_ > 0.0) {
        samplesPerStep_ = (60.0 / bpm_) * sampleRate_ / 4.0 / static_cast<double>(rate_);
        samplesPerStep_ = std::max(samplesPerStep_, 1.0);
    }
}

} // namespace LianCore