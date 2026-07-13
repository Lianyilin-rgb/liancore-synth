// =============================================================================
// LianCore - WaveguideResonator 波导谐振器实现
// 改进的Karplus-Strong物理模型弦乐合成
// =============================================================================
#include "WaveguideResonator.h"
#include "../utils/AudioUtils.h"

namespace LianCore {

// =============================================================================
// 构造/析构
// =============================================================================
WaveguideResonator::WaveguideResonator(const juce::String& name)
    : AudioNode(NodeType::WaveguideResonator, name) {
    delayLine_.resize(kMaxDelaySamples, 0.0f);
}

// =============================================================================
// 音频处理
// =============================================================================
void WaveguideResonator::prepareToPlay(double sampleRate, int maxSamplesPerBlock) {
    AudioNode::prepareToPlay(sampleRate, maxSamplesPerBlock);
    sampleRate_ = sampleRate;
    blockSize_ = maxSamplesPerBlock;

    // 根据当前频率计算延迟线长度
    float delaySamples = static_cast<float>(sampleRate_) / frequency_ * stringTension_;
    delayLineSize_ = juce::jlimit(4, kMaxDelaySamples, static_cast<int>(delaySamples));

    // 重置所有状态
    writePos_ = 0;
    dampingState_ = 0.0f;
    allpassState_ = 0.0f;
    std::fill(delayLine_.begin(), delayLine_.end(), 0.0f);
}

void WaveguideResonator::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    auto& output = getOutputBuffer(0);
    output.clear();

    int numSamples = buffer.getNumSamples();
    float* outL = output.getWritePointer(0);
    float* outR = output.getWritePointer(1);

    // 处理MIDI消息
    for (const auto& msg : midi) {
        auto message = msg.getMessage();
        if (message.isNoteOn()) {
            float freq = static_cast<float>(
                AudioUtils::midiNoteToFrequency(message.getNoteNumber()));
            setFrequency(freq);
            triggerNote(message.getFloatVelocity());
        } else if (message.isNoteOff()) {
            noteTriggered_ = false;
        }
    }

    // 如果触发了新音符，用激励信号填充延迟线
    if (noteTriggered_) {
        juce::HeapBlock<float> excitationBuf(delayLineSize_);
        generateExcitation(excitationBuf.getData(), delayLineSize_);

        // 将激励信号写入延迟线
        for (int i = 0; i < delayLineSize_; ++i) {
            delayLine_[i] = excitationBuf[i];
        }
        writePos_ = 0;
        noteTriggered_ = false;
    }

    // 衰减因子: 0.0 = 快速衰减, 1.0 = 几乎无限延音
    float decayFactor = juce::jmap(decay_, 0.0f, 1.0f, 0.5f, 0.9995f);

