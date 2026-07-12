// =============================================================================
// LianCore - NodeFactory 节点工厂
// 根据NodeType创建对应的音频节点实例
// =============================================================================
#pragma once

#include "AudioNode.h"
#include <memory>

namespace LianCore {

// =============================================================================
// NodeFactory - 节点工厂类
// =============================================================================
class NodeFactory {
public:
    // 创建指定类型的节点
    static std::unique_ptr<AudioNode> createNode(NodeType type, const juce::String& name = {});

    // 获取节点类型的默认名称
    static juce::String getDefaultName(NodeType type);

    // 获取节点类型的默认端口配置
    static void configureDefaultPorts(AudioNode* node);

private:
    NodeFactory() = delete;
};

} // namespace LianCore