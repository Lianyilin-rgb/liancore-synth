// =============================================================================
// LianCore - ParameterSpaceMapper 参数空间映射器 (Alpha阶段: 占位)
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <string>

namespace LianCore {

struct ParameterMapping;

class ParameterSpaceMapper {
public:
    ParameterSpaceMapper();
    ~ParameterSpaceMapper() = default;

    // 将AI嵌入向量映射到参数空间
    std::vector<ParameterMapping> mapEmbeddingToParameters(
        const std::vector<float>& embedding,
        float confidence = 0.5f
    );

    // 获取参数空间的维度
    int getParameterSpaceDimension() const { return 64; }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterSpaceMapper)
};

} // namespace LianCore