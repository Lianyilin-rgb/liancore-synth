// =============================================================================
// LianCore - MultiSampler 多采样播放器 实现
// 多采样键位/力度映射、AI自动映射、循环播放
// =============================================================================
#include "MultiSampler.h"
#include "../utils/AudioUtils.h"

namespace LianCore {

// =============================================================================
// 构造 / 析构
// =============================================================================
MultiSampler::MultiSampler(const juce::String& name)
    : AudioNode(NodeType::MultiSampler, name) {
    // 初始化所有声音为不活跃状态
    for (auto& voice : voices_) {
        voice.zoneIndex = -1;
        voice.position = 0.0f;
        voice.pitchRatio = 1.0f;
        voice.velocity = 1.0f;
        voice.pan = 0.0f;
        voice.volume = 1.0f;
        voice.active = false;
        voice.midiNote = -1;
    }
}

// =============================================================================
// 音频处理
// =============================================================================
void MultiSampler::prepareToPlay(double sampleRate, int maxSamplesPerBlock) {
    AudioNode::prepareToPlay(sampleRate, maxSamplesPerBlock);
    // 存储采样率与块大小，供后续音频处理使用
    sampleRate_ = sampleRate;
    blockSize_ = maxSamplesPerBlock;

    // 重置所有声音状态，确保在重新准备时不会有残留活跃声音
    for (auto& voice : voices_) {
        voice.active = false;
        voice.midiNote = -1;
    }
}

void MultiSampler::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    auto& output = getOutputBuffer(0);
    output.clear();

    int numSamples = buffer.getNumSamples();
    float* outL = output.getWritePointer(0);
    float* outR = output.getWritePointer(1);

    // ---- 处理MIDI事件 ----
    for (const auto& event : midi) {
        auto message = event.getMessage();

        if (message.isNoteOn()) {
            // 查找匹配的采样区域
            int noteNumber = message.getNoteNumber();
            int velocity = static_cast<int>(message.getVelocity());
            const SampleZone* zone = findZone(noteNumber, velocity);

            if (zone != nullptr) {
                // 找到空闲声音槽位
                Voice& voice = findFreeVoice();
                // 在样本区域列表中查找该zone的索引
                int zoneIdx = static_cast<int>(zone - sampleZones_.data());
                voice.zoneIndex = zoneIdx;
                voice.position = 0.0f; // 从头开始播放

                // 计算音高倍率：根据根音偏移量调整播放速度
                float pitchOffset = zone->tune + static_cast<float>(noteNumber - zone->rootNote);
                voice.pitchRatio = static_cast<float>(AudioUtils::semitonesToRatio(static_cast<double>(pitchOffset)));

                // 力度归一化到 [0, 1]
                voice.velocity = static_cast<float>(velocity) / 127.0f;
                voice.pan = zone->pan;
                voice.volume = zone->volume;
                voice.active = true;
                voice.midiNote = noteNumber;
            }
        }
        else if (message.isNoteOff()) {
            // 查找对应音符的活跃声音并将其关闭
            int noteNumber = message.getNoteNumber();
            for (auto& voice : voices_) {
                if (voice.active && voice.midiNote == noteNumber) {
                    voice.active = false;
                    voice.midiNote = -1;
                }
            }
        }
    }