    for (int i = 0; i < numSamples; ++i) {
        // =====================================================================
        // 1. 从延迟线读取 (经典KS: readPos = writePos)
        // =====================================================================
        int readPos = writePos_;
        float currentSample = delayLine_[readPos];

        // =====================================================================
        // 2. 阻尼滤波器 (一阶低通, 模拟弦乐高频衰减)
        //    damping_ = 0: 无滤波(明亮)
        //    damping_ = 1: 最大滤波(暗淡)
        // =====================================================================
        float damped = (1.0f - damping_) * currentSample + damping_ * dampingState_;
        dampingState_ = damped;

        // =====================================================================
        // 3. 衰减
        // =====================================================================
        float decayed = damped * decayFactor;

        // =====================================================================
        // 4. 全通滤波器 (非线性/非谐波效果)
        // =====================================================================
        float processed = processAllpass(decayed);

        // =====================================================================
        // 5. 写回延迟线
        // =====================================================================
        delayLine_[writePos_] = processed;

        // =====================================================================
        // 6. 拾音位置: 在两个读出位置之间插值
        //    pickupPosition_ = 0: 琴桥位置(明亮)
        //    pickupPosition_ = 0.5: 中间位置(平衡)
        //    pickupPosition_ = 1: 远端位置(柔和)
        // =====================================================================
        float readOffset = pickupPosition_ * delayLineSize_;
        int readOffsetInt = static_cast<int>(readOffset);
        float readFrac = readOffset - readOffsetInt;
        int pickupPos1 = (writePos_ + readOffsetInt) % delayLineSize_;
        int pickupPos2 = (pickupPos1 + 1) % delayLineSize_;
        float pickupSample = AudioUtils::lerp(
            delayLine_[pickupPos1], delayLine_[pickupPos2], readFrac);

        // =====================================================================
        // 7. 体共振: 从延迟线半长位置读取(2倍频率谐振)
        //    模拟琴体共鸣
        // =====================================================================
        float bodySample = 0.0f;
        if (bodyResonance_ > 0.0f && delayLineSize_ >= 4) {
            int bodyReadPos = (writePos_ - delayLineSize_ / 2 + delayLineSize_) % delayLineSize_;
            bodySample = delayLine_[bodyReadPos] * bodyResonance_ * 0.3f;
        }

        // =====================================================================
        // 8. 混合输出
        // =====================================================================
        float outSample = (pickupSample + bodySample) * volume_ * velocity_;

        outL[i] = outSample;
        outR[i] = outSample;

        // =====================================================================
        // 9. 推进写入位置 (环形缓冲区)
        // =====================================================================
        writePos_ = (writePos_ + 1) % delayLineSize_;
    }
}

void WaveguideResonator::releaseResources() {
    AudioNode::releaseResources();
    std::fill(delayLine_.begin(), delayLine_.end(), 0.0f);
    dampingState_ = 0.0f;
    allpassState_ = 0.0f;
    writePos_ = 0;
}

// =============================================================================
// Karplus-Strong参数
// =============================================================================
void WaveguideResonator::setFrequency(float hz) {
    frequency_ = AudioUtils::clamp(hz, 1.0f, 20000.0f);

    // 延迟线长度 = 采样率 / 频率, 由张力调整
    float delaySamples = static_cast<float>(sampleRate_) / frequency_ * stringTension_;
    delayLineSize_ = juce::jlimit(4, kMaxDelaySamples, static_cast<int>(delaySamples));

    // 如果写入位置超出新的延迟线长度，重置
    if (writePos_ >= delayLineSize_) {
        writePos_ = 0;
    }
}

void WaveguideResonator::setDecay(float decay) {
    decay_ = AudioUtils::clamp(decay, 0.0f, 1.0f);
}

void WaveguideResonator::setDamping(float damping) {
    damping_ = AudioUtils::clamp(damping, 0.0f, 1.0f);
}

void WaveguideResonator::setBodyResonance(float amount) {
    bodyResonance_ = AudioUtils::clamp(amount, 0.0f, 1.0f);
}

// =============================================================================
// 激励控制
// =============================================================================
void WaveguideResonator::setExcitationType(ExcitationType type) {
    excitationType_ = type;
}

void WaveguideResonator::setExcitationSample(const juce::AudioSampleBuffer& sample) {
    excitationSample_.makeCopyOf(sample);
}

// =============================================================================
// 物理参数
// =============================================================================
void WaveguideResonator::setStringTension(float tension) {
    stringTension_ = AudioUtils::clamp(tension, 0.5f, 2.0f);

    // 张力变化时重新计算延迟线长度
    float delaySamples = static_cast<float>(sampleRate_) / frequency_ * stringTension_;
    delayLineSize_ = juce::jlimit(4, kMaxDelaySamples, static_cast<int>(delaySamples));

    if (writePos_ >= delayLineSize_) {
        writePos_ = 0;
    }
}

void WaveguideResonator::setStringInharmonicity(float amount) {
    stringInharmonicity_ = AudioUtils::clamp(amount, 0.0f, 1.0f);
}

