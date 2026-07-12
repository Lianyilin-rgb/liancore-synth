// =============================================================================
// LianCore - ModulationMatrix 实现
// =============================================================================
#include "ModulationMatrix.h"
#include "../utils/AudioUtils.h"

namespace LianCore {

ModulationMatrix::ModulationMatrix() {
    routes_.reserve(kMaxRoutes);
}

// =============================================================================
// 调制源/目标注册
// =============================================================================
void ModulationMatrix::registerSource(const juce::String& id, ModulationSource* source) {
    sources_[id] = source;
}

void ModulationMatrix::unregisterSource(const juce::String& id) {
    sources_.erase(id);
    // 移除相关路由
    routes_.erase(std::remove_if(routes_.begin(), routes_.end(),
        [&id](const ModulationRoute& r) { return r.sourceId == id; }),
        routes_.end());
}

ModulationSource* ModulationMatrix::getSource(const juce::String& id) {
    auto it = sources_.find(id);
    return it != sources_.end() ? it->second : nullptr;
}

void ModulationMatrix::registerTarget(const juce::String& id, ModulationTarget* target) {
    targets_[id] = target;
}

void ModulationMatrix::unregisterTarget(const juce::String& id) {
    targets_.erase(id);
    routes_.erase(std::remove_if(routes_.begin(), routes_.end(),
        [&id](const ModulationRoute& r) { return r.targetId == id; }),
        routes_.end());
}

ModulationTarget* ModulationMatrix::getTarget(const juce::String& id) {
    auto it = targets_.find(id);
    return it != targets_.end() ? it->second : nullptr;
}

// =============================================================================
// 调制路由管理
// =============================================================================
int ModulationMatrix::addModulation(const juce::String& sourceId,
                                     const juce::String& targetId,
                                     float amount) {
    if (routes_.size() >= kMaxRoutes) return -1;

    auto* source = getSource(sourceId);
    auto* target = getTarget(targetId);
    if (!source || !target) return -1;

    ModulationRoute route;
    route.sourceId = sourceId;
    route.targetId = targetId;
    route.amount = AudioUtils::clamp(amount, -1.0f, 1.0f);
    route.lastValue = 0.0f;

    routes_.push_back(route);
    return static_cast<int>(routes_.size()) - 1;
}

void ModulationMatrix::removeModulation(int routeIndex) {
    if (routeIndex >= 0 && routeIndex < static_cast<int>(routes_.size())) {
        routes_.erase(routes_.begin() + routeIndex);
    }
}

void ModulationMatrix::setModulationAmount(int routeIndex, float amount) {
    if (routeIndex >= 0 && routeIndex < static_cast<int>(routes_.size())) {
        routes_[routeIndex].amount = AudioUtils::clamp(amount, -1.0f, 1.0f);
    }
}

void ModulationMatrix::clearAllRoutes() {
    routes_.clear();
}

float ModulationMatrix::getModulationAmount(int routeIndex) const {
    if (routeIndex >= 0 && routeIndex < static_cast<int>(routes_.size())) {
        return routes_[routeIndex].amount;
    }
    return 0.0f;
}

// =============================================================================
// 每采样处理
// =============================================================================
void ModulationMatrix::processBlock(int numSamples) {
    for (auto& route : routes_) {
        auto* source = getSource(route.sourceId);
        auto* target = getTarget(route.targetId);
        if (!source || !target) continue;

        float sourceValue = source->getValue();
        route.lastValue = sourceValue;

        // 将调制值映射到目标范围
        float normalizedValue = sourceValue * route.amount;
        target->applyModulation(normalizedValue);
    }
}

// =============================================================================
// 可视化快照
// =============================================================================
std::vector<ModulationMatrix::ModulationSnapshot> ModulationMatrix::getSnapshot() const {
    std::vector<ModulationSnapshot> snapshots;
    snapshots.reserve(routes_.size());

    for (const auto& route : routes_) {
        ModulationSnapshot snap;
        snap.sourceId = route.sourceId;
        snap.targetId = route.targetId;
        snap.amount = route.amount;
        snap.currentValue = route.lastValue;
        snapshots.push_back(snap);
    }

    return snapshots;
}

// =============================================================================
// 序列化
// =============================================================================
juce::var ModulationMatrix::toJson() const {
    juce::Array<juce::var> routesArray;
    for (const auto& route : routes_) {
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty("sourceId", route.sourceId);
        obj->setProperty("targetId", route.targetId);
        obj->setProperty("amount", route.amount);
        routesArray.add(obj.get());
    }
    return juce::var(routesArray);
}

void ModulationMatrix::fromJson(const juce::var& json) {
    routes_.clear();
    if (auto* arr = json.getArray()) {
        for (const auto& item : *arr) {
            if (auto* obj = item.getDynamicObject()) {
                addModulation(
                    obj->getProperty("sourceId").toString(),
                    obj->getProperty("targetId").toString(),
                    obj->getProperty("amount")
                );
            }
        }
    }
}

} // namespace LianCore