    // ---- 处理活跃声音，逐采样混合输出 ----
    for (int i = 0; i < numSamples; ++i) {
        float sampleSumL = 0.0f;
        float sampleSumR = 0.0f;

        for (auto& voice : voices_) {
            if (!voice.active) continue;

            // 获取对应的采样区域
            int zoneIdx = voice.zoneIndex;
            if (zoneIdx < 0 || zoneIdx >= static_cast<int>(sampleZones_.size())) {
                voice.active = false;
                continue;
            }
            const SampleZone& zone = sampleZones_[static_cast<size_t>(zoneIdx)];

            // 读取当前采样位置的音频数据（带线性插值）
            float sampleL = readSample(zone, voice.position, 0);
            float sampleR = sampleL;
            if (zone.buffer.getNumChannels() > 1) {
                sampleR = readSample(zone, voice.position, 1);
            }

            // 计算声像增益（线性声像定位）
            float leftGain = (1.0f - voice.pan) * 0.5f;
            float rightGain = (1.0f + voice.pan) * 0.5f;

            sampleSumL += sampleL * leftGain * voice.volume * voice.velocity;
            sampleSumR += sampleR * rightGain * voice.volume * voice.velocity;

            // 推进播放位置
            voice.position += voice.pitchRatio;

            // 检查是否播放完毕（到达缓冲区末尾）
            int bufferLength = zone.buffer.getNumSamples();
            if (voice.position >= static_cast<float>(bufferLength)) {
                // 处理循环模式
                if (zone.loop && loopMode_ != LoopMode::NoLoop) {
                    // 循环：回到循环起始点
                    voice.position = static_cast<float>(zone.loopStart);
                }
                else {
                    // 不循环：声音终止
                    voice.active = false;
                    voice.midiNote = -1;
                }
            }
        }

        // 应用总音量并写入输出缓冲区
        outL[i] = sampleSumL * volume_;
        outR[i] = sampleSumR * volume_;
    }
}

void MultiSampler::releaseResources() {
    AudioNode::releaseResources();
    // 释放所有声音资源
    for (auto& voice : voices_) {
        voice.active = false;
        voice.midiNote = -1;
    }
}

// =============================================================================
// 采样管理
// =============================================================================
int MultiSampler::addSample(const juce::File& file, int rootNote, int lowNote, int highNote) {
    // 检查采样区域数量上限
    if (sampleZones_.size() >= kMaxSampleZones) {
        return -1; // 已达上限
    }

    if (!file.existsAsFile()) {
        return -1; // 文件不存在
    }

    // 使用 JUCE AudioFormatManager 加载WAV文件
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(file));
    if (reader == nullptr) {
        return -1; // 无法解析音频格式
    }

    int numSamples = static_cast<int>(reader->lengthInSamples);
    int numChannels = static_cast<int>(reader->numChannels);

    // 创建采样区域
    SampleZone zone;
    zone.samplePath = file.getFullPathName();
    zone.rootNote = rootNote;
    zone.lowNote = lowNote;
    zone.highNote = highNote;
    zone.buffer.setSize(numChannels, numSamples);
    reader->read(&zone.buffer, 0, numSamples, 0, true, true);

    // 设置循环点为整个缓冲区
    zone.loopStart = 0;
    zone.loopEnd = numSamples;
    zone.loop = false;
    zone.active = true;

    sampleZones_.push_back(zone);
    return static_cast<int>(sampleZones_.size()) - 1; // 返回新添加区域的索引
}

void MultiSampler::removeSample(int index) {
    if (index >= 0 && index < static_cast<int>(sampleZones_.size())) {
        sampleZones_.erase(sampleZones_.begin() + index);
    }
}

void MultiSampler::clearSamples() {
    sampleZones_.clear();
    // 同时终止所有活跃声音
    for (auto& voice : voices_) {
        voice.active = false;
        voice.midiNote = -1;
    }
}

// =============================================================================
// 键位映射
// =============================================================================
void MultiSampler::setKeyRange(int sampleIndex, int lowNote, int highNote) {
    if (sampleIndex >= 0 && sampleIndex < static_cast<int>(sampleZones_.size())) {
        sampleZones_[static_cast<size_t>(sampleIndex)].lowNote = lowNote;
        sampleZones_[static_cast<size_t>(sampleIndex)].highNote = highNote;
    }
}

void MultiSampler::setVelocityRange(int sampleIndex, int lowVel, int highVel) {
    if (sampleIndex >= 0 && sampleIndex < static_cast<int>(sampleZones_.size())) {
        sampleZones_[static_cast<size_t>(sampleIndex)].lowVelocity = lowVel;
        sampleZones_[static_cast<size_t>(sampleIndex)].highVelocity = highVel;
    }
}

