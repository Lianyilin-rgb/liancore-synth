// =============================================================================
// LianCore - AudioGraphEngine 实现
// =============================================================================
#include "AudioGraphEngine.h"
#include "NodeFactory.h"

namespace LianCore {

// =============================================================================
// 构造/析构
// =============================================================================
AudioGraphEngine::AudioGraphEngine() {
}

AudioGraphEngine::~AudioGraphEngine() {
    releaseResources();
}

// =============================================================================
// 节点管理
// =============================================================================
NodeId AudioGraphEngine::addNode(NodeType type, const juce::String& name) {
    auto node = NodeFactory::createNode(type, name);
    NodeId id = node->getNodeId();

    if (isPrepared_) {
        node->prepareToPlay(sampleRate_, maxSamplesPerBlock_);
    }

    nodes_[id] = std::move(node);
    notifyGraphChanged();
    return id;
}

void AudioGraphEngine::removeNode(NodeId id) {
    // 先移除所有与该节点相关的连接
    connections_.erase(
        std::remove_if(connections_.begin(), connections_.end(),
            [&id](const AudioConnection& conn) {
                return conn.sourceNodeId == id || conn.destNodeId == id;
            }),
        connections_.end()
    );

    nodes_.erase(id);
    rebuildTopologicalOrder();
    notifyGraphChanged();
}

AudioNode* AudioGraphEngine::getNode(NodeId id) {
    auto it = nodes_.find(id);
    return it != nodes_.end() ? it->second.get() : nullptr;
}

const AudioNode* AudioGraphEngine::getNode(NodeId id) const {
    auto it = nodes_.find(id);
    return it != nodes_.end() ? it->second.get() : nullptr;
}

bool AudioGraphEngine::hasNode(NodeId id) const {
    return nodes_.find(id) != nodes_.end();
}

void AudioGraphEngine::forEachNode(std::function<void(AudioNode*)> callback) {
    for (auto& pair : nodes_) {
        callback(pair.second.get());
    }
}

std::vector<NodeId> AudioGraphEngine::getAllNodeIds() const {
    std::vector<NodeId> ids;
    ids.reserve(nodes_.size());
    for (const auto& pair : nodes_) {
        ids.push_back(pair.first);
    }
    return ids;
}

// =============================================================================
// 连接管理
// =============================================================================
ConnectionId AudioGraphEngine::connect(NodeId srcNodeId, int srcPortIndex,
                                        NodeId dstNodeId, int dstPortIndex) {
    auto* srcNode = getNode(srcNodeId);
    auto* dstNode = getNode(dstNodeId);

    jassert(srcNode != nullptr);
    jassert(dstNode != nullptr);
    jassert(srcPortIndex < srcNode->getNumOutputPorts());
    jassert(dstPortIndex < dstNode->getNumInputPorts());

    // 生成连接ID
    ConnectionId id = srcNodeId + "_" + juce::String(srcPortIndex)
                    + "_to_" + dstNodeId + "_" + juce::String(dstPortIndex);

    // 检查是否已存在相同连接
    auto it = std::find_if(connections_.begin(), connections_.end(),
        [&id](const AudioConnection& conn) { return conn.id == id; });
    if (it != connections_.end()) {
        return id; // 连接已存在
    }

    AudioConnection conn;
    conn.id = id;
    conn.sourceNodeId = srcNodeId;
    conn.sourcePortIndex = srcPortIndex;
    conn.destNodeId = dstNodeId;
    conn.destPortIndex = dstPortIndex;

    connections_.push_back(conn);

    // 标记端口为已连接
    srcNode->setPortConnected(srcPortIndex, false, true);
    dstNode->setPortConnected(dstPortIndex, true, true);

    rebuildTopologicalOrder();
    notifyGraphChanged();
    return id;
}

void AudioGraphEngine::disconnect(ConnectionId id) {
    auto it = std::find_if(connections_.begin(), connections_.end(),
        [&id](const AudioConnection& conn) { return conn.id == id; });

    if (it != connections_.end()) {
        // 取消端口连接标记
        if (auto* srcNode = getNode(it->sourceNodeId)) {
            srcNode->setPortConnected(it->sourcePortIndex, false, false);
        }
        if (auto* dstNode = getNode(it->destNodeId)) {
            dstNode->setPortConnected(it->destPortIndex, true, false);
        }

        connections_.erase(it);
        rebuildTopologicalOrder();
        notifyGraphChanged();
    }
}

void AudioGraphEngine::disconnectAll() {
    for (const auto& conn : connections_) {
        if (auto* srcNode = getNode(conn.sourceNodeId)) {
            srcNode->setPortConnected(conn.sourcePortIndex, false, false);
        }
        if (auto* dstNode = getNode(conn.destNodeId)) {
            dstNode->setPortConnected(conn.destPortIndex, true, false);
        }
    }
    connections_.clear();
    processingOrder_.clear();
    notifyGraphChanged();
}

// =============================================================================
// 音频处理
// =============================================================================
void AudioGraphEngine::prepareToPlay(double sampleRate, int maxSamplesPerBlock) {
    sampleRate_ = sampleRate;
    maxSamplesPerBlock_ = maxSamplesPerBlock;

    for (auto& pair : nodes_) {
        pair.second->prepareToPlay(sampleRate, maxSamplesPerBlock);
    }

    isPrepared_ = true;
}

void AudioGraphEngine::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    if (!isPrepared_ || processingOrder_.empty()) {
        return;
    }

