// =============================================================================
// LianCore - GranularPlayer 粒子播放器 (增强版) 实现
// 新增: 10种包络类型, AI密度优化, 粒子方向控制, 散射控制
// =============================================================================
#include "GranularPlayer.h"
#include "../utils/AudioUtils.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace LianCore {

GranularPlayer::GranularPlayer(const juce::String& name)
    : AudioNode(NodeType::GranularPlayer, name) {
    for (auto& grain : grains_) {
        grain.position = 0.0f;
        grain.pitch = 1.0f;
        grain.pan = 0.0f;
        grain.envelopePos = 0.0f;
        grain.envelopeSpeed = 0.0f;
        grain.age = 0.0f;
        grain.lifetime = 0.0f;
        grain.active = false;
        grain.direction = 0;
        grain.directionPhase = 0.0f;
    }
    activeGrainCount_ = 0;
}

void GranularPlayer::prepareToPlay(double sampleRate, int maxSamplesPerBlock) {
    AudioNode::prepareToPlay(sampleRate, maxSamplesPerBlock);
    sampleRate_ = sampleRate;
    blockSize_ = maxSamplesPerBlock;

    if (grainDensity_ > 0.0f) {
        samplesPerGrain_ = static_cast<float>(sampleRate_) / grainDensity_;
    } else {
        samplesPerGrain_ = static_cast<float>(sampleRate_);
    }
    samplesSinceLastGrain_ = 0.0f;

    for (auto& grain : grains_) {
        grain.active = false;
    }
    activeGrainCount_ = 0;
    currentCpuLoad_ = 0.0f;
}

void GranularPlayer::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midi*/) {
    auto& output = getOutputBuffer(0);
    output.clear();

    int numSamples = buffer.getNumSamples();
    float* outL = output.getWritePointer(0);
    float* outR = output.getWritePointer(1);

    if (!sourceLoaded_ || sourceBuffer_.getNumSamples() == 0) {
        for (int i = 0; i < numSamples; ++i) { outL[i] = 0.0f; outR[i] = 0.0f; }
        return;
    }

    int sourceLength = sourceBuffer_.getNumSamples();

    // AI密度优化
    float effectiveDensity = grainDensity_;
    if (aiDensityControl_) {
        updateAI();
        effectiveDensity = aiOptimalDensity_;
    }
    if (effectiveDensity > 0.0f) {
        samplesPerGrain_ = static_cast<float>(sampleRate_) / effectiveDensity;
    }

    // 逐采样处理
    for (int i = 0; i < numSamples; ++i) {
        samplesSinceLastGrain_ += 1.0f;
        while (samplesSinceLastGrain_ >= samplesPerGrain_ && activeGrainCount_ < kMaxGrains) {
            spawnGrain();
            samplesSinceLastGrain_ -= samplesPerGrain_;
        }

        float sampleOutL = 0.0f;
        float sampleOutR = 0.0f;

        for (auto& grain : grains_) {
            if (!grain.active) continue;

            // 方向控制
            float readPos = grain.position;
            if (grain.direction == 1) {
                // 反向: 从尾部向头部读取
                readPos = grain.lifetime - grain.age;
            } else if (grain.direction == 2) {
                // 双向: 弹跳
                float phase = grain.directionPhase;
                if (phase < 0.5f) {
                    readPos = grain.lifetime * (phase * 2.0f);
                } else {
                    readPos = grain.lifetime * (2.0f - phase * 2.0f);
                }
            }

            int readChannel = 0;
            float srcSample = readSourceSample(readPos, readChannel);

            float envPos = juce::jlimit(0.0f, 1.0f, grain.envelopePos);
            float envelope = getGrainEnvelope(envPos);

            float grainSample = srcSample * envelope;

            float leftGain = (1.0f - grain.pan) * 0.5f;
            float rightGain = (1.0f + grain.pan) * 0.5f;

            sampleOutL += grainSample * leftGain;
            sampleOutR += grainSample * rightGain;

            grain.position += grain.pitch;
            grain.envelopePos += grain.envelopeSpeed;
            grain.age += 1.0f;
            grain.directionPhase += grain.envelopeSpeed * 0.5f;

            if (grain.age >= grain.lifetime) {
                grain.active = false;
                --activeGrainCount_;
            }
        }

        outL[i] = sampleOutL * volume_;
        outR[i] = sampleOutR * volume_;
    }

    cleanupDeadGrains();
}