void WaveguideResonator::setPickupPosition(float pos) {
    pickupPosition_ = AudioUtils::clamp(pos, 0.0f, 1.0f);
}

// =============================================================================
// 播放控制
// =============================================================================
void WaveguideResonator::setVolume(float volume) {
    volume_ = AudioUtils::clamp(volume, 0.0f, 1.0f);
}

void WaveguideResonator::triggerNote(float velocity) {
    noteTriggered_ = true;
    velocity_ = AudioUtils::clamp(velocity, 0.0f, 1.0f);
}

// =============================================================================
// 激励信号生成
// =============================================================================
void WaveguideResonator::generateExcitation(float* output, int numSamples) {
    auto& rng = AudioUtils::getThreadLocalRNG();

    switch (excitationType_) {
        // =====================================================================
        // 噪声激励: 用白噪声填充延迟线 (经典KS)
        // =====================================================================
        case ExcitationType::Noise: {
            for (int i = 0; i < numSamples; ++i) {
                output[i] = rng.nextFloat(-1.0f, 1.0f) * velocity_;
            }
            break;
        }

        // =====================================================================
        // 脉冲激励: 在延迟线开头产生一个尖脉冲, 其余为零
        // =====================================================================
        case ExcitationType::Pulse: {
            std::fill_n(output, numSamples, 0.0f);

            // 脉冲宽度: 延迟线长度的5% (最少4个采样)
            int pulseWidth = juce::jmax(4, numSamples / 20);
            for (int i = 0; i < pulseWidth && i < numSamples; ++i) {
                // 指数衰减的脉冲
                float envelope = std::exp(-static_cast<float>(i) / (pulseWidth * 0.3f));
                output[i] = (rng.nextFloat(-0.3f, 0.3f) + 0.7f) * envelope * velocity_;
            }
            break;
        }

        // =====================================================================
        // 采样激励: 将外部采样复制到延迟线
        // =====================================================================
        case ExcitationType::Sample: {
            if (excitationSample_.getNumSamples() > 0) {
                const float* srcData = excitationSample_.getReadPointer(0);
                int srcLen = excitationSample_.getNumSamples();

                for (int i = 0; i < numSamples; ++i) {
                    // 循环读取采样数据
                    output[i] = srcData[i % srcLen] * velocity_;
                }
            } else {
                // 无采样数据时退化为噪声激励
                for (int i = 0; i < numSamples; ++i) {
                    output[i] = rng.nextFloat(-1.0f, 1.0f) * velocity_;
                }
            }
            break;
        }
    }
}

// =============================================================================
// 全通滤波器 (用于非谐波效果)
// 一阶全通: y[n] = a * x[n] + x[n-1] - a * y[n-1]
// 重构为单状态形式: v[n] = x[n] - a * v[n-1]; y[n] = v[n-1] + a * v[n]
// =============================================================================
float WaveguideResonator::processAllpass(float input) {
    float a = stringInharmonicity_;

    // 无非谐波时直通
    if (a <= 0.0f) {
        return input;
    }

    // v[n] = x[n] - a * v[n-1]
    float v = input - a * allpassState_;
    // y[n] = v[n-1] + a * v[n]
    float output = allpassState_ + a * v;
    // 存储 v[n] 用于下一次迭代
    allpassState_ = v;

    return output;
}

// =============================================================================
// 参数接口 (9个参数, 归一化到 [0.0, 1.0])
// =============================================================================
float WaveguideResonator::getParameter(int index) const {
    switch (index) {
        case 0: return (frequency_ - 1.0f) / 19999.0f;        // 频率: 1Hz-20000Hz
        case 1: return decay_;                                  // 衰减: 0.0-1.0
        case 2: return damping_;                                // 阻尼: 0.0-1.0
        case 3: return bodyResonance_;                          // 体共振: 0.0-1.0
        case 4: return static_cast<float>(excitationType_) / 2.0f; // 激励类型: 0=Noise, 1=Pulse, 2=Sample
        case 5: return (stringTension_ - 0.5f) / 1.5f;         // 张力: 0.5-2.0
        case 6: return stringInharmonicity_;                    // 非谐波: 0.0-1.0
        case 7: return pickupPosition_;                         // 拾音位置: 0.0-1.0
        case 8: return volume_;                                 // 音量: 0.0-1.0
        default: return 0.0f;
    }
}