    auto startTime = juce::Time::getMillisecondCounterHiRes();

    // 按拓扑排序顺序处理每个节点
    for (auto* node : processingOrder_) {
        node->processBlock(buffer, midi);
    }

    // 在节点间路由音频数据
    routeAudioBetweenNodes();

    // 更新CPU使用率
    auto elapsed = juce::Time::getMillisecondCounterHiRes() - startTime;
    currentCpuUsage_ = elapsed;

    // 移动平均
    constexpr double alpha = 0.01;
    averageCpuUsage_ = averageCpuUsage_ * (1.0 - alpha) + elapsed * alpha;
    cpuUsageSampleCount_++;
}

void AudioGraphEngine::releaseResources() {
    for (auto& pair : nodes_) {
        pair.second->releaseResources();
    }
    isPrepared_ = false;
}

// =============================================================================
// 拓扑排序 - Kahn算法实现
// =============================================================================
void AudioGraphEngine::rebuildTopologicalOrder() {
    processingOrder_.clear();

    if (nodes_.empty()) return;

    // 构建邻接表和入度
    std::unordered_map<NodeId, int> inDegree;
    std::unordered_map<NodeId, std::vector<NodeId>> adjacencyList;

    for (const auto& pair : nodes_) {
        inDegree[pair.first] = 0;
        adjacencyList[pair.first] = {};
    }

    for (const auto& conn : connections_) {
        adjacencyList[conn.sourceNodeId].push_back(conn.destNodeId);
        inDegree[conn.destNodeId]++;
    }

    // 初始化队列(入度为0的节点)
    std::queue<NodeId> queue;
    for (const auto& pair : inDegree) {
        if (pair.second == 0) {
            queue.push(pair.first);
        }
    }

    // Kahn算法
    while (!queue.empty()) {
        NodeId current = queue.front();
        queue.pop();
        processingOrder_.push_back(getNode(current));

        for (const auto& neighbor : adjacencyList[current]) {
            inDegree[neighbor]--;
            if (inDegree[neighbor] == 0) {
                queue.push(neighbor);
            }
        }
    }

    // 检测环路: 如果排序结果数量不等于节点数, 存在环路
    if (processingOrder_.size() != nodes_.size()) {
        // 环路检测: 将所有未处理的节点加入（它们形成环）
        // 生产环境中应记录警告
        jassertfalse; // 音频图中不应存在环路!
    }
}

