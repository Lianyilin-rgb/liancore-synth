// =============================================================================
// LianCore - AudioNode 音频节点基类
// 统一节点式音频图架构的核心抽象
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <string>
#include <memory>

namespace LianCore {

// =============================================================================
// 前向声明
// =============================================================================
using NodeId = juce::String;
using ConnectionId = juce::String;

// =============================================================================
// 节点类型枚举 - 对齐规格文档中的15种节点类型
// =============================================================================
enum class NodeType {
    // 合成引擎
    WavetableOscillator,        // 波表振荡器 (对标 Serum 2)
    VirtualAnalogOscillator,    // 虚拟模拟振荡器
    NoiseGenerator,             // 噪声发生器
    SpectralOscillator,         // 频谱振荡器 (对标 Vital)
    GranularPlayer,             // 粒子播放器 (对标 Absynth)
    WaveguideResonator,         // 波导谐振器 (对标 Absynth)
    MultiSampler,               // 多采样播放器 (对标 Avenger 2)
    DrumSlicer,                 // 鼓切片触发器 (对标 Avenger 2)

    // 信号处理
    Filter,                 // 滤波器
    Distortion,             // 失真
    Delay,                  // 延迟
    Reverb,                 // 混响
    Compressor,             // 压缩器
    EQ,                     // 均衡器
    Chorus,                 // 合声 (P1-2)
    Flanger,                // 镶边 (P1-2)
    Phaser,                 // 移相 (P1-2)
    BitCrusher,             // 比特粉碎 (P1-2)
    RingMod,                // 环形调制 (P1-2)
    ConvolutionReverb,      // 卷积混响 (P1-2)

    // 调制器
    LFO,                    // 低频振荡器
    Envelope,               // 包络
    ChaosLFO,               // 混沌LFO (P1-3)
    ChaosEnvelope,           // 混沌包络 (P1-3)
    MacroControl,           // 宏控制
    StepSequencer,          // 步进音序器

    // 路由
    Mixer,                  // 混合器
    Splitter,               // 分离器
    AudioInput,             // 音频输入
    AudioOutput,            // 音频输出
};

// =============================================================================
// 端口描述符
// =============================================================================
struct PortDescriptor {
    juce::String name;
    juce::String unit;
    bool isAudio;       // true=音频端口, false=控制端口
    float defaultValue;
    float minValue;
    float maxValue;
};

// =============================================================================
// 音频端口数据
// =============================================================================
struct AudioPort {
    juce::String name;
    PortDescriptor descriptor;
    juce::AudioBuffer<float> buffer;
    bool isConnected;
};

// =============================================================================
// 音频连接
// =============================================================================
struct AudioConnection {
    ConnectionId id;
    NodeId sourceNodeId;
    int sourcePortIndex;
    NodeId destNodeId;
    int destPortIndex;
};

// =============================================================================
// AudioNode 基类 - 所有音频处理节点的抽象基类
// =============================================================================
class AudioNode {
public:
    AudioNode(NodeType type, const juce::String& name);
    virtual ~AudioNode() = default;

    // =========================================================================
    // 生命周期
    // =========================================================================
    virtual void prepareToPlay(double sampleRate, int maxSamplesPerBlock);
    virtual void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) = 0;
    virtual void releaseResources();

    // =========================================================================
    // 节点标识
    // =========================================================================
    const NodeId& getNodeId() const { return nodeId_; }
    NodeType getNodeType() const { return nodeType_; }
    const juce::String& getName() const { return name_; }
    void setName(const juce::String& name) { name_ = name; }

    // =========================================================================
    // 端口管理
    // =========================================================================
    int getNumInputPorts() const { return static_cast<int>(inputPorts_.size()); }
    int getNumOutputPorts() const { return static_cast<int>(outputPorts_.size()); }
    const PortDescriptor& getPortDescriptor(int portIndex, bool isInput) const;
    bool isPortConnected(int portIndex, bool isInput) const;
    void setPortConnected(int portIndex, bool isInput, bool connected);

    // 初始化端口 (公开以支持测试和脚本直接创建节点)
    void addInputPort(const juce::String& name, const PortDescriptor& desc);
    void addOutputPort(const juce::String& name, const PortDescriptor& desc);

    // =========================================================================
    // 输入/输出缓冲区访问
    // =========================================================================
    juce::AudioBuffer<float>& getInputBuffer(int portIndex);
    juce::AudioBuffer<float>& getOutputBuffer(int portIndex);
    const juce::AudioBuffer<float>& getOutputBuffer(int portIndex) const;

    // =========================================================================
    // 参数访问 (子类可重写以暴露特定参数)
    // =========================================================================
    virtual int getNumParameters() const { return 0; }
    virtual float getParameter(int index) const { return 0.0f; }
    virtual void setParameter(int index, float value) {}
    virtual juce::String getParameterName(int index) const { return {}; }
    virtual juce::String getParameterText(int index) const { return {}; }

    // =========================================================================
    // 性能监控
    // =========================================================================
    double getLastProcessingTimeMs() const { return lastProcessingTimeMs_; }
    size_t getMemoryUsageBytes() const { return memoryUsageBytes_; }

    // =========================================================================
    // 序列化
    // =========================================================================
    virtual juce::var toJson() const;
    virtual void fromJson(const juce::var& json);

protected:
    // NodeFactory 需要访问端口初始化方法
    friend class NodeFactory;

    // 性能追踪
    void updateProcessingTime(double ms);
    void updateMemoryUsage(size_t bytes);

    // 成员变量
    NodeId nodeId_;
    NodeType nodeType_;
    juce::String name_;

    std::vector<AudioPort> inputPorts_;
    std::vector<AudioPort> outputPorts_;

    double sampleRate_ = 44100.0;
    int maxSamplesPerBlock_ = 256;

    double lastProcessingTimeMs_ = 0.0;
    size_t memoryUsageBytes_ = 0;

    // 静态ID计数器
    static std::atomic<int64_t> s_nextId;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioNode)
};

// =============================================================================
// 节点类型名称映射
// =============================================================================
juce::String nodeTypeToString(NodeType type);
NodeType stringToNodeType(const juce::String& str);

} // namespace LianCore