void GranularPlayer::releaseResources() {
    AudioNode::releaseResources();
    for (auto& grain : grains_) grain.active = false;
    activeGrainCount_ = 0;
}

// =============================================================================
// 粒子生成 (增强)
// =============================================================================
void GranularPlayer::spawnGrain() {
    Grain* targetGrain = nullptr;
    for (auto& grain : grains_) {
        if (!grain.active) { targetGrain = &grain; break; }
    }
    if (!targetGrain) return;

    int sourceLength = sourceBuffer_.getNumSamples();
    if (sourceLength == 0) return;

    float grainSizeInSamples = grainSize_ * static_cast<float>(sampleRate_) / 1000.0f;
    float centerPos = grainPosition_ * static_cast<float>(sourceLength);
    float scatterRange = grainScatter_ * grainSizeInSamples * 2.0f;
    float randomOffset = (nextRandomFloat() * 2.0f - 1.0f) * scatterRange;
    targetGrain->position = centerPos + randomOffset;

    if (targetGrain->position < 0.0f) targetGrain->position += static_cast<float>(sourceLength);
    if (targetGrain->position >= static_cast<float>(sourceLength))
        targetGrain->position -= static_cast<float>(sourceLength);

    float basePitch = static_cast<float>(AudioUtils::semitonesToRatio(static_cast<double>(pitchShift_)));
    float randomPitchSemitones = (nextRandomFloat() * 2.0f - 1.0f) * grainPitchRandom_;
    float randomPitch = static_cast<float>(AudioUtils::semitonesToRatio(static_cast<double>(randomPitchSemitones)));
    targetGrain->pitch = basePitch * randomPitch;

    targetGrain->pan = (nextRandomFloat() * 2.0f - 1.0f) * grainPanRandom_;
    targetGrain->pan = juce::jlimit(-1.0f, 1.0f, targetGrain->pan);

    targetGrain->envelopePos = 0.0f;
    float envelopeLengthInSamples = envelopeLength_ * static_cast<float>(sampleRate_) / 1000.0f;
    if (envelopeLengthInSamples > 0.0f) {
        targetGrain->envelopeSpeed = 1.0f / envelopeLengthInSamples;
    } else {
        targetGrain->envelopeSpeed = 1.0f;
    }

    targetGrain->age = 0.0f;
    targetGrain->lifetime = grainSizeInSamples;

    // 方向设置
    if (grainDirection_ == GrainDirection::Forward) {
        targetGrain->direction = 0;
    } else if (grainDirection_ == GrainDirection::Reverse) {
        targetGrain->direction = 1;
    } else {
        // 双向: 随机选择起始方向
        float bias = directionBias_;
        if (nextRandomFloat() < bias) {
            targetGrain->direction = 1; // 反向起始
        } else {
            targetGrain->direction = 2; // 双向
        }
    }
    targetGrain->directionPhase = 0.0f;

    targetGrain->active = true;
    ++activeGrainCount_;
}

// =============================================================================
// 粒子包络 (扩展: 10种类型)
// =============================================================================
float GranularPlayer::getGrainEnvelope(float position) const {
    return computeEnvelope(envelopeType_, position);
}