void MultiSampler::setRootNote(int sampleIndex, int note) {
    if (sampleIndex >= 0 && sampleIndex < static_cast<int>(sampleZones_.size())) {
        sampleZones_[static_cast<size_t>(sampleIndex)].rootNote = note;
    }
}

// =============================================================================
// AI自动映射
// =============================================================================
void MultiSampler::autoMapSamples(const std::vector<juce::File>& files) {
    if (files.empty()) return;

    // 计算每个文件覆盖的键位范围（88键标准钢琴）
    int totalKeys = 88; // A0(21) 到 C8(108)
    int keyStart = 21;  // MIDI音符A0

    for (size_t i = 0; i < files.size(); ++i) {
        const juce::File& file = files[i];
        if (!file.existsAsFile()) continue;

        juce::String fileName = file.getFileNameWithoutExtension();

        // 尝试从文件名中检测根音
        // 支持格式: "C4", "C#4", "Db4", "60", "piano-C4", "C4-piano" 等
        int detectedRoot = 60; // 默认C4

        // 正则匹配: 音符名+八度编号 (如 C4, F#3, Eb5)
        juce::StringArray noteNames = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        bool found = false;

        for (int ni = 0; ni < noteNames.size() && !found; ++ni) {
            int idx = fileName.indexOf(noteNames[ni]);
            if (idx >= 0) {
                // 检查后面是否有八度编号
                int octaveIdx = idx + noteNames[ni].length();
                if (octaveIdx < fileName.length() && juce::CharacterFunctions::isDigit(fileName[octaveIdx])) {
                    int octave = fileName[octaveIdx] - '0';
                    // MIDI音符 = 八度*12 + 半音索引 + 12 (C0=12, 所以C4=60)
                    // 八度0从C0开始，MIDI编号: C0=12, C1=24, C2=36, C3=48, C4=60
                    detectedRoot = (octave + 1) * 12 + ni; // +1因为MIDI音名从C-1开始
                    found = true;
                }
            }
        }

        // 如果没找到音符名，尝试检测纯数字 (MIDI音符编号)
        if (!found) {
            // 查找文件名中的数字
            juce::String digits;
            for (int ci = 0; ci < fileName.length(); ++ci) {
                if (juce::CharacterFunctions::isDigit(fileName[ci])) {
                    digits += fileName[ci];
                }
            }
            if (digits.isNotEmpty()) {
                int num = digits.getIntValue();
                // 如果是合理的MIDI音符编号范围 (0-127)
                if (num >= 0 && num <= 127) {
                    detectedRoot = num;
                }
            }
        }

        // 计算每个文件覆盖的键位范围
        int keysPerFile = totalKeys / static_cast<int>(files.size());
        int remainder = totalKeys % static_cast<int>(files.size());
        int localLow = keyStart + static_cast<int>(i) * keysPerFile;
        int localHigh = localLow + keysPerFile - 1;
        // 最后一个文件获得余数键位
        if (i == files.size() - 1) {
            localHigh += remainder;
        }

        // 添加采样区域
        addSample(file, detectedRoot, localLow, localHigh);
    }
}

void MultiSampler::detectSlices(const juce::File& audioFile, float /*threshold*/) {
    // 占位符：AI切片检测功能
    // 未来将通过机器学习算法自动检测音频中的瞬态/切片点
    // 当前版本仅保留接口，待后续实现
    juce::ignoreUnused(audioFile);
}

// =============================================================================
// 播放控制
// =============================================================================
void MultiSampler::setLoopMode(LoopMode mode) {
    loopMode_ = mode;
}

void MultiSampler::setCrossfadeLength(int samples) {
    crossfadeLength_ = juce::jmax(0, samples);
}

