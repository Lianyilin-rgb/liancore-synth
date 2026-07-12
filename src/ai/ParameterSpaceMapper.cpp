// =============================================================================
// LianCore - ParameterSpaceMapper 实现
// =============================================================================
#include "ParameterSpaceMapper.h"
#include "../params/ParameterTree.h"

namespace LianCore {

ParameterSpaceMapper::ParameterSpaceMapper() = default;

std::vector<ParameterMapping> ParameterSpaceMapper::mapEmbeddingToParameters(
    const std::vector<float>& embedding, float confidence) {

    std::vector<ParameterMapping> mappings;
    if (embedding.empty()) return mappings;

    // Alpha阶段: 简单映射策略
    // 将嵌入向量的前几个维度映射到关键参数
    static const std::vector<juce::String> parameterIds = {
        "filter_cutoff",
        "filter_resonance",
        "osc_waveform",
        "env_attack",
        "env_release",
        "lfo_rate",
        "lfo_depth",
    };

    for (size_t i = 0; i < parameterIds.size() && i < embedding.size(); ++i) {
        ParameterMapping mapping;
        mapping.parameterId = parameterIds[i];
        mapping.value = embedding[i];
        mapping.explanation = juce::String::formatted(
            "AI嵌入向量维度%zu映射到参数", i + 1);
        mappings.push_back(mapping);
    }

    return mappings;
}

} // namespace LianCore