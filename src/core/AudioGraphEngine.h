// =============================================================================
// LianCore - AudioGraphEngine 音频图引擎
// 统一节点式音频图架构的核心管理器
// 负责节点生命周期、连接管理、拓扑排序、信号处理调度
// =============================================================================
#pragma once

#include "AudioNode.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>

namespace LianCore {

// =============================================================================
// 图变更回调类型
// =============================================================================
using GraphChangeCallback = std::function<void()>;

// =============================================================================
// AudioGraphEngine - 音频图管理器
// =============================================================================
class AudioGraphEngine {
public:
    AudioGraphEngine();
    ~AudioGraphEngine();

    // =========================================================================
    // 节点管理
    // =========================================================================
    NodeId addNode(NodeType type, const juce::String& name = {});
    void removeNode(NodeId id);
    AudioNode* getNode(NodeId id);
    const AudioNode* getNode(NodeId id) const;
    bool hasNode(NodeId id) const;
    int getNodeCount() const { return static_cast<int>(nodes_.size()); }

    // 遍历所有节点
    void forEachNode(std::function<void(AudioNode*)> callback);
    std::vector<NodeId> getAllNodeIds() const;

    // =========================================================================
    // 连接管理
    // =========================================================================
    ConnectionId connect(NodeId srcNodeId, int srcPortIndex,
                         NodeId dstNodeId, int dstPortIndex);
    void disconnect(ConnectionId id);
    void disconnectAll();
    const std::vector<AudioConnection>& getConnections() const { return connections_; }

    // =========================================================================
    // 音频处理
    // =========================================================================
    void prepareToPlay(double sampleRate, int maxSamplesPerBlock);
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);
    void releaseResources();

    // =========================================================================
    // 序列化
    // =========================================================================
    juce::var toJson() const;
    void fromJson(const juce::var& json);

    // =========================================================================
    // 性能监控
    // =========================================================================
    double getCurrentCpuUsage() const;
    double getAverageCpuUsage() const;
    size_t getTotalMemoryUsage() const;

    // =========================================================================
    // 变更通知
    // =========================================================================
    void addChangeListener(GraphChangeCallback callback);
    void removeChangeListeners();

private:
    // =========================================================================
    // 内部方法
    // =========================================================================
    void rebuildTopologicalOrder();
    void notifyGraphChanged();
    void routeAudioBetweenNodes();

    // =========================================================================
    // 成员变量
    // =========================================================================
    std::unordered_map<NodeId, std::unique_ptr<AudioNode>> nodes_;
    std::vector<AudioConnection> connections_;
    std::vector<AudioNode*> processingOrder_; // 拓扑排序结果

    // 性能追踪
    double currentCpuUsage_ = 0.0;
    double averageCpuUsage_ = 0.0;
    int cpuUsageSampleCount_ = 0;
    juce::Time lastProcessTime_;

    // 变更回调
    std::vector<GraphChangeCallback> changeListeners_;

    // 处理状态
    bool isPrepared_ = false;
    double sampleRate_ = 44100.0;
    int maxSamplesPerBlock_ = 256;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioGraphEngine)
};

} // namespace LianCore