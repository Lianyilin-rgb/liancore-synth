// =============================================================================
// LianCore - DrumSlicer 鼓切片触发器 实现
// 将音频Loop自动/手动切片，通过MIDI触发每个切片
// =============================================================================
#include "MultiSampler.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include "../utils/AudioUtils.h"

namespace LianCore {

// =============================================================================
// 构造 / 析构
// =============================================================================
DrumSlicer::DrumSlicer(const juce::String& name)
    : AudioNode(NodeType::DrumSlicer, name) {
    // 初始化所有切片播放器为不活跃状态
    for (auto& player : slicePlayers_) {
        player.sliceIndex = -1;
        player.position = 0.0f;
        player.pitchRatio = 1.0f;
        player.velocity = 1.0f;
        player.pan = 0.0f;
        player.active = false;
    }
}

// =============================================================================
// 音频处理
// =============================================================================
void DrumSlicer::prepareToPlay(double sampleRate, int maxSamplesPerBlock) {
    AudioNode::prepareToPlay(sampleRate, maxSamplesPerBlock);
    sampleRate_ = sampleRate;
    blockSize_ = maxSamplesPerBlock;
}

void DrumSlicer::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    auto& output = getOutputBuffer(0);
    output.clear();

    int numSamples = buffer.getNumSamples();
    float* outL = output.getWritePointer(0);
    float* outR = output.getWritePointer(1);

    // 如果没有加载Loop或没有切片，输出静音
    if (!loopLoaded_ || slices_.empty() || loopBuffer_.getNumSamples() == 0) {
        AudioUtils::clearBufferSIMD(outL, numSamples);
        AudioUtils::clearBufferSIMD(outR, numSamples);
        return;
    }

    int loopChannels = loopBuffer_.getNumChannels();

    // ---- 处理MIDI事件，触发切片 ----
    for (const auto& metadata : midi) {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn()) {
            int midiNote = msg.getNoteNumber();
            float velocity = msg.getFloatVelocity();

            // 查找匹配的切片（按MIDI音符映射）
            for (int i = 0; i < static_cast<int>(slices_.size()); ++i) {
                if (slices_[i].midiNote == midiNote) {
                    triggerSlice(i, velocity);
                    break;
                }
            }
        }
    }

    // ---- 处理每个活跃的切片播放器 ----
    for (int sampleIdx = 0; sampleIdx < numSamples; ++sampleIdx) {
        float sampleOutL = 0.0f;
        float sampleOutR = 0.0f;

        for (auto& player : slicePlayers_) {
            if (!player.active || player.sliceIndex < 0) continue;
            if (player.sliceIndex >= static_cast<int>(slices_.size())) {
                player.active = false;
                continue;
            }

            const auto& slice = slices_[player.sliceIndex];
            int sliceLength = slice.endSample - slice.startSample;
            if (sliceLength <= 0) {
                player.active = false;
                continue;
            }

            // 读取切片音频（带线性插值）
            float sampleVal = readLoopSample(player.position, 0);

            // 声像计算
            float leftGain = (1.0f - player.pan) * 0.5f;
            float rightGain = (1.0f + player.pan) * 0.5f;

            sampleOutL += sampleVal * player.velocity * leftGain;
            sampleOutR += sampleVal * player.velocity * rightGain;

            // 根据方向更新播放位置
            if (slice.reverse) {
                player.position -= player.pitchRatio;
                // 反向播放：位置低于切片起始位置则结束
                if (player.position < static_cast<float>(slice.startSample)) {
                    player.active = false;
                }
            } else {
                player.position += player.pitchRatio;
                // 正向播放：位置超过切片结束位置则结束
                if (player.position >= static_cast<float>(slice.endSample)) {
                    player.active = false;
                }
            }
        }

        // 写入输出缓冲区
        outL[sampleIdx] = sampleOutL * volume_;
        outR[sampleIdx] = sampleOutR * volume_;
    }
}