void WaveguideResonator::setParameter(int index, float value) {
    switch (index) {
        case 0: setFrequency(1.0f + value * 19999.0f); break;
        case 1: setDecay(value); break;
        case 2: setDamping(value); break;
        case 3: setBodyResonance(value); break;
        case 4: {
            int typeIdx = juce::jlimit(0, 2, static_cast<int>(value * 3.0f));
            setExcitationType(static_cast<ExcitationType>(typeIdx));
            break;
        }
        case 5: setStringTension(0.5f + value * 1.5f); break;
        case 6: setStringInharmonicity(value); break;
        case 7: setPickupPosition(value); break;
        case 8: setVolume(value); break;
        default: break;
    }
}

juce::String WaveguideResonator::getParameterName(int index) const {
    switch (index) {
        case 0: return "频率";
        case 1: return "衰减";
        case 2: return "阻尼";
        case 3: return "体共振";
        case 4: return "激励类型";
        case 5: return "弦张力";
        case 6: return "非谐波";
        case 7: return "拾音位置";
        case 8: return "音量";
        default: return "未知";
    }
}

// =============================================================================
// 序列化
// =============================================================================
juce::var WaveguideResonator::toJson() const {
    auto json = AudioNode::toJson();

    // Karplus-Strong参数
    json.getDynamicObject()->setProperty("frequency", frequency_);
    json.getDynamicObject()->setProperty("decay", decay_);
    json.getDynamicObject()->setProperty("damping", damping_);
    json.getDynamicObject()->setProperty("bodyResonance", bodyResonance_);

    // 激励参数
    json.getDynamicObject()->setProperty("excitationType", static_cast<int>(excitationType_));

    // 物理参数
    json.getDynamicObject()->setProperty("stringTension", stringTension_);
    json.getDynamicObject()->setProperty("stringInharmonicity", stringInharmonicity_);
    json.getDynamicObject()->setProperty("pickupPosition", pickupPosition_);

    // 播放控制
    json.getDynamicObject()->setProperty("volume", volume_);

    return json;
}

void WaveguideResonator::fromJson(const juce::var& json) {
    AudioNode::fromJson(json);

    // Karplus-Strong参数
    if (json.hasProperty("frequency")) {
        setFrequency(static_cast<float>(json["frequency"]));
    }
    if (json.hasProperty("decay")) {
        setDecay(static_cast<float>(json["decay"]));
    }
    if (json.hasProperty("damping")) {
        setDamping(static_cast<float>(json["damping"]));
    }
    if (json.hasProperty("bodyResonance")) {
        setBodyResonance(static_cast<float>(json["bodyResonance"]));
    }

    // 激励参数
    if (json.hasProperty("excitationType")) {
        int type = static_cast<int>(json["excitationType"]);
        setExcitationType(static_cast<ExcitationType>(type));
    }

    // 物理参数
    if (json.hasProperty("stringTension")) {
        setStringTension(static_cast<float>(json["stringTension"]));
    }
    if (json.hasProperty("stringInharmonicity")) {
        setStringInharmonicity(static_cast<float>(json["stringInharmonicity"]));
    }
    if (json.hasProperty("pickupPosition")) {
        setPickupPosition(static_cast<float>(json["pickupPosition"]));
    }

    // 播放控制
    if (json.hasProperty("volume")) {
        setVolume(static_cast<float>(json["volume"]));
    }
}

} // namespace LianCore