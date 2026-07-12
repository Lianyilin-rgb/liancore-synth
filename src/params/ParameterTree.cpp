// =============================================================================
// LianCore - ParameterTree 实现
// =============================================================================
#include "ParameterTree.h"

namespace LianCore {

LianCoreParameterTree::LianCoreParameterTree(juce::AudioProcessor& processor)
    : state_(processor, &undoManager_, "LianCoreParams", {}) {
}

LianCoreParameterTree::~LianCoreParameterTree() = default;

// =============================================================================
// 参数注册
// =============================================================================
void LianCoreParameterTree::registerParameter(const juce::String& id,
                                               const juce::String& name,
                                               float defaultValue,
                                               float minValue,
                                               float maxValue,
                                               float step,
                                               const juce::String& unit,
                                               const juce::String& description,
                                               const juce::String& category) {
    // 创建JUCE参数
    auto* param = new juce::AudioParameterFloat(
        juce::ParameterID(id, 1),
        name,
        juce::NormalisableRange<float>(minValue, maxValue, step),
        defaultValue
    );

    state_.createAndAddParameter(std::unique_ptr<juce::AudioParameterFloat>(param));

    // 存储元数据
    ParameterMeta meta;
    meta.id = id;
    meta.name = name;
    meta.description = description;
    meta.category = category;
    meta.unit = unit;
    meta.defaultValue = defaultValue;
    meta.minValue = minValue;
    meta.maxValue = maxValue;
    parameterMeta_.push_back(meta);
}

void LianCoreParameterTree::beginGroup(const juce::String& groupId, const juce::String& groupName) {
    // JUCE参数树不支持分组，这里做标记
    juce::ignoreUnused(groupId, groupName);
}

void LianCoreParameterTree::endGroup() {
    // 分组结束标记
}

// =============================================================================
// 参数访问
// =============================================================================
float LianCoreParameterTree::getParameterValue(const juce::String& id) const {
    auto* param = state_.getParameter(id);
    return param ? param->getValue() : 0.0f;
}

void LianCoreParameterTree::setParameterValue(const juce::String& id, float value) {
    auto* param = state_.getParameter(id);
    if (param) {
        param->setValueNotifyingHost(value);
        listeners_.call([&](ParameterTreeListener& l) {
            l.onParameterChanged(id, value);
        });
    }
}

juce::RangedAudioParameter* LianCoreParameterTree::getParameter(const juce::String& id) {
    return state_.getParameter(id);
}

std::vector<juce::String> LianCoreParameterTree::getAllParameterIds() const {
    std::vector<juce::String> ids;
    for (const auto& meta : parameterMeta_) {
        ids.push_back(meta.id);
    }
    return ids;
}

// =============================================================================
// 批量更新
// =============================================================================
void LianCoreParameterTree::applyParameterBatch(const std::vector<ParameterMapping>& mappings) {
    pushUndoState();

    for (const auto& mapping : mappings) {
        auto* param = state_.getParameter(mapping.parameterId);
        if (param) {
            param->setValueNotifyingHost(mapping.value);
        }
    }

    listeners_.call([](ParameterTreeListener& l) {
        l.onParameterBatchApplied();
    });
}

// =============================================================================
// 撤销/重做
// =============================================================================
void LianCoreParameterTree::pushUndoState() {
    undoManager_.beginNewTransaction();
}

bool LianCoreParameterTree::undo() {
    return undoManager_.undo();
}

bool LianCoreParameterTree::redo() {
    return undoManager_.redo();
}

int LianCoreParameterTree::getUndoDepth() const {
    return undoManager_.getNumActionsInCurrentTransaction();
}

int LianCoreParameterTree::getRedoDepth() const {
    return undoManager_.getNumberOfUnitsTaken();
}

// =============================================================================
// 预设管理
// =============================================================================
juce::var LianCoreParameterTree::getPresetAsJson() const {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();

    for (const auto& meta : parameterMeta_) {
        obj->setProperty(meta.id, getParameterValue(meta.id));
    }

    return juce::var(obj.get());
}

void LianCoreParameterTree::restorePresetFromJson(const juce::var& json) {
    if (auto* obj = json.getDynamicObject()) {
        pushUndoState();

        for (const auto& prop : obj->getProperties()) {
            setParameterValue(prop.name.toString(), prop.value);
        }
    }
}

// =============================================================================
// 监听器
// =============================================================================
void LianCoreParameterTree::addListener(ParameterTreeListener* listener) {
    listeners_.add(listener);
}

void LianCoreParameterTree::removeListener(ParameterTreeListener* listener) {
    listeners_.remove(listener);
}

} // namespace LianCore