// =============================================================================
// 节点间音频路由
// =============================================================================
void AudioGraphEngine::routeAudioBetweenNodes() {
    for (const auto& conn : connections_) {
        auto* srcNode = getNode(conn.sourceNodeId);
        auto* dstNode = getNode(conn.destNodeId);

        if (!srcNode || !dstNode) continue;

        // 将源节点的输出缓冲区复制到目标节点的输入缓冲区
        const auto& srcBuffer = srcNode->getOutputBuffer(conn.sourcePortIndex);
        auto& dstBuffer = dstNode->getInputBuffer(conn.destPortIndex);

        // 快速缓冲区复制
        for (int ch = 0; ch < srcBuffer.getNumChannels(); ++ch) {
            dstBuffer.copyFrom(ch, 0, srcBuffer, ch, 0, srcBuffer.getNumSamples());
        }
    }
}

// =============================================================================
// 序列化
// =============================================================================
juce::var AudioGraphEngine::toJson() const {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();

    // 序列化节点
    juce::Array<juce::var> nodesArray;
    for (const auto& pair : nodes_) {
        nodesArray.add(pair.second->toJson());
    }
    obj->setProperty("nodes", nodesArray);

    // 序列化连接
    juce::Array<juce::var> connectionsArray;
    for (const auto& conn : connections_) {
        juce::DynamicObject::Ptr connObj = new juce::DynamicObject();
        connObj->setProperty("id", conn.id);
        connObj->setProperty("sourceNodeId", conn.sourceNodeId);
        connObj->setProperty("sourcePortIndex", conn.sourcePortIndex);
        connObj->setProperty("destNodeId", conn.destNodeId);
        connObj->setProperty("destPortIndex", conn.destPortIndex);
        connectionsArray.add(connObj.get());
    }
    obj->setProperty("connections", connectionsArray);

    return juce::var(obj.get());
}

void AudioGraphEngine::fromJson(const juce::var& json) {
    if (auto* obj = json.getDynamicObject()) {
        // 清空现有状态
        disconnectAll();
        nodes_.clear();

        // 反序列化节点
        if (auto* nodesArray = obj->getProperty("nodes").getArray()) {
            for (const auto& nodeVar : *nodesArray) {
                if (auto* nodeObj = nodeVar.getDynamicObject()) {
                    auto type = stringToNodeType(nodeObj->getProperty("nodeType").toString());
                    auto name = nodeObj->getProperty("name").toString();
                    auto nodeId = addNode(type, name);
                    if (auto* node = getNode(nodeId)) {
                        node->fromJson(nodeVar);
                    }
                }
            }
        }

        // 反序列化连接
        if (auto* connectionsArray = obj->getProperty("connections").getArray()) {
            for (const auto& connVar : *connectionsArray) {
                if (auto* connObj = connVar.getDynamicObject()) {
                    connect(
                        connObj->getProperty("sourceNodeId").toString(),
                        connObj->getProperty("sourcePortIndex"),
                        connObj->getProperty("destNodeId").toString(),
                        connObj->getProperty("destPortIndex")
                    );
                }
            }
        }
    }
}

// =============================================================================
// 性能监控
// =============================================================================
double AudioGraphEngine::getCurrentCpuUsage() const {
    return currentCpuUsage_;
}

double AudioGraphEngine::getAverageCpuUsage() const {
    return averageCpuUsage_;
}

size_t AudioGraphEngine::getTotalMemoryUsage() const {
    size_t total = 0;
    for (const auto& pair : nodes_) {
        total += pair.second->getMemoryUsageBytes();
    }
    return total;
}

// =============================================================================
// 变更通知
// =============================================================================
void AudioGraphEngine::addChangeListener(GraphChangeCallback callback) {
    changeListeners_.push_back(std::move(callback));
}

void AudioGraphEngine::removeChangeListeners() {
    changeListeners_.clear();
}

void AudioGraphEngine::notifyGraphChanged() {
    for (auto& listener : changeListeners_) {
        if (listener) listener();
    }
}

} // namespace LianCore