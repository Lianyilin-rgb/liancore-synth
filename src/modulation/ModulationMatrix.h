// =============================================================================
// LianCore - ModulationMatrix 调制矩阵 (矩阵路由表方案)
// 支持32个调制槽位，每个槽位：源→目标→量
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <unordered_map>
#include <memory>

namespace LianCore {

// =============================================================================
// 调制源接口
// =============================================================================
class ModulationSource {
public:
    virtual ~ModulationSource() = default;
    virtual float getValue() const = 0;
    virtual juce::String getName() const = 0;
    virtual juce::String getUnit() const { return {}; }
    virtual bool isBipolar() const { return false; }
};

// =============================================================================
// 调制目标接口
// =============================================================================
class ModulationTarget {
public:
    virtual ~ModulationTarget() = default;
    virtual void applyModulation(float normalizedValue) = 0;
    virtual juce::String getName() const = 0;
    virtual float getCurrentValue() const = 0;
    virtual float getMinValue() const = 0;
    virtual float getMaxValue() const = 0;
};

// =============================================================================
// 调制矩阵
// =============================================================================
class ModulationMatrix {
public:
    static constexpr int kMaxRoutes = 32;

    ModulationMatrix();
    ~ModulationMatrix() = default;

    // =========================================================================
    // 调制源/目标注册
    // =========================================================================
    void registerSource(const juce::String& id, ModulationSource* source);
    void unregisterSource(const juce::String& id);
    ModulationSource* getSource(const juce::String& id);

    void registerTarget(const juce::String& id, ModulationTarget* target);
    void unregisterTarget(const juce::String& id);
    ModulationTarget* getTarget(const juce::String& id);

    // =========================================================================
    // 调制路由管理
    // =========================================================================
    int addModulation(const juce::String& sourceId, const juce::String& targetId, float amount);
    void removeModulation(int routeIndex);
    void setModulationAmount(int routeIndex, float amount);
    void clearAllRoutes();

    int getNumRoutes() const { return static_cast<int>(routes_.size()); }
    float getModulationAmount(int routeIndex) const;

    // =========================================================================
    // 每采样处理
    // =========================================================================
    void processBlock(int numSamples);

    // =========================================================================
    // 可视化快照
    // =========================================================================
    struct ModulationSnapshot {
        juce::String sourceId;
        juce::String targetId;
        float amount;
        float currentValue;
    };
    std::vector<ModulationSnapshot> getSnapshot() const;

    // =========================================================================
    // 序列化
    // =========================================================================
    juce::var toJson() const;
    void fromJson(const juce::var& json);

private:
    struct ModulationRoute {
        juce::String sourceId;
        juce::String targetId;
        float amount;
        float lastValue;
    };

    std::vector<ModulationRoute> routes_;
    std::unordered_map<juce::String, ModulationSource*> sources_;
    std::unordered_map<juce::String, ModulationTarget*> targets_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulationMatrix)
};

} // namespace LianCore