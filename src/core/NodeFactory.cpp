// =============================================================================
// LianCore - NodeFactory 实现
// =============================================================================
#include "NodeFactory.h"

// 合成引擎节点 (Alpha - 已完成)
#include "WavetableOscillator.h"
#include "VirtualAnalogOscillator.h"
#include "NoiseGenerator.h"

// 合成引擎节点 (Beta - 新增)
#include "SpectralOscillator.h"
#include "GranularPlayer.h"
#include "WaveguideResonator.h"
#include "MultiSampler.h"  // 包含 MultiSampler, DrumSlicer, StepSequencer

// 信号处理节点
#include "FilterProcessor.h"

// 信号处理节点 (Beta Week 2 - 效果器链)
#include "Distortion.h"
#include "Delay.h"
#include "Reverb.h"
#include "Compressor.h"
#include "EQ.h"

// 调制节点
#include "EnvelopeGenerator.h"
#include "LFOGenerator.h"

namespace LianCore {

// =============================================================================
// 内部辅助节点类 (定义必须在 createNode 使用之前)
// =============================================================================

// 占位节点实现 (用于未实现的节点类型)
class PlaceholderNode : public AudioNode {
public:
    PlaceholderNode(NodeType type, const juce::String& name)
        : AudioNode(type, name) {}

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {
        // 占位节点: 静音通过
    }
};

// 混合器节点实现
class MixerNode : public AudioNode {
public:
    MixerNode(const juce::String& name)
        : AudioNode(NodeType::Mixer, name) {}

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override {
        auto& output = getOutputBuffer(0);
        output.clear();

        // 混合所有连接的输入
        for (int i = 0; i < getNumInputPorts(); ++i) {
            if (isPortConnected(i, true)) {
                const auto& input = getInputBuffer(i);
                for (int ch = 0; ch < output.getNumChannels(); ++ch) {
                    output.addFrom(ch, 0, input, ch, 0, input.getNumSamples());
                }
            }
        }
    }
};

// 音频输出节点实现
class AudioOutputNode : public AudioNode {
public:
    AudioOutputNode(const juce::String& name)
        : AudioNode(NodeType::AudioOutput, name) {}

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override {
        if (isPortConnected(0, true)) {
            const auto& input = getInputBuffer(0);
            // 将输入复制到主输出缓冲区
            for (int ch = 0; ch < buffer.getNumChannels() && ch < input.getNumChannels(); ++ch) {
                buffer.copyFrom(ch, 0, input, ch, 0, input.getNumSamples());
            }
        }
    }
};

// =============================================================================
// NodeFactory::createNode
// =============================================================================

std::unique_ptr<AudioNode> NodeFactory::createNode(NodeType type, const juce::String& name) {
    juce::String nodeName = name.isEmpty() ? getDefaultName(type) : name;

    std::unique_ptr<AudioNode> node;

    switch (type) {
        // =====================================================================
        // 合成引擎 (Alpha阶段实现)
        // =====================================================================
        case NodeType::WavetableOscillator:
            node = std::make_unique<WavetableOscillator>(nodeName);
            break;

        case NodeType::VirtualAnalogOscillator:
            node = std::make_unique<VirtualAnalogOscillator>(nodeName);
            break;

        case NodeType::NoiseGenerator:
            node = std::make_unique<NoiseGenerator>(nodeName);
            break;

        // =====================================================================
        // 信号处理 (Alpha阶段实现)
        // =====================================================================
        case NodeType::Filter:
            node = std::make_unique<FilterProcessor>(nodeName);
            break;

        case NodeType::Mixer:
            // 混合器节点 - 暂用基础AudioNode实现
            node = std::make_unique<MixerNode>(nodeName);
            break;

        // =====================================================================
        // 调制器 (Alpha阶段实现)
        // =====================================================================
        case NodeType::Envelope:
            node = std::make_unique<EnvelopeGenerator>(nodeName);
            break;

        case NodeType::LFO:
            node = std::make_unique<LFOGenerator>(nodeName);
            break;

        // =====================================================================
        // 路由节点
        // =====================================================================
        case NodeType::AudioOutput:
            node = std::make_unique<AudioOutputNode>(nodeName);
            break;

        // =====================================================================
        // 合成引擎 (Beta阶段实现)
        // =====================================================================
        case NodeType::SpectralOscillator:
            node = std::make_unique<SpectralOscillator>(nodeName);
            break;

        case NodeType::GranularPlayer:
            node = std::make_unique<GranularPlayer>(nodeName);
            break;

        case NodeType::WaveguideResonator:
            node = std::make_unique<WaveguideResonator>(nodeName);
            break;

        case NodeType::MultiSampler:
            node = std::make_unique<MultiSampler>(nodeName);
            break;

        case NodeType::DrumSlicer:
            node = std::make_unique<DrumSlicer>(nodeName);
            break;

        case NodeType::StepSequencer:
            node = std::make_unique<StepSequencer>(nodeName);
            break;

        // =====================================================================
        // 效果器链 (Beta Week 2实现)
        // =====================================================================
        case NodeType::Distortion:
            node = std::make_unique<Distortion>(nodeName);
            break;

        case NodeType::Delay:
            node = std::make_unique<Delay>(nodeName);
            break;

        case NodeType::Reverb:
            node = std::make_unique<Reverb>(nodeName);
            break;

        case NodeType::Compressor:
            node = std::make_unique<Compressor>(nodeName);
            break;

        case NodeType::EQ:
            node = std::make_unique<EQ>(nodeName);
            break;

        // =====================================================================
        // 待实现 (Release阶段)
        // =====================================================================
        case NodeType::MacroControl:
        case NodeType::Splitter:
        case NodeType::AudioInput:
        default:
            // 未实现的节点类型创建占位节点
            node = std::make_unique<PlaceholderNode>(type, nodeName);
            break;
    }

    if (node) {
        configureDefaultPorts(node.get());
    }

    return node;
}

juce::String NodeFactory::getDefaultName(NodeType type) {
    switch (type) {
        case NodeType::WavetableOscillator: return "波表振荡器";
        case NodeType::SpectralOscillator:  return "频谱振荡器";
        case NodeType::GranularPlayer:      return "粒子播放器";
        case NodeType::WaveguideResonator:  return "波导谐振器";
        case NodeType::MultiSampler:        return "多采样器";
        case NodeType::DrumSlicer:          return "鼓切片器";
        case NodeType::Filter:              return "滤波器";
        case NodeType::Distortion:          return "失真";
        case NodeType::Delay:               return "延迟";
        case NodeType::Reverb:              return "混响";
        case NodeType::Compressor:          return "压缩器";
        case NodeType::EQ:                  return "均衡器";
        case NodeType::LFO:                 return "LFO";
        case NodeType::Envelope:            return "包络";
        case NodeType::MacroControl:        return "宏控制";
        case NodeType::StepSequencer:       return "步进音序器";
        case NodeType::Mixer:               return "混合器";
        case NodeType::Splitter:            return "分离器";
        case NodeType::AudioInput:          return "音频输入";
        case NodeType::AudioOutput:         return "音频输出";
        default: return "未知节点";
    }
}

void NodeFactory::configureDefaultPorts(AudioNode* node) {
    if (!node) return;

    PortDescriptor audioDesc;
    audioDesc.isAudio = true;
    audioDesc.defaultValue = 0.0f;
    audioDesc.minValue = -1.0f;
    audioDesc.maxValue = 1.0f;
    audioDesc.unit = "";

    PortDescriptor controlDesc;
    controlDesc.isAudio = false;
    controlDesc.defaultValue = 0.0f;
    controlDesc.minValue = 0.0f;
    controlDesc.maxValue = 1.0f;
    controlDesc.unit = "";

    switch (node->getNodeType()) {
        case NodeType::WavetableOscillator:
        case NodeType::VirtualAnalogOscillator:
        case NodeType::NoiseGenerator:
        case NodeType::SpectralOscillator:
        case NodeType::GranularPlayer:
        case NodeType::WaveguideResonator:
        case NodeType::MultiSampler:
        case NodeType::DrumSlicer:
            // 振荡器/合成器: 无音频输入, 1个音频输出
            node->addOutputPort("音频输出", audioDesc);
            break;

        case NodeType::StepSequencer:
            // 步进音序器: 无音频输入, 1个控制输出
            node->addOutputPort("序列输出", controlDesc);
            break;

        case NodeType::Filter:
        case NodeType::Distortion:
        case NodeType::Delay:
        case NodeType::Reverb:
        case NodeType::Compressor:
        case NodeType::EQ:
            // 效果器: 1个音频输入, 1个音频输出
            node->addInputPort("音频输入", audioDesc);
            node->addOutputPort("音频输出", audioDesc);
            break;

        case NodeType::Envelope:
        case NodeType::LFO:
            // 调制器: 无音频端口, 仅控制输出
            node->addOutputPort("调制输出", controlDesc);
            break;

        case NodeType::Mixer:
            // 混合器: 多路输入, 1路输出
            for (int i = 1; i <= 8; ++i) {
                node->addInputPort("输入 " + juce::String(i), audioDesc);
            }
            node->addOutputPort("混合输出", audioDesc);
            break;

        case NodeType::AudioOutput:
            // 输出节点: 1路输入, 无输出
            node->addInputPort("主输入", audioDesc);
            break;

        default:
            break;
    }
}

} // namespace LianCore