void MultiSampler::setVolume(float volume) {
    volume_ = AudioUtils::clamp(volume, 0.0f, 1.0f);
}

// =============================================================================
// 查找匹配的采样区域
// =============================================================================
const SampleZone* MultiSampler::findZone(int midiNote, int velocity) const {
    // 遍历所有采样区域，查找第一个匹配键位和力度范围的区域
    for (const auto& zone : sampleZones_) {
        if (!zone.active) continue;
        if (midiNote >= zone.lowNote && midiNote <= zone.highNote &&
            velocity >= zone.lowVelocity && velocity <= zone.highVelocity) {
            return &zone;
        }
    }
    return nullptr; // 未找到匹配区域
}

// =============================================================================
// 查找空闲声音 (内部方法)
// =============================================================================
MultiSampler::Voice& MultiSampler::findFreeVoice() {
    // 优先查找不活跃的声音槽位
    for (auto& voice : voices_) {
        if (!voice.active) {
            return voice;
        }
    }
    // 所有声音都活跃时，偷取最旧的声音（数组第一个）
    // 简单策略：直接返回第一个声音
    voices_[0].active = false;
    voices_[0].midiNote = -1;
    return voices_[0];
}

// =============================================================================
// 处理单个声音 (内部方法)
// =============================================================================
void MultiSampler::processVoice(Voice& voice, juce::AudioBuffer<float>& buffer, int numSamples) {
    if (!voice.active) return;
    int zoneIdx = voice.zoneIndex;
    if (zoneIdx < 0 || zoneIdx >= static_cast<int>(sampleZones_.size())) {
        voice.active = false;
        return;
    }

    const SampleZone& zone = sampleZones_[static_cast<size_t>(zoneIdx)];
    int bufferLength = zone.buffer.getNumSamples();
    if (bufferLength == 0) {
        voice.active = false;
        return;
    }

    float* outL = buffer.getWritePointer(0);
    float* outR = buffer.getWritePointer(1);
    int numChannels = zone.buffer.getNumChannels();

    for (int i = 0; i < numSamples; ++i) {
        // 读取采样数据（线性插值）
        float sampleL = readSample(zone, voice.position, 0);
        float sampleR = sampleL;
        if (numChannels > 1) {
            sampleR = readSample(zone, voice.position, 1);
        }

        // 声像定位
        float leftGain = (1.0f - voice.pan) * 0.5f;
        float rightGain = (1.0f + voice.pan) * 0.5f;

        outL[i] += sampleL * leftGain * voice.volume * voice.velocity;
        outR[i] += sampleR * rightGain * voice.volume * voice.velocity;

        // 推进播放位置
        voice.position += voice.pitchRatio;

        // 循环处理
        if (voice.position >= static_cast<float>(bufferLength)) {
            if (zone.loop && loopMode_ != LoopMode::NoLoop) {
                // 回到循环起始点
                voice.position = static_cast<float>(zone.loopStart);
                // 乒乓循环可在此扩展
            }
            else {
                voice.active = false;
                voice.midiNote = -1;
                break;
            }
        }
    }
}