void DrumSlicer::releaseResources() {
    AudioNode::releaseResources();
    // 停用所有播放器
    for (auto& player : slicePlayers_) {
        player.active = false;
    }
}

// =============================================================================
// Loop加载
// =============================================================================
void DrumSlicer::loadLoop(const juce::File& file) {
    if (!file.existsAsFile()) {
        loopLoaded_ = false;
        return;
    }

    // 使用 JUCE AudioFormatManager 读取WAV文件
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(file));
    if (reader == nullptr) {
        loopLoaded_ = false;
        return;
    }

    int numSamples = static_cast<int>(reader->lengthInSamples);
    int numChannels = static_cast<int>(reader->numChannels);

    // 限制加载长度（防止内存溢出，约30秒 @ 48kHz）
    if (numSamples > 1440000) {
        numSamples = 1440000;
    }

    loopBuffer_.setSize(numChannels, numSamples);
    reader->read(&loopBuffer_, 0, numSamples, 0, true, true);
    loopLoaded_ = true;

    // 清除旧切片
    slices_.clear();
}

// =============================================================================
// 切片检测（基于幅度阈值瞬态检测）
// =============================================================================
void DrumSlicer::detectSlices(float threshold, int minSliceLength) {
    if (!loopLoaded_ || loopBuffer_.getNumSamples() == 0) {
        return;
    }

    slices_.clear();

    int totalSamples = loopBuffer_.getNumSamples();
    const float* data = loopBuffer_.getReadPointer(0);

    // 阈值裁剪到合理范围
    threshold = AudioUtils::clamp(threshold, 0.0f, 1.0f);
    minSliceLength = std::max(1, minSliceLength);

    // 计算整个Loop的RMS作为参考
    float rmsSum = 0.0f;
    for (int i = 0; i < totalSamples; ++i) {
        rmsSum += data[i] * data[i];
    }
    float rms = std::sqrt(rmsSum / static_cast<float>(totalSamples));

    // 瞬态检测阈值：参考RMS乘以用户阈值
    float detectThreshold = rms * threshold * 3.0f; // 3倍RMS * 阈值比例
    if (detectThreshold < 0.001f) detectThreshold = 0.001f;

    // 瞬态检测窗口
    const int windowSize = 256; // 约5.8ms @ 44.1kHz
    const int holdTime = static_cast<int>(sampleRate_ * 0.02); // 20ms保持时间

    bool inSlice = false;
    int sliceStart = 0;
    int lastTransient = -holdTime; // 防止太近的瞬态

    for (int i = 0; i < totalSamples; i += windowSize / 2) {
        // 计算当前窗口的峰值幅度
        float peak = 0.0f;
        int windowEnd = std::min(i + windowSize, totalSamples);
        for (int j = i; j < windowEnd; ++j) {
            float absVal = std::abs(data[j]);
            if (absVal > peak) peak = absVal;
        }

        // 检测瞬态
        if (peak > detectThreshold && (i - lastTransient) > holdTime) {
            if (inSlice) {
                // 关闭上一个切片
                Slice slice;
                slice.startSample = sliceStart;
                slice.endSample = i;
                slice.midiNote = 60 + static_cast<int>(slices_.size()); // 自动分配MIDI音符
                slice.pitch = 0.0f;
                slice.reverse = false;
                slices_.push_back(slice);
            }

            // 开启新切片
            sliceStart = i;
            inSlice = true;
            lastTransient = i;

            // 限制切片数量
            if (static_cast<int>(slices_.size()) >= kMaxSlices) break;
        }
    }

    // 关闭最后一个切片
    if (inSlice && static_cast<int>(slices_.size()) < kMaxSlices) {
        Slice slice;
        slice.startSample = sliceStart;
        slice.endSample = totalSamples;
        slice.midiNote = 60 + static_cast<int>(slices_.size());
        slice.pitch = 0.0f;
        slice.reverse = false;
        slices_.push_back(slice);
    }

    // 过滤过短的切片
    if (minSliceLength > 1) {
        slices_.erase(
            std::remove_if(slices_.begin(), slices_.end(),
                [minSliceLength](const Slice& s) {
                    return (s.endSample - s.startSample) < minSliceLength;
                }),
            slices_.end()
        );
    }

    // 重新编号MIDI映射
    autoMapToMidi();
}

