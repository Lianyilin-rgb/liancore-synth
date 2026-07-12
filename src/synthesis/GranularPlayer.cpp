// =============================================================================
// LianCore - GranularPlayer 粒子播放器 实现
// 粒子合成引擎：将音频分割为微小粒子重组
// =============================================================================
#include "GranularPlayer.h"
#include "../utils/AudioUtils.h"

namespace LianCore {

// =============================================================================
// 构造 / 析构
// =============================================================================
GranularPlayer::GranularPlayer(const juce::String& name)
    : AudioNode(NodeType::GranularPlayer, name) {
    // 初始化所有粒子为不活跃状态
    for (auto& grain : grains_) {
        grain.position = 0.0f;
        grain.pitch = 1.0f;
        grain.pan = 0.0f;
        grain.envelopePos = 0.0f;
        grain.envelopeSpeed = 0.0f;
        grain.age = 0.0f;
        grain.lifetime = 0.0f;
        grain.active = false;
    }
    activeGrainCount_ = 0;
}

// =============================================================================
// 音频处理
// =============================================================================
void GranularPlayer::prepareToPlay(double sampleRate, int maxSamplesPerBlock) {
    AudioNode::prepareToPlay(sampleRate, maxSamplesPerBlock);
    sampleRate_ = sampleRate;
    blockSize_ = maxSamplesPerBlock;

    // 根据密度计算每粒间隔（采样数）
    if (grainDensity_ > 0.0f) {
        samplesPerGrain_ = static_cast<float>(sampleRate_) / grainDensity_;
    } else {
        samplesPerGrain_ = static_cast<float>(sampleRate_); // 默认1秒一粒
    }
    samplesSinceLastGrain_ = 0.0f;

    // 重置所有粒子状态
    for (auto& grain : grains_) {
        grain.active = false;
    }
    activeGrainCount_ = 0;
}

void GranularPlayer::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midi*/) {
    auto& output = getOutputBuffer(0);
    output.clear();

    int numSamples = buffer.getNumSamples();
    float* outL = output.getWritePointer(0);
    float* outR = output.getWritePointer(1);

    // 如果没有加载源音频，输出静音
    if (!sourceLoaded_ || sourceBuffer_.getNumSamples() == 0) {
        AudioUtils::clearBufferSIMD(outL, numSamples);
        AudioUtils::clearBufferSIMD(outR, numSamples);
        return;
    }

    int sourceLength = sourceBuffer_.getNumSamples();
    int sourceChannels = sourceBuffer_.getNumChannels();

    // 更新密度参数（可能在运行时被修改）
    if (grainDensity_ > 0.0f) {
        samplesPerGrain_ = static_cast<float>(sampleRate_) / grainDensity_;
    }

    // 逐采样处理
    for (int i = 0; i < numSamples; ++i) {
        // ---- 生成新粒子 ----
        samplesSinceLastGrain_ += 1.0f;
        while (samplesSinceLastGrain_ >= samplesPerGrain_ && activeGrainCount_ < kMaxGrains) {
            spawnGrain();
            samplesSinceLastGrain_ -= samplesPerGrain_;
        }

        float sampleOutL = 0.0f;
        float sampleOutR = 0.0f;

        // ---- 处理每个活跃粒子 ----
        for (auto& grain : grains_) {
            if (!grain.active) continue;

            // 读取源音频采样（使用通道0，或根据声像选择通道）
            int readChannel = 0;
            float srcSample = readSourceSample(grain.position, readChannel);

            // 获取包络值
            float envPos = AudioUtils::clamp(grain.envelopePos, 0.0f, 1.0f);
            float envelope = getGrainEnvelope(envPos);

            // 应用包络
            float grainSample = srcSample * envelope;

            // 计算声像增益（线性声像）
            float leftGain = (1.0f - grain.pan) * 0.5f;
            float rightGain = (1.0f + grain.pan) * 0.5f;

            sampleOutL += grainSample * leftGain;
            sampleOutR += grainSample * rightGain;

            // 更新粒子状态
            grain.position += grain.pitch;
            grain.envelopePos += grain.envelopeSpeed;
            grain.age += 1.0f;

            // 检查粒子是否过期
            if (grain.age >= grain.lifetime) {
                grain.active = false;
                --activeGrainCount_;
            }
        }

        // 写入输出缓冲区
        outL[i] = sampleOutL * volume_;
        outR[i] = sampleOutR * volume_;
    }

    // 清理过期粒子（安全兜底）
    cleanupDeadGrains();
}

void GranularPlayer::releaseResources() {
    AudioNode::releaseResources();
    for (auto& grain : grains_) {
        grain.active = false;
    }
    activeGrainCount_ = 0;
}