// =============================================================================
// 读取采样 (内部方法 - 线性插值，循环边界处理)
// =============================================================================
float MultiSampler::readSample(const SampleZone& zone, float position, int channel) {
    int bufferLength = zone.buffer.getNumSamples();
    if (bufferLength == 0) return 0.0f;

    // 确保通道索引有效
    int numChannels = zone.buffer.getNumChannels();
    if (channel >= numChannels) channel = numChannels - 1;
    if (channel < 0) channel = 0;

    // 环绕处理：根据循环模式决定边界行为
    if (zone.loop && loopMode_ != LoopMode::NoLoop && zone.loopStart < zone.loopEnd) {
        // 循环模式：将位置限制在循环区域内
        int loopLen = zone.loopEnd - zone.loopStart;
        if (loopLen <= 0) loopLen = bufferLength;

        // 将position映射到循环区域
        float localPos = position - static_cast<float>(zone.loopStart);
        while (localPos < 0.0f) localPos += static_cast<float>(loopLen);
        while (localPos >= static_cast<float>(loopLen)) localPos -= static_cast<float>(loopLen);

        position = static_cast<float>(zone.loopStart) + localPos;
    }
    else {
        // 非循环模式：超出范围返回0
        if (position < 0.0f || position >= static_cast<float>(bufferLength)) {
            return 0.0f;
        }
    }

    // 线性插值
    int idx0 = static_cast<int>(position);
    int idx1 = idx0 + 1;

    // 处理边界环绕
    if (idx1 >= bufferLength) {
        if (zone.loop && loopMode_ != LoopMode::NoLoop && zone.loopStart < zone.loopEnd) {
            idx1 = zone.loopStart; // 循环回起点
        }
        else {
            idx1 = bufferLength - 1; // 钳制到最后一个采样
        }
    }

    float frac = position - static_cast<float>(idx0);
    const float* data = zone.buffer.getReadPointer(channel);
    float sample0 = data[idx0];
    float sample1 = data[idx1];

    return AudioUtils::lerp(sample0, sample1, frac);
}

// =============================================================================
// 参数接口 (4个参数)
// 0: 音量, 1: 循环模式, 2: 交叉淡化长度, 3: 音高偏移
// =============================================================================
float MultiSampler::getParameter(int index) const {
    switch (index) {
        case 0: return volume_;                                                              // 音量
        case 1: return static_cast<float>(static_cast<int>(loopMode_)) / 2.0f;               // 循环模式 (0=NoLoop, 1=Forward, 2=PingPong)
        case 2: return static_cast<float>(crossfadeLength_) / 4096.0f;                       // 交叉淡化长度 (0~4096 → 0~1)
        case 3: return 0.5f;                                                                 // 总音高 (默认居中)
        default: return 0.0f;
    }
}

void MultiSampler::setParameter(int index, float value) {
    switch (index) {
        case 0: setVolume(value); break;
        case 1: setLoopMode(static_cast<LoopMode>(static_cast<int>(value * 2.999f))); break;
        case 2: setCrossfadeLength(static_cast<int>(value * 4096.0f)); break;
        case 3: /* 总音高 - 暂无全局音高偏移 */ break;
        default: break;
    }
}

juce::String MultiSampler::getParameterName(int index) const {
    switch (index) {
        case 0: return "音量";
        case 1: return "循环模式";
        case 2: return "交叉淡化";
        case 3: return "音高";
        default: return "未知";
    }
}

// =============================================================================
// JSON 序列化
// =============================================================================
juce::var MultiSampler::toJson() const {
    auto json = AudioNode::toJson();

    // 播放控制参数
    json.getDynamicObject()->setProperty("volume", volume_);
    json.getDynamicObject()->setProperty("loopMode", static_cast<int>(loopMode_));
    json.getDynamicObject()->setProperty("crossfadeLength", crossfadeLength_);

    // 序列化所有采样区域
    juce::Array<juce::var> zonesArray;
    for (const auto& zone : sampleZones_) {
        juce::DynamicObject::Ptr zoneObj = new juce::DynamicObject();
        zoneObj->setProperty("samplePath", zone.samplePath);
        zoneObj->setProperty("rootNote", zone.rootNote);
        zoneObj->setProperty("lowNote", zone.lowNote);
        zoneObj->setProperty("highNote", zone.highNote);
        zoneObj->setProperty("lowVelocity", zone.lowVelocity);
        zoneObj->setProperty("highVelocity", zone.highVelocity);
        zoneObj->setProperty("volume", zone.volume);
        zoneObj->setProperty("pan", zone.pan);
        zoneObj->setProperty("tune", zone.tune);
        zoneObj->setProperty("loop", zone.loop);
        zoneObj->setProperty("loopStart", zone.loopStart);
        zoneObj->setProperty("loopEnd", zone.loopEnd);
        zoneObj->setProperty("active", zone.active);

        // 缓冲区数据序列化为Base64编码的二进制数据
        juce::MemoryBlock block(zone.buffer.getRawDataPointer(),
            static_cast<size_t>(zone.buffer.getNumSamples() * zone.buffer.getNumChannels() * sizeof(float)));
        zoneObj->setProperty("bufferData", block.toBase64Encoding());
        zoneObj->setProperty("bufferNumChannels", zone.buffer.getNumChannels());
        zoneObj->setProperty("bufferNumSamples", zone.buffer.getNumSamples());

        zonesArray.add(juce::var(zoneObj));
    }
    json.getDynamicObject()->setProperty("zones", zonesArray);

    return json;
}