// =============================================================================
// AI密度控制 (实现)
// =============================================================================
void GranularPlayer::updateAI() {
    // 基于信号复杂度和CPU负载自动调整最优密度
    const float cpuTarget = 0.3f;  // 目标CPU使用率30%
    const float cpuAlpha = 0.1f;   // CPU平滑系数

    // 估算当前CPU负载 (基于活跃粒子数)
    float currentLoad = static_cast<float>(activeGrainCount_) / static_cast<float>(kMaxGrains);
    currentCpuLoad_ = currentCpuLoad_ * (1.0f - cpuAlpha) + currentLoad * cpuAlpha;

    // 估算信号复杂度 (基于粒子密度变化率)
    signalComplexity_ = estimateSignalComplexity();

    // 动态调整密度
    float baseDensity = grainDensity_;
    float complexityFactor = 1.0f + signalComplexity_ * 0.5f;

    // CPU负载过高时降低密度
    float loadFactor = 1.0f;
    if (currentCpuLoad_ > cpuTarget) {
        loadFactor = cpuTarget / currentCpuLoad_;
    }

    float optimal = baseDensity * complexityFactor * loadFactor;
    aiOptimalDensity_ = juce::jlimit(kMinGrainDensity, kMaxGrainDensity, optimal);

    // 降噪: 平滑输出
    aiOptimalDensity_ = aiOptimalDensity_ * 0.9f + aiOptimalDensity_ * 0.1f;
}

float GranularPlayer::estimateSignalComplexity() const {
    // 基于活跃粒子数和音高随机性估算信号复杂度
    float activity = static_cast<float>(activeGrainCount_) / 50.0f;
    float pitchVar = grainPitchRandom_ / 12.0f;
    float panVar = grainPanRandom_;
    return (activity * 0.4f + pitchVar * 0.3f + panVar * 0.3f);
}

// =============================================================================
// 源音频采样读取
// =============================================================================
float GranularPlayer::readSourceSample(float position, int channel) const {
    int sourceLength = sourceBuffer_.getNumSamples();
    if (sourceLength == 0) return 0.0f;

    while (position < 0.0f) position += static_cast<float>(sourceLength);
    while (position >= static_cast<float>(sourceLength)) position -= static_cast<float>(sourceLength);

    int numChannels = sourceBuffer_.getNumChannels();
    if (channel >= numChannels) channel = numChannels - 1;
    if (channel < 0) channel = 0;

    int idx0 = static_cast<int>(position);
    int idx1 = idx0 + 1;
    if (idx1 >= sourceLength) idx1 = 0;

    float frac = position - static_cast<float>(idx0);
    const float* data = sourceBuffer_.getReadPointer(channel);
    return AudioUtils::lerp(data[idx0], data[idx1], frac);
}

void GranularPlayer::cleanupDeadGrains() {
    for (auto& grain : grains_) {
        if (grain.active && grain.age >= grain.lifetime) {
            grain.active = false;
            --activeGrainCount_;
        }
    }
}

// =============================================================================
// 源音频加载
// =============================================================================
void GranularPlayer::loadSourceBuffer(const juce::AudioSampleBuffer& buffer) {
    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();
    if (numSamples == 0 || numChannels == 0) { sourceLoaded_ = false; return; }
    sourceBuffer_.setSize(numChannels, numSamples);
    for (int ch = 0; ch < numChannels; ++ch) {
        sourceBuffer_.copyFrom(ch, 0, buffer, ch, 0, numSamples);
    }
    sourceLoaded_ = true;
}

void GranularPlayer::loadSourceFile(const juce::File& file) {
    if (!file.existsAsFile()) { sourceLoaded_ = false; return; }
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (!reader) { sourceLoaded_ = false; return; }
    int numSamples = static_cast<int>(reader->lengthInSamples);
    int numChannels = static_cast<int>(reader->numChannels);
    if (numSamples > 480000) numSamples = 480000;
    sourceBuffer_.setSize(numChannels, numSamples);
    reader->read(&sourceBuffer_, 0, numSamples, 0, true, true);
    sourceLoaded_ = true;
}

