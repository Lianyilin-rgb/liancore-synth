// =============================================================================
// LianCore - AudioNode 实现
// =============================================================================
#include "AudioNode.h"

namespace LianCore {

// 静态ID计数器初始化
std::atomic<int64_t> AudioNode::s_nextId{ 1 };

// =============================================================================
// 构造/析构
// =============================================================================
AudioNode::AudioNode(NodeType type, const juce::String& name)
    : nodeType_(type)
    , name_(name)
{
    // 生成唯一节点ID: "WavetableOscillator_1", "Filter_2", etc.
    nodeId_ = nodeTypeToString(type) + "_" + juce::String(s_nextId.fetch_add(1));
}

// =============================================================================
// 生命周期
// =============================================================================
void AudioNode::prepareToPlay(double sampleRate, int maxSamplesPerBlock) {
    sampleRate_ = sampleRate;
    maxSamplesPerBlock_ = maxSamplesPerBlock;

    // 为所有端口分配缓冲区
    for (auto& port : inputPorts_) {
        port.buffer.setSize(2, maxSamplesPerBlock); // 立体声
        port.buffer.clear();
    }
    for (auto& port : outputPorts_) {
        port.buffer.setSize(2, maxSamplesPerBlock);
        port.buffer.clear();
    }
}

void AudioNode::releaseResources() {
    // 释放所有端口缓冲区
    for (auto& port : inputPorts_) {
        port.buffer.setSize(0, 0);
    }
    for (auto& port : outputPorts_) {
        port.buffer.setSize(0, 0);
    }
}

// =============================================================================
// 端口管理
// =============================================================================
const PortDescriptor& AudioNode::getPortDescriptor(int portIndex, bool isInput) const {
    const auto& ports = isInput ? inputPorts_ : outputPorts_;
    jassert(portIndex >= 0 && portIndex < static_cast<int>(ports.size()));
    return ports[static_cast<size_t>(portIndex)].descriptor;
}

bool AudioNode::isPortConnected(int portIndex, bool isInput) const {
    const auto& ports = isInput ? inputPorts_ : outputPorts_;
    jassert(portIndex >= 0 && portIndex < static_cast<int>(ports.size()));
    return ports[static_cast<size_t>(portIndex)].isConnected;
}

void AudioNode::setPortConnected(int portIndex, bool isInput, bool connected) {
    auto& ports = isInput ? inputPorts_ : outputPorts_;
    jassert(portIndex >= 0 && portIndex < static_cast<int>(ports.size()));
    ports[static_cast<size_t>(portIndex)].isConnected = connected;
}

// =============================================================================
// 缓冲区访问
// =============================================================================
juce::AudioBuffer<float>& AudioNode::getInputBuffer(int portIndex) {
    jassert(portIndex >= 0 && portIndex < static_cast<int>(inputPorts_.size()));
    return inputPorts_[static_cast<size_t>(portIndex)].buffer;
}

juce::AudioBuffer<float>& AudioNode::getOutputBuffer(int portIndex) {
    jassert(portIndex >= 0 && portIndex < static_cast<int>(outputPorts_.size()));
    return outputPorts_[static_cast<size_t>(portIndex)].buffer;
}

const juce::AudioBuffer<float>& AudioNode::getOutputBuffer(int portIndex) const {
    jassert(portIndex >= 0 && portIndex < static_cast<int>(outputPorts_.size()));
    return outputPorts_[static_cast<size_t>(portIndex)].buffer;
}

// =============================================================================
// 端口初始化
// =============================================================================
void AudioNode::addInputPort(const juce::String& name, const PortDescriptor& desc) {
    AudioPort port;
    port.name = name;
    port.descriptor = desc;
    port.isConnected = false;
    inputPorts_.push_back(std::move(port));
}

void AudioNode::addOutputPort(const juce::String& name, const PortDescriptor& desc) {
    AudioPort port;
    port.name = name;
    port.descriptor = desc;
    port.isConnected = false;
    outputPorts_.push_back(std::move(port));
}

// =============================================================================
// 性能追踪
// =============================================================================
void AudioNode::updateProcessingTime(double ms) {
    lastProcessingTimeMs_ = ms;
}

void AudioNode::updateMemoryUsage(size_t bytes) {
    memoryUsageBytes_ = bytes;
}

// =============================================================================
// 序列化
// =============================================================================
juce::var AudioNode::toJson() const {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("nodeId", nodeId_);
    obj->setProperty("nodeType", nodeTypeToString(nodeType_));
    obj->setProperty("name", name_);

    // 序列化参数
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

void AudioNode::fromJson(const juce::var& json) {
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

// =============================================================================
// 节点类型名称映射
// =============================================================================
juce::String nodeTypeToString(NodeType type) {
    switch (type) {
        case NodeType::WavetableOscillator:        return "WavetableOscillator";
        case NodeType::VirtualAnalogOscillator:    return "VirtualAnalogOscillator";
        case NodeType::NoiseGenerator:             return "NoiseGenerator";
        case NodeType::SpectralOscillator:         return "SpectralOscillator";
        case NodeType::GranularPlayer:      return "GranularPlayer";
        case NodeType::WaveguideResonator:  return "WaveguideResonator";
        case NodeType::MultiSampler:        return "MultiSampler";
        case NodeType::DrumSlicer:          return "DrumSlicer";
        case NodeType::Filter:              return "Filter";
        case NodeType::Distortion:          return "Distortion";
        case NodeType::Delay:               return "Delay";
        case NodeType::Reverb:              return "Reverb";
        case NodeType::Compressor:          return "Compressor";
        case NodeType::EQ:                  return "EQ";
        case NodeType::LFO:                 return "LFO";
        case NodeType::Envelope:            return "Envelope";
        case NodeType::MacroControl:        return "MacroControl";
        case NodeType::StepSequencer:       return "StepSequencer";
        case NodeType::Mixer:               return "Mixer";
        case NodeType::Splitter:            return "Splitter";
        case NodeType::AudioInput:          return "AudioInput";
        case NodeType::AudioOutput:         return "AudioOutput";
        default: return "Unknown";
    }
}

NodeType stringToNodeType(const juce::String& str) {
    static const std::unordered_map<juce::String, NodeType> map = {
        {"WavetableOscillator",        NodeType::WavetableOscillator},
        {"VirtualAnalogOscillator",    NodeType::VirtualAnalogOscillator},
        {"NoiseGenerator",             NodeType::NoiseGenerator},
        {"SpectralOscillator",         NodeType::SpectralOscillator},
        {"GranularPlayer",      NodeType::GranularPlayer},
        {"WaveguideResonator",  NodeType::WaveguideResonator},
        {"MultiSampler",        NodeType::MultiSampler},
        {"DrumSlicer",          NodeType::DrumSlicer},
        {"Filter",              NodeType::Filter},
        {"Distortion",          NodeType::Distortion},
        {"Delay",               NodeType::Delay},
        {"Reverb",              NodeType::Reverb},
        {"Compressor",          NodeType::Compressor},
        {"EQ",                  NodeType::EQ},
        {"LFO",                 NodeType::LFO},
        {"Envelope",            NodeType::Envelope},
        {"MacroControl",        NodeType::MacroControl},
        {"StepSequencer",       NodeType::StepSequencer},
        {"Mixer",               NodeType::Mixer},
        {"Splitter",            NodeType::Splitter},
        {"AudioInput",          NodeType::AudioInput},
        {"AudioOutput",         NodeType::AudioOutput},
    };
    auto it = map.find(str);
    return it != map.end() ? it->second : NodeType::AudioOutput;
}

} // namespace LianCore