void MultiSampler::fromJson(const juce::var& json) {
    AudioNode::fromJson(json);

    if (auto* obj = json.getDynamicObject()) {
        // 播放控制参数
        volume_ = static_cast<float>(obj->getProperty("volume").operator double());
        loopMode_ = static_cast<LoopMode>(
            static_cast<int>(obj->getProperty("loopMode").operator double()));
        crossfadeLength_ = static_cast<int>(obj->getProperty("crossfadeLength").operator double());

        // 反序列化采样区域
        sampleZones_.clear();
        if (auto* zonesArray = obj->getProperty("zones").getArray()) {
            for (const auto& zoneVar : *zonesArray) {
                if (auto* zoneObj = zoneVar.getDynamicObject()) {
                    SampleZone zone;
                    zone.samplePath = zoneObj->getProperty("samplePath").toString();
                    zone.rootNote = static_cast<int>(zoneObj->getProperty("rootNote").operator double());
                    zone.lowNote = static_cast<int>(zoneObj->getProperty("lowNote").operator double());
                    zone.highNote = static_cast<int>(zoneObj->getProperty("highNote").operator double());
                    zone.lowVelocity = static_cast<int>(zoneObj->getProperty("lowVelocity").operator double());
                    zone.highVelocity = static_cast<int>(zoneObj->getProperty("highVelocity").operator double());
                    zone.volume = static_cast<float>(zoneObj->getProperty("volume").operator double());
                    zone.pan = static_cast<float>(zoneObj->getProperty("pan").operator double());
                    zone.tune = static_cast<float>(zoneObj->getProperty("tune").operator double());
                    zone.loop = static_cast<bool>(zoneObj->getProperty("loop").operator double());
                    zone.loopStart = static_cast<int>(zoneObj->getProperty("loopStart").operator double());
                    zone.loopEnd = static_cast<int>(zoneObj->getProperty("loopEnd").operator double());
                    zone.active = static_cast<bool>(zoneObj->getProperty("active").operator double());

                    // 恢复缓冲区数据
                    int bufferChannels = static_cast<int>(zoneObj->getProperty("bufferNumChannels").operator double());
                    int bufferSamples = static_cast<int>(zoneObj->getProperty("bufferNumSamples").operator double());
                    if (bufferChannels > 0 && bufferSamples > 0) {
                        juce::String base64Data = zoneObj->getProperty("bufferData").toString();
                        if (base64Data.isNotEmpty()) {
                            juce::MemoryBlock block;
                            if (block.fromBase64Encoding(base64Data)) {
                                zone.buffer.setSize(bufferChannels, bufferSamples);
                                if (block.getSize() >= static_cast<size_t>(bufferChannels * bufferSamples * static_cast<int>(sizeof(float)))) {
                                    // 恢复通道顺序数据
                                    float* rawData = static_cast<float*>(block.getData());
                                    for (int ch = 0; ch < bufferChannels; ++ch) {
                                        zone.buffer.copyFrom(ch, 0, rawData + ch * bufferSamples, bufferSamples);
                                    }
                                }
                            }
                        }
                    }
                    sampleZones_.push_back(zone);
                }
            }
        }
    }
}

} // namespace LianCore