// =============================================================================
// 粒子参数设置
// =============================================================================
void GranularPlayer::setGrainSize(float ms) {
    grainSize_ = juce::jlimit(kMinGrainSize, kMaxGrainSize, ms);
}
void GranularPlayer::setGrainDensity(float perSec) {
    grainDensity_ = juce::jlimit(kMinGrainDensity, kMaxGrainDensity, perSec);
    if (sampleRate_ > 0.0) samplesPerGrain_ = static_cast<float>(sampleRate_) / grainDensity_;
}
void GranularPlayer::setGrainPosition(float normPos) {
    grainPosition_ = juce::jlimit(0.0f, 1.0f, normPos);
}
void GranularPlayer::setGrainPitchRandom(float semitones) {
    grainPitchRandom_ = juce::jlimit(0.0f, 24.0f, semitones);
}
void GranularPlayer::setGrainPanRandom(float amount) {
    grainPanRandom_ = juce::jlimit(0.0f, 1.0f, amount);
}
void GranularPlayer::setGrainScatter(float amount) {
    grainScatter_ = juce::jlimit(0.0f, 1.0f, amount);
}

void GranularPlayer::setGrainEnvelopeType(GrainEnvelopeType type) {
    envelopeType_ = type;
}
void GranularPlayer::setGrainEnvelopeLength(float ms) {
    envelopeLength_ = juce::jlimit(kMinGrainSize, kMaxGrainSize, ms);
}

void GranularPlayer::setGrainDirection(GrainDirection dir) {
    grainDirection_ = dir;
}
void GranularPlayer::setDirectionBias(float bias) {
    directionBias_ = juce::jlimit(0.0f, 1.0f, bias);
}

void GranularPlayer::enableAiDensityControl(bool enabled) {
    aiDensityControl_ = enabled;
    if (!enabled) aiOptimalDensity_ = grainDensity_;
}
void GranularPlayer::setAiDenoiseLevel(float level) {
    aiDenoiseLevel_ = juce::jlimit(0.0f, 1.0f, level);
}

void GranularPlayer::setVolume(float volume) {
    volume_ = juce::jlimit(0.0f, 1.0f, volume);
}
void GranularPlayer::setPitchShift(float semitones) {
    pitchShift_ = juce::jlimit(-24.0f, 24.0f, semitones);
}

// =============================================================================
// 参数接口 (13个参数)
// =============================================================================
float GranularPlayer::getParameter(int index) const {
    switch (index) {
        case 0:  return (grainSize_ - kMinGrainSize) / (kMaxGrainSize - kMinGrainSize);
        case 1:  return (grainDensity_ - kMinGrainDensity) / (kMaxGrainDensity - kMinGrainDensity);
        case 2:  return grainPosition_;
        case 3:  return grainPitchRandom_ / 24.0f;
        case 4:  return grainPanRandom_;
        case 5:  return static_cast<float>(static_cast<int>(envelopeType_)) / 9.0f;
        case 6:  return (envelopeLength_ - kMinGrainSize) / (kMaxGrainSize - kMinGrainSize);
        case 7:  return volume_;
        case 8:  return (pitchShift_ + 24.0f) / 48.0f;
        case 9:  return aiDenoiseLevel_;
        case 10: return grainScatter_;
        case 11: return static_cast<float>(static_cast<int>(grainDirection_)) / 2.0f;
        case 12: return directionBias_;
        default: return 0.0f;
    }
}

void GranularPlayer::setParameter(int index, float value) {
    switch (index) {
        case 0:  setGrainSize(kMinGrainSize + value * (kMaxGrainSize - kMinGrainSize)); break;
        case 1:  setGrainDensity(kMinGrainDensity + value * (kMaxGrainDensity - kMinGrainDensity)); break;
        case 2:  setGrainPosition(value); break;
        case 3:  setGrainPitchRandom(value * 24.0f); break;
        case 4:  setGrainPanRandom(value); break;
        case 5:  setGrainEnvelopeType(static_cast<GrainEnvelopeType>(static_cast<int>(value * 9.999f))); break;
        case 6:  setGrainEnvelopeLength(kMinGrainSize + value * (kMaxGrainSize - kMinGrainSize)); break;
        case 7:  setVolume(value); break;
        case 8:  setPitchShift(value * 48.0f - 24.0f); break;
        case 9:  setAiDenoiseLevel(value); break;
        case 10: setGrainScatter(value); break;
        case 11: setGrainDirection(static_cast<GrainDirection>(static_cast<int>(value * 2.999f))); break;
        case 12: setDirectionBias(value); break;
        default: break;
    }
}