// =============================================================================
// 手动切片管理
// =============================================================================
void DrumSlicer::addManualSlice(int samplePos) {
    if (!loopLoaded_ || static_cast<int>(slices_.size()) >= kMaxSlices) {
        return;
    }

    int totalSamples = loopBuffer_.getNumSamples();
    samplePos = AudioUtils::clamp(static_cast<float>(samplePos), 0.0f, static_cast<float>(totalSamples - 1));

    Slice slice;
    slice.startSample = samplePos;
    // 找到最近的已有切片结束位置或音频结尾
    int nextEnd = totalSamples;
    for (const auto& existing : slices_) {
        if (existing.startSample > samplePos && existing.endSample < nextEnd) {
            nextEnd = existing.endSample;
        }
    }
    slice.endSample = nextEnd;
    slice.midiNote = 60 + static_cast<int>(slices_.size());
    slice.pitch = 0.0f;
    slice.reverse = false;

    // 按起始位置排序插入
    auto insertPos = slices_.begin();
    while (insertPos != slices_.end() && insertPos->startSample < samplePos) {
        ++insertPos;
    }
    slices_.insert(insertPos, slice);

    // 更新相邻切片的边界
    autoMapToMidi();
}

void DrumSlicer::removeSlice(int index) {
    if (index < 0 || index >= static_cast<int>(slices_.size())) {
        return;
    }

    slices_.erase(slices_.begin() + index);

    // 重新编号MIDI映射
    autoMapToMidi();

    // 停用指向该切片的播放器
    for (auto& player : slicePlayers_) {
        if (player.sliceIndex == index) {
            player.active = false;
        }
    }
}

// =============================================================================
// 切片触发
// =============================================================================
void DrumSlicer::triggerSlice(int index, float velocity) {
    if (index < 0 || index >= static_cast<int>(slices_.size())) {
        return;
    }

    // 查找空闲播放器
    SlicePlayer* target = nullptr;
    for (auto& player : slicePlayers_) {
        if (!player.active) {
            target = &player;
            break;
        }
    }
    if (target == nullptr) return; // 播放器池已满

    const auto& slice = slices_[index];

    target->sliceIndex = index;
    // 根据播放方向设置起始位置
    if (slice.reverse) {
        target->position = static_cast<float>(slice.endSample - 1);
    } else {
        target->position = static_cast<float>(slice.startSample);
    }
    target->pitchRatio = static_cast<float>(AudioUtils::semitonesToRatio(static_cast<double>(slice.pitch)));
    target->velocity = AudioUtils::clamp(velocity, 0.0f, 1.0f);
    target->pan = 0.0f;
    target->active = true;
}

// =============================================================================
// 切片属性设置
// =============================================================================
void DrumSlicer::setSlicePitch(int index, float semitones) {
    if (index < 0 || index >= static_cast<int>(slices_.size())) {
        return;
    }
    slices_[index].pitch = AudioUtils::clamp(semitones, -24.0f, 24.0f);
}

void DrumSlicer::setSliceDirection(int index, bool reverse) {
    if (index < 0 || index >= static_cast<int>(slices_.size())) {
        return;
    }
    slices_[index].reverse = reverse;
}

// =============================================================================
// MIDI映射
// =============================================================================
void DrumSlicer::setSliceNote(int index, int midiNote) {
    if (index < 0 || index >= static_cast<int>(slices_.size())) {
        return;
    }
    slices_[index].midiNote = AudioUtils::clamp(static_cast<float>(midiNote), 0.0f, 127.0f);
}