// =============================================================================
// 粒子生成
// =============================================================================
void GranularPlayer::spawnGrain() {
    // 找到一个空闲粒子槽位
    Grain* targetGrain = nullptr;
    for (auto& grain : grains_) {
        if (!grain.active) {
            targetGrain = &grain;
            break;
        }
    }
    if (targetGrain == nullptr) return; // 粒子池已满

    int sourceLength = sourceBuffer_.getNumSamples();
    if (sourceLength == 0) return;

    // 计算粒子在源音频中的位置
    float grainSizeInSamples = grainSize_ * static_cast<float>(sampleRate_) / 1000.0f;
    float centerPos = grainPosition_ * static_cast<float>(sourceLength);
    // 在中心位置附近随机散布（散布范围 = 粒子大小的一半）
    float randomOffset = (nextRandomFloat() * 2.0f - 1.0f) * grainSizeInSamples * 0.5f;
    targetGrain->position = centerPos + randomOffset;
    // 确保位置在有效范围内
    if (targetGrain->position < 0.0f) targetGrain->position += static_cast<float>(sourceLength);
    if (targetGrain->position >= static_cast<float>(sourceLength))
        targetGrain->position -= static_cast<float>(sourceLength);

    // 计算音高倍率: 基础音高偏移 + 随机偏移
    float basePitch = static_cast<float>(AudioUtils::semitonesToRatio(static_cast<double>(pitchShift_)));
    float randomPitchSemitones = (nextRandomFloat() * 2.0f - 1.0f) * grainPitchRandom_;
    float randomPitch = static_cast<float>(AudioUtils::semitonesToRatio(static_cast<double>(randomPitchSemitones)));
    targetGrain->pitch = basePitch * randomPitch;

    // 随机声像
    targetGrain->pan = (nextRandomFloat() * 2.0f - 1.0f) * grainPanRandom_;
    targetGrain->pan = AudioUtils::clamp(targetGrain->pan, -1.0f, 1.0f);

    // 设置包络参数
    targetGrain->envelopePos = 0.0f;
    float envelopeLengthInSamples = envelopeLength_ * static_cast<float>(sampleRate_) / 1000.0f;
    if (envelopeLengthInSamples > 0.0f) {
        targetGrain->envelopeSpeed = 1.0f / envelopeLengthInSamples;
    } else {
        targetGrain->envelopeSpeed = 1.0f; // 极短包络，立即完成
    }

    // 设置粒子生存时间
    targetGrain->age = 0.0f;
    targetGrain->lifetime = grainSizeInSamples;

    targetGrain->active = true;
    ++activeGrainCount_;
}

// =============================================================================
// 粒子包络
// =============================================================================
float GranularPlayer::getGrainEnvelope(float position) const {
    // position: 0.0 ~ 1.0，包络归一化位置
    switch (envelopeType_) {
        case GrainEnvelopeType::Hann: {
            // cos²(π * (position - 0.5)) = sin²(π * position)
            float angle = static_cast<float>(AudioUtils::kPI) * (position - 0.5f);
            float cosVal = std::cos(angle);
            return cosVal * cosVal;
        }
        case GrainEnvelopeType::Hamming: {
            // 0.54 - 0.46 * cos(2π * position)
            return 0.54f - 0.46f * std::cos(static_cast<float>(AudioUtils::kTwoPI) * position);
        }
        case GrainEnvelopeType::Triangle: {
            // 1 - |2 * position - 1|
            return 1.0f - std::abs(2.0f * position - 1.0f);
        }
        case GrainEnvelopeType::Exponential: {
            // exp(-position * 5.0)
            return std::exp(-position * 5.0f);
        }
        case GrainEnvelopeType::Rectangular: {
            return 1.0f;
        }
        default:
            return 0.0f;
    }
}

// =============================================================================
// 源音频采样读取（带线性插值，环绕）
// =============================================================================
float GranularPlayer::readSourceSample(float position, int channel) const {
    int sourceLength = sourceBuffer_.getNumSamples();
    if (sourceLength == 0) return 0.0f;

    // 环绕处理：确保位置在 [0, sourceLength) 范围内
    while (position < 0.0f) position += static_cast<float>(sourceLength);
    while (position >= static_cast<float>(sourceLength)) position -= static_cast<float>(sourceLength);

    // 确保通道索引有效
    int numChannels = sourceBuffer_.getNumChannels();
    if (channel >= numChannels) channel = numChannels - 1;
    if (channel < 0) channel = 0;

    // 线性插值
    int idx0 = static_cast<int>(position);
    int idx1 = idx0 + 1;
    if (idx1 >= sourceLength) idx1 = 0; // 环绕

    float frac = position - static_cast<float>(idx0);
    const float* data = sourceBuffer_.getReadPointer(channel);
    float sample0 = data[idx0];
    float sample1 = data[idx1];

    return AudioUtils::lerp(sample0, sample1, frac);
}

// =============================================================================
// 清理过期粒子
// =============================================================================
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

    if (numSamples == 0 || numChannels == 0) {
        sourceLoaded_ = false;
        return;
    }

    sourceBuffer_.setSize(numChannels, numSamples);
    for (int ch = 0; ch < numChannels; ++ch) {
        sourceBuffer_.copyFrom(ch, 0, buffer, ch, 0, numSamples);
    }
    sourceLoaded_ = true;
}

void GranularPlayer::loadSourceFile(const juce::File& file) {
    if (!file.existsAsFile()) {
        sourceLoaded_ = false;
        return;
    }

    // 使用 JUCE AudioFormatManager 读取音频文件
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(file));
    if (reader == nullptr) {
        sourceLoaded_ = false;
        return;
    }

    int numSamples = static_cast<int>(reader->lengthInSamples);
    int numChannels = static_cast<int>(reader->numChannels);

    // 限制加载长度（防止内存溢出）
    if (numSamples > 480000) { // 10秒 @ 48kHz
        numSamples = 480000;
    }

    sourceBuffer_.setSize(numChannels, numSamples);
    reader->read(&sourceBuffer_, 0, numSamples, 0, true, true);
    sourceLoaded_ = true;
}

// =============================================================================
// 粒子参数设置
// =============================================================================
void GranularPlayer::setGrainSize(float ms) {
    grainSize_ = AudioUtils::clamp(ms, kMinGrainSize, kMaxGrainSize);
    // 更新密度参数
    if (grainDensity_ > 0.0f && sampleRate_ > 0.0) {
        samplesPerGrain_ = static_cast<float>(sampleRate_) / grainDensity_;
    }
}

void GranularPlayer::setGrainDensity(float perSec) {
    grainDensity_ = AudioUtils::clamp(perSec, kMinGrainDensity, kMaxGrainDensity);
    if (sampleRate_ > 0.0) {
        samplesPerGrain_ = static_cast<float>(sampleRate_) / grainDensity_;
    }
}

void GranularPlayer::setGrainPosition(float normPos) {
    grainPosition_ = AudioUtils::clamp(normPos, 0.0f, 1.0f);
}

void GranularPlayer::setGrainPitchRandom(float semitones) {
    grainPitchRandom_ = AudioUtils::clamp(semitones, 0.0f, 24.0f);
}

void GranularPlayer::setGrainPanRandom(float amount) {
    grainPanRandom_ = AudioUtils::clamp(amount, 0.0f, 1.0f);
}

// =============================================================================
// 粒子包络设置
// =============================================================================
void GranularPlayer::setGrainEnvelopeType(GrainEnvelopeType type) {
    envelopeType_ = type;
}

void GranularPlayer::setGrainEnvelopeLength(float ms) {
    envelopeLength_ = AudioUtils::clamp(ms, kMinGrainSize, kMaxGrainSize);
}

// =============================================================================
// AI增强
// =============================================================================
void GranularPlayer::enableAiDensityControl(bool enabled) {
    aiDensityControl_ = enabled;
}

void GranularPlayer::setAiDenoiseLevel(float level) {
    aiDenoiseLevel_ = AudioUtils::clamp(level, 0.0f, 1.0f);
}

// =============================================================================
// 播放控制
// =============================================================================
void GranularPlayer::setVolume(float volume) {
    volume_ = AudioUtils::clamp(volume, 0.0f, 1.0f);
}

void GranularPlayer::setPitchShift(float semitones) {
    pitchShift_ = AudioUtils::clamp(semitones, -24.0f, 24.0f);
}

// =============================================================================
// 参数接口 (10个参数)
// =============================================================================
float GranularPlayer::getParameter(int index) const {
    switch (index) {
        case 0: return (grainSize_ - kMinGrainSize) / (kMaxGrainSize - kMinGrainSize);        // 粒子大小
        case 1: return (grainDensity_ - kMinGrainDensity) / (kMaxGrainDensity - kMinGrainDensity); // 粒子密度
        case 2: return grainPosition_;                                                         // 读取位置
        case 3: return grainPitchRandom_ / 24.0f;                                              // 音高随机
        case 4: return grainPanRandom_;                                                        // 声像随机
        case 5: return static_cast<float>(static_cast<int>(envelopeType_)) / 4.0f;             // 包络类型
        case 6: return (envelopeLength_ - kMinGrainSize) / (kMaxGrainSize - kMinGrainSize);    // 包络长度
        case 7: return volume_;                                                                // 音量
        case 8: return (pitchShift_ + 24.0f) / 48.0f;                                         // 音高偏移 (-24~+24 → 0~1)
        case 9: return aiDenoiseLevel_;                                                        // AI降噪
        default: return 0.0f;
    }
}