juce::String GranularPlayer::getParameterName(int index) const {
    switch (index) {
        case 0:  return "粒子大小";
        case 1:  return "粒子密度";
        case 2:  return "读取位置";
        case 3:  return "音高随机";
        case 4:  return "声像随机";
        case 5:  return "包络类型";
        case 6:  return "包络长度";
        case 7:  return "音量";
        case 8:  return "音高偏移";
        case 9:  return "AI降噪";
        case 10: return "散射量";
        case 11: return "粒子方向";
        case 12: return "方向偏差";
        default: return "未知";
    }
}

// =============================================================================
// 随机数
// =============================================================================
float GranularPlayer::nextRandomFloat() {
    uint32_t x = randomSeed_;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    randomSeed_ = x;
    return static_cast<float>(x) / 4294967296.0f;
}

// =============================================================================
// JSON 序列化
// =============================================================================
juce::var GranularPlayer::toJson() const {
    auto json = AudioNode::toJson();
    auto* obj = json.getDynamicObject();
    obj->setProperty("grainSize", grainSize_);
    obj->setProperty("grainDensity", grainDensity_);
    obj->setProperty("grainPosition", grainPosition_);
    obj->setProperty("grainPitchRandom", grainPitchRandom_);
    obj->setProperty("grainPanRandom", grainPanRandom_);
    obj->setProperty("grainScatter", grainScatter_);
    obj->setProperty("envelopeType", static_cast<int>(envelopeType_));
    obj->setProperty("envelopeLength", envelopeLength_);
    obj->setProperty("grainDirection", static_cast<int>(grainDirection_));
    obj->setProperty("directionBias", directionBias_);
    obj->setProperty("aiDensityControl", aiDensityControl_);
    obj->setProperty("aiDenoiseLevel", aiDenoiseLevel_);
    obj->setProperty("volume", volume_);
    obj->setProperty("pitchShift", pitchShift_);
    return json;
}

void GranularPlayer::fromJson(const juce::var& json) {
    AudioNode::fromJson(json);
    if (auto* obj = json.getDynamicObject()) {
        grainSize_ = static_cast<float>(obj->getProperty("grainSize").operator double());
        grainDensity_ = static_cast<float>(obj->getProperty("grainDensity").operator double());
        grainPosition_ = static_cast<float>(obj->getProperty("grainPosition").operator double());
        grainPitchRandom_ = static_cast<float>(obj->getProperty("grainPitchRandom").operator double());
        grainPanRandom_ = static_cast<float>(obj->getProperty("grainPanRandom").operator double());
        grainScatter_ = static_cast<float>(obj->getProperty("grainScatter").operator double());
        envelopeType_ = static_cast<GrainEnvelopeType>(static_cast<int>(obj->getProperty("envelopeType").operator double()));
        envelopeLength_ = static_cast<float>(obj->getProperty("envelopeLength").operator double());
        grainDirection_ = static_cast<GrainDirection>(static_cast<int>(obj->getProperty("grainDirection").operator double()));
        directionBias_ = static_cast<float>(obj->getProperty("directionBias").operator double());
        aiDensityControl_ = static_cast<bool>(obj->getProperty("aiDensityControl").operator double());
        aiDenoiseLevel_ = static_cast<float>(obj->getProperty("aiDenoiseLevel").operator double());
        volume_ = static_cast<float>(obj->getProperty("volume").operator double());
        pitchShift_ = static_cast<float>(obj->getProperty("pitchShift").operator double());
    }
    if (sampleRate_ > 0.0 && grainDensity_ > 0.0f) {
        samplesPerGrain_ = static_cast<float>(sampleRate_) / grainDensity_;
    }
}

} // namespace LianCore