void DrumSlicer::autoMapToMidi() {
    // 从C4(60)开始，按顺序映射每个切片到半音阶
    for (int i = 0; i < static_cast<int>(slices_.size()); ++i) {
        slices_[i].midiNote = 60 + i;
        if (slices_[i].midiNote > 127) {
            slices_[i].midiNote = 127;
        }
    }
}

// =============================================================================
// 播放控制
// =============================================================================
void DrumSlicer::setVolume(float volume) {
    volume_ = AudioUtils::clamp(volume, 0.0f, 1.0f);
}

// =============================================================================
// Loop采样读取（线性插值，环绕）
// =============================================================================
float DrumSlicer::readLoopSample(float position, int channel) const {
    int totalSamples = loopBuffer_.getNumSamples();
    if (totalSamples == 0) return 0.0f;

    // 环绕处理：确保位置在 [0, totalSamples) 范围内
    while (position < 0.0f) position += static_cast<float>(totalSamples);
    while (position >= static_cast<float>(totalSamples)) position -= static_cast<float>(totalSamples);

    // 确保通道索引有效
    int numChannels = loopBuffer_.getNumChannels();
    if (channel >= numChannels) channel = numChannels - 1;
    if (channel < 0) channel = 0;

    // 线性插值
    int idx0 = static_cast<int>(position);
    int idx1 = idx0 + 1;
    if (idx1 >= totalSamples) idx1 = 0; // 环绕

    float frac = position - static_cast<float>(idx0);
    const float* data = loopBuffer_.getReadPointer(channel);
    float sample0 = data[idx0];
    float sample1 = data[idx1];

    return AudioUtils::lerp(sample0, sample1, frac);
}

// =============================================================================
// 参数接口 (1个参数: 音量)
// =============================================================================
float DrumSlicer::getParameter(int index) const {
    switch (index) {
        case 0: return volume_; // 音量
        default: return 0.0f;
    }
}

void DrumSlicer::setParameter(int index, float value) {
    switch (index) {
        case 0: setVolume(value); break;
        default: break;
    }
}

juce::String DrumSlicer::getParameterName(int index) const {
    switch (index) {
        case 0: return "音量";
        default: return "未知";
    }
}

// =============================================================================
// JSON 序列化
// =============================================================================
juce::var DrumSlicer::toJson() const {
    auto json = AudioNode::toJson();

    // 播放参数
    json.getDynamicObject()->setProperty("volume", volume_);

    // 切片数据
    juce::Array<juce::var> sliceArray;
    for (const auto& slice : slices_) {
        juce::DynamicObject::Ptr sliceObj = new juce::DynamicObject();
        sliceObj->setProperty("startSample", slice.startSample);
        sliceObj->setProperty("endSample", slice.endSample);
        sliceObj->setProperty("midiNote", slice.midiNote);
        sliceObj->setProperty("pitch", slice.pitch);
        sliceObj->setProperty("reverse", slice.reverse);
        sliceArray.add(juce::var(sliceObj.get()));
    }
    json.getDynamicObject()->setProperty("slices", sliceArray);

    return json;
}

void DrumSlicer::fromJson(const juce::var& json) {
    AudioNode::fromJson(json);

    if (auto* obj = json.getDynamicObject()) {
        // 播放参数
        volume_ = static_cast<float>(obj->getProperty("volume").operator double());

        // 切片数据
        slices_.clear();
        if (auto* sliceProp = obj->getProperty("slices").getArray()) {
            for (const auto& item : *sliceProp) {
                if (auto* sliceObj = item.getDynamicObject()) {
                    Slice slice;
                    slice.startSample = static_cast<int>(sliceObj->getProperty("startSample").operator double());
                    slice.endSample = static_cast<int>(sliceObj->getProperty("endSample").operator double());
                    slice.midiNote = static_cast<int>(sliceObj->getProperty("midiNote").operator double());
                    slice.pitch = static_cast<float>(sliceObj->getProperty("pitch").operator double());
                    slice.reverse = static_cast<bool>(sliceObj->getProperty("reverse").operator double());
                    slices_.push_back(slice);
                }
            }
        }
    }
}

} // namespace LianCore