void GranularPlayer::setParameter(int index, float value) {
    switch (index) {
        case 0: setGrainSize(kMinGrainSize + value * (kMaxGrainSize - kMinGrainSize)); break;
        case 1: setGrainDensity(kMinGrainDensity + value * (kMaxGrainDensity - kMinGrainDensity)); break;
        case 2: setGrainPosition(value); break;
        case 3: setGrainPitchRandom(value * 24.0f); break;
        case 4: setGrainPanRandom(value); break;
        case 5: setGrainEnvelopeType(static_cast<GrainEnvelopeType>(static_cast<int>(value * 4.999f))); break;
        case 6: setGrainEnvelopeLength(kMinGrainSize + value * (kMaxGrainSize - kMinGrainSize)); break;
        case 7: setVolume(value); break;
        case 8: setPitchShift(value * 48.0f - 24.0f); break;
        case 9: setAiDenoiseLevel(value); break;
        default: break;
    }
}

juce::String GranularPlayer::getParameterName(int index) const {
    switch (index) {
        case 0: return "粒子大小";
        case 1: return "粒子密度";
        case 2: return "读取位置";
        case 3: return "音高随机";
        case 4: return "声像随机";
        case 5: return "包络类型";
        case 6: return "包络长度";
        case 7: return "音量";
        case 8: return "音高偏移";
        case 9: return "AI降噪";
        default: return "未知";
    }
}

// =============================================================================
// 随机数生成器 (xorshift)
// =============================================================================
float GranularPlayer::nextRandomFloat() {
    uint32_t x = randomSeed_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    randomSeed_ = x;
    // 归一化到 [0.0, 1.0)
    return static_cast<float>(x) / 4294967296.0f; // UINT32_MAX + 1
}

// =============================================================================
// JSON 序列化
// =============================================================================
juce::var GranularPlayer::toJson() const {
    auto json = AudioNode::toJson();

    // 粒子参数
    json.getDynamicObject()->setProperty("grainSize", grainSize_);
    json.getDynamicObject()->setProperty("grainDensity", grainDensity_);
    json.getDynamicObject()->setProperty("grainPosition", grainPosition_);
    json.getDynamicObject()->setProperty("grainPitchRandom", grainPitchRandom_);
    json.getDynamicObject()->setProperty("grainPanRandom", grainPanRandom_);

    // 包络参数
    json.getDynamicObject()->setProperty("envelopeType", static_cast<int>(envelopeType_));
    json.getDynamicObject()->setProperty("envelopeLength", envelopeLength_);

    // AI参数
    json.getDynamicObject()->setProperty("aiDensityControl", aiDensityControl_);
    json.getDynamicObject()->setProperty("aiDenoiseLevel", aiDenoiseLevel_);

    // 播放参数
    json.getDynamicObject()->setProperty("volume", volume_);
    json.getDynamicObject()->setProperty("pitchShift", pitchShift_);

    return json;
}

void GranularPlayer::fromJson(const juce::var& json) {
    AudioNode::fromJson(json);

    if (auto* obj = json.getDynamicObject()) {
        // 粒子参数
        grainSize_ = static_cast<float>(obj->getProperty("grainSize").operator double());
        grainDensity_ = static_cast<float>(obj->getProperty("grainDensity").operator double());
        grainPosition_ = static_cast<float>(obj->getProperty("grainPosition").operator double());
        grainPitchRandom_ = static_cast<float>(obj->getProperty("grainPitchRandom").operator double());
        grainPanRandom_ = static_cast<float>(obj->getProperty("grainPanRandom").operator double());

        // 包络参数
        envelopeType_ = static_cast<GrainEnvelopeType>(
            static_cast<int>(obj->getProperty("envelopeType").operator double()));
        envelopeLength_ = static_cast<float>(obj->getProperty("envelopeLength").operator double());

        // AI参数
        aiDensityControl_ = static_cast<bool>(obj->getProperty("aiDensityControl").operator double());
        aiDenoiseLevel_ = static_cast<float>(obj->getProperty("aiDenoiseLevel").operator double());

        // 播放参数
        volume_ = static_cast<float>(obj->getProperty("volume").operator double());
        pitchShift_ = static_cast<float>(obj->getProperty("pitchShift").operator double());
    }

    // 重新计算密度相关参数
    if (sampleRate_ > 0.0 && grainDensity_ > 0.0f) {
        samplesPerGrain_ = static_cast<float>(sampleRate_) / grainDensity_;
    }
}

} // namespace LianCore