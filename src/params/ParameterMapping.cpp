// =============================================================================
// LianCore - ParameterMapping 参数映射系统 实现
// =============================================================================

#include "ParameterMapping.h"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace LianCore {

// =============================================================================
// 工具函数
// =============================================================================

// 限制值在范围内
static float clampValue(float value, float minVal, float maxVal) {
    return std::max(minVal, std::min(maxVal, value));
}

// 线性插值
static float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// =============================================================================
// AutomationBreakpoint 序列化
// =============================================================================

juce::var AutomationBreakpoint::toJson() const {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("time", time);
    obj->setProperty("value", value);

    // 曲线类型转字符串
    juce::String curveStr;
    switch (curveType) {
        case AutomationCurveType::Linear:      curveStr = "Linear";      break;
        case AutomationCurveType::Smooth:      curveStr = "Smooth";      break;
        case AutomationCurveType::Exponential: curveStr = "Exponential"; break;
        case AutomationCurveType::Hold:        curveStr = "Hold";        break;
    }
    obj->setProperty("curveType", curveStr);

    return juce::var(obj.get());
}

AutomationBreakpoint AutomationBreakpoint::fromJson(const juce::var& json) {
    AutomationBreakpoint bp;
    if (json.isObject()) {
        bp.time = json["time"];
        bp.value = json["value"];

        juce::String curveStr = json["curveType"].toString();
        if (curveStr == "Smooth")       bp.curveType = AutomationCurveType::Smooth;
        else if (curveStr == "Exponential") bp.curveType = AutomationCurveType::Exponential;
        else if (curveStr == "Hold")        bp.curveType = AutomationCurveType::Hold;
        else                                bp.curveType = AutomationCurveType::Linear;
    }
    return bp;
}

// =============================================================================
// ParameterSnapshot 实现
// =============================================================================

juce::var ParameterSnapshot::toJson() const {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("name", juce::String(name));
    obj->setProperty("description", juce::String(description));
    obj->setProperty("timestamp", timestamp.toISO8601(true));

    // 参数列表
    juce::Array<juce::var> paramsArray;
    for (const auto& [paramName, paramValue] : parameters) {
        juce::DynamicObject::Ptr paramObj = new juce::DynamicObject();
        paramObj->setProperty("name", juce::String(paramName));
        paramObj->setProperty("value", paramValue);
        paramsArray.add(juce::var(paramObj.get()));
    }
    obj->setProperty("parameters", juce::var(paramsArray));

    return juce::var(obj.get());
}

ParameterSnapshot ParameterSnapshot::fromJson(const juce::var& json) {
    ParameterSnapshot snapshot;
    if (!json.isObject()) return snapshot;

    snapshot.name = json["name"].toString().toStdString();
    snapshot.description = json["description"].toString().toStdString();
    snapshot.timestamp = juce::Time::fromISO8601(json["timestamp"].toString());

    if (auto* paramsArray = json["parameters"].getArray()) {
        for (const auto& item : *paramsArray) {
            if (item.isObject()) {
                std::string paramName = item["name"].toString().toStdString();
                float paramValue = item["value"];
                snapshot.parameters.emplace_back(paramName, paramValue);
            }
        }
    }

    return snapshot;
}

ParameterSnapshot ParameterSnapshot::morphTo(const ParameterSnapshot& target, float progress) const {
    progress = clampValue(progress, 0.0f, 1.0f);

    ParameterSnapshot result;
    result.name = name + " -> " + target.name;
    result.description = "Morph between snapshots at " + std::to_string(progress);
    result.timestamp = juce::Time::getCurrentTime();

    // 构建目标参数查找表 (name -> value)
    std::unordered_map<std::string, float> targetMap;
    for (const auto& [name, value] : target.parameters) {
        targetMap[name] = value;
    }

    // 遍历源参数，进行插值
    std::unordered_map<std::string, bool> processed;
    for (const auto& [paramName, srcValue] : parameters) {
        auto it = targetMap.find(paramName);
        if (it != targetMap.end()) {
            // 两边都有 -> 线性插值
            float morphedValue = lerp(srcValue, it->second, progress);
            result.parameters.emplace_back(paramName, morphedValue);
            processed[paramName] = true;
        } else {
            // 只在源中存在 -> 保留源值
            result.parameters.emplace_back(paramName, srcValue);
            processed[paramName] = true;
        }
    }

    // 只在目标中存在的参数 -> 逐步引入
    for (const auto& [paramName, tgtValue] : target.parameters) {
        if (processed.find(paramName) == processed.end()) {
            // 参数仅在目标中存在，使用progress控制引入量
            // progress=0时不引入（保持未定义），progress>0时逐步引入
            if (progress > 0.0f) {
                result.parameters.emplace_back(paramName, tgtValue);
            }
        }
    }

    return result;
}

// =============================================================================
// ParameterAutomation 实现
// =============================================================================

void ParameterAutomation::addBreakpoint(const std::string& paramName, double time,
                                         float value, AutomationCurveType curveType) {
    AutomationBreakpoint bp;
    bp.time = time;
    bp.value = value;
    bp.curveType = curveType;

    auto& list = automationData_[paramName];
    list.push_back(bp);

    // 按时间排序
    std::sort(list.begin(), list.end(),
              [](const AutomationBreakpoint& a, const AutomationBreakpoint& b) {
                  return a.time < b.time;
              });
}

void ParameterAutomation::removeBreakpoint(const std::string& paramName, double time) {
    auto it = automationData_.find(paramName);
    if (it == automationData_.end()) return;

    auto& list = it->second;
    list.erase(std::remove_if(list.begin(), list.end(),
                              [time](const AutomationBreakpoint& bp) {
                                  return std::abs(bp.time - time) < 0.000001;
                              }),
               list.end());

    // 如果参数没有断点了，清除该条目
    if (list.empty()) {
        automationData_.erase(it);
    }
}

void ParameterAutomation::clearParameter(const std::string& paramName) {
    automationData_.erase(paramName);
}

void ParameterAutomation::clearAll() {
    automationData_.clear();
}

const std::vector<AutomationBreakpoint>* ParameterAutomation::getBreakpoints(
    const std::string& paramName) const {
    auto it = automationData_.find(paramName);
    if (it != automationData_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<std::string> ParameterAutomation::getAutomatedParameters() const {
    std::vector<std::string> result;
    result.reserve(automationData_.size());
    for (const auto& [name, _] : automationData_) {
        result.push_back(name);
    }
    return result;
}

int ParameterAutomation::getTotalBreakpointCount() const {
    int count = 0;
    for (const auto& [_, list] : automationData_) {
        count += static_cast<int>(list.size());
    }
    return count;
}

bool ParameterAutomation::hasAutomation(const std::string& paramName) const {
    auto it = automationData_.find(paramName);
    return it != automationData_.end() && !it->second.empty();
}

float ParameterAutomation::evaluate(const std::string& paramName, double time) const {
    auto it = automationData_.find(paramName);
    if (it == automationData_.end() || it->second.empty()) {
        return 0.0f;
    }

    const auto& breakpoints = it->second;

    // 时间在最左侧断点之前
    if (time <= breakpoints.front().time) {
        return breakpoints.front().value;
    }

    // 时间在最右侧断点之后
    if (time >= breakpoints.back().time) {
        return breakpoints.back().value;
    }

    // 在两点之间查找
    for (size_t i = 0; i < breakpoints.size() - 1; ++i) {
        const auto& bp0 = breakpoints[i];
        const auto& bp1 = breakpoints[i + 1];

        if (time >= bp0.time && time <= bp1.time) {
            // 计算归一化时间 (0~1)
            double duration = bp1.time - bp0.time;
            float t = (duration > 0.0) ? static_cast<float>((time - bp0.time) / duration) : 0.0f;
            t = clampValue(t, 0.0f, 1.0f);

            // 根据 bp0 的曲线类型进行插值
            switch (bp0.curveType) {
                case AutomationCurveType::Linear:
                    return interpolateLinear(bp0.value, bp1.value, t);

                case AutomationCurveType::Smooth:
                    return interpolateSmooth(bp0.value, bp1.value, t);

                case AutomationCurveType::Exponential: {
                    // 指数曲线: 确保值不为0时才能安全使用指数
                    if (bp0.value <= 0.0f && bp1.value <= 0.0f) {
                        return interpolateLinear(bp0.value, bp1.value, t);
                    }
                    return interpolateExponential(bp0.value, bp1.value, t);
                }

                case AutomationCurveType::Hold:
                    return bp0.value; // 保持前一个值，不插值
            }
        }
    }

    return 0.0f;
}

// 静态插值函数
float ParameterAutomation::interpolateLinear(float v0, float v1, float t) {
    return lerp(v0, v1, t);
}

float ParameterAutomation::interpolateSmooth(float v0, float v1, float t) {
    // 余弦平滑插值: (1 - cos(t*PI)) / 2
    float smoothT = (1.0f - std::cos(static_cast<float>(t * juce::MathConstants<double>::pi))) * 0.5f;
    return lerp(v0, v1, smoothT);
}

float ParameterAutomation::interpolateExponential(float v0, float v1, float t) {
    // 指数插值: 使用对数/指数映射
    // 确保值为正 (音频参数通常为正值或可偏移)
    const float epsilon = 0.0001f;
    float safeV0 = (v0 <= 0.0f) ? epsilon : v0;
    float safeV1 = (v1 <= 0.0f) ? epsilon : v1;

    float logV0 = std::log(safeV0);
    float logV1 = std::log(safeV1);
    float result = std::exp(lerp(logV0, logV1, t));

    // 如果原始值有0或负值，做线性混合
    if (v0 <= 0.0f || v1 <= 0.0f) {
        float linearResult = lerp(v0, v1, t);
        return lerp(linearResult, result, 0.5f);
    }

    return result;
}

// =============================================================================
// ParameterAutomation 序列化
// =============================================================================

juce::var ParameterAutomation::toJson() const {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();

    juce::Array<juce::var> paramsArray;
    for (const auto& [paramName, breakpoints] : automationData_) {
        juce::DynamicObject::Ptr paramObj = new juce::DynamicObject();
        paramObj->setProperty("parameterName", juce::String(paramName));

        juce::Array<juce::var> bpArray;
        for (const auto& bp : breakpoints) {
            bpArray.add(bp.toJson());
        }
        paramObj->setProperty("breakpoints", juce::var(bpArray));
        paramsArray.add(juce::var(paramObj.get()));
    }
    obj->setProperty("automationData", juce::var(paramsArray));

    return juce::var(obj.get());
}

void ParameterAutomation::fromJson(const juce::var& json) {
    clearAll();
    if (!json.isObject()) return;

    if (auto* paramsArray = json["automationData"].getArray()) {
        for (const auto& item : *paramsArray) {
            if (!item.isObject()) continue;

            std::string paramName = item["parameterName"].toString().toStdString();

            if (auto* bpArray = item["breakpoints"].getArray()) {
                for (const auto& bpItem : *bpArray) {
                    AutomationBreakpoint bp = AutomationBreakpoint::fromJson(bpItem);
                    // 直接插入，最后统一排序
                    automationData_[paramName].push_back(bp);
                }

                // 排序
                auto& list = automationData_[paramName];
                std::sort(list.begin(), list.end(),
                          [](const AutomationBreakpoint& a, const AutomationBreakpoint& b) {
                              return a.time < b.time;
                          });
            }
        }
    }
}

// =============================================================================
// MacroControl::MacroMapping 序列化
// =============================================================================

juce::var MacroControl::MacroMapping::toJson() const {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("parameterId", juce::String(parameterId));
    obj->setProperty("minValue", minValue);
    obj->setProperty("maxValue", maxValue);
    obj->setProperty("curve", curve);
    return juce::var(obj.get());
}

MacroControl::MacroMapping MacroControl::MacroMapping::fromJson(const juce::var& json) {
    MacroMapping mapping;
    if (json.isObject()) {
        mapping.parameterId = json["parameterId"].toString().toStdString();
        mapping.minValue = json["minValue"];
        mapping.maxValue = json["maxValue"];
        mapping.curve = json["curve"];
    }
    return mapping;
}

// =============================================================================
// MacroControl 实现
// =============================================================================

void MacroControl::setParameterSetter(ParameterSetter setter) {
    parameterSetter_ = std::move(setter);
}

void MacroControl::addMapping(int macroIndex, const std::string& paramId,
                               float minValue, float maxValue, float curve) {
    if (macroIndex < 0 || macroIndex >= kNumMacros) return;

    // 检查是否已存在相同参数ID的映射，若存在则更新
    auto& mappings = macroMappings_[macroIndex];
    for (auto& m : mappings) {
        if (m.parameterId == paramId) {
            m.minValue = minValue;
            m.maxValue = maxValue;
            m.curve = curve;
            return;
        }
    }

    // 添加新映射
    MacroMapping mapping;
    mapping.parameterId = paramId;
    mapping.minValue = minValue;
    mapping.maxValue = maxValue;
    mapping.curve = curve;
    mappings.push_back(mapping);
}

void MacroControl::removeMapping(int macroIndex, const std::string& paramId) {
    if (macroIndex < 0 || macroIndex >= kNumMacros) return;

    auto& mappings = macroMappings_[macroIndex];
    mappings.erase(std::remove_if(mappings.begin(), mappings.end(),
                                  [&paramId](const MacroMapping& m) {
                                      return m.parameterId == paramId;
                                  }),
                   mappings.end());
}

void MacroControl::clearMacro(int macroIndex) {
    if (macroIndex < 0 || macroIndex >= kNumMacros) return;
    macroMappings_[macroIndex].clear();
}

void MacroControl::clearAll() {
    for (auto& mappings : macroMappings_) {
        mappings.clear();
    }
}

const std::vector<MacroControl::MacroMapping>* MacroControl::getMappings(int macroIndex) const {
    if (macroIndex < 0 || macroIndex >= kNumMacros) return nullptr;
    return &macroMappings_[macroIndex];
}

void MacroControl::setMacroValue(int macroIndex, float normalizedValue) {
    if (macroIndex < 0 || macroIndex >= kNumMacros) return;

    normalizedValue = clampValue(normalizedValue, 0.0f, 1.0f);
    macroValues_[macroIndex] = normalizedValue;

    // 触发所有目标参数更新
    if (parameterSetter_) {
        for (const auto& mapping : macroMappings_[macroIndex]) {
            // 应用曲线到归一化宏值
            float shapedValue = applyCurve(normalizedValue, mapping.curve);

            // 映射到目标参数范围
            float targetValue = lerp(mapping.minValue, mapping.maxValue, shapedValue);

            // 通过回调设置参数值
            parameterSetter_(mapping.parameterId, targetValue);
        }
    }
}

float MacroControl::getMacroValue(int macroIndex) const {
    if (macroIndex < 0 || macroIndex >= kNumMacros) return 0.0f;
    return macroValues_[macroIndex];
}

const std::array<float, MacroControl::kNumMacros>& MacroControl::getAllMacroValues() const {
    return macroValues_;
}

void MacroControl::setMacroName(int macroIndex, const std::string& name) {
    if (macroIndex < 0 || macroIndex >= kNumMacros) return;
    macroNames_[macroIndex] = name;
}

std::string MacroControl::getMacroName(int macroIndex) const {
    if (macroIndex < 0 || macroIndex >= kNumMacros) return "";
    return macroNames_[macroIndex];
}

float MacroControl::applyCurve(float normalizedValue, float curve) {
    // curve范围: -1.0 (强对数) ~ 0.0 (线性) ~ 1.0 (强指数)
    // 避免极端值
    if (normalizedValue <= 0.0f) return 0.0f;
    if (normalizedValue >= 1.0f) return 1.0f;

    // 曲线映射: 使用指数函数
    // curve < 0: 对数风格 (先快后慢)
    // curve == 0: 线性
    // curve > 0: 指数风格 (先慢后快)
    const float curveStrength = 9.0f; // 曲线强度系数

    if (curve < -0.001f) {
        // 对数: 指数 < 1, 值先快速上升后变缓
        float exponent = 1.0f / (1.0f - curve * curveStrength);
        return std::pow(normalizedValue, exponent);
    } else if (curve > 0.001f) {
        // 指数: 指数 > 1, 值先缓慢上升后加速
        float exponent = 1.0f + curve * curveStrength;
        return std::pow(normalizedValue, exponent);
    } else {
        // 线性
        return normalizedValue;
    }
}

// =============================================================================
// MacroControl 序列化
// =============================================================================

juce::var MacroControl::toJson() const {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();

    // 宏映射
    juce::Array<juce::var> macrosArray;
    for (int i = 0; i < kNumMacros; ++i) {
        juce::DynamicObject::Ptr macroObj = new juce::DynamicObject();
        macroObj->setProperty("index", i);
        macroObj->setProperty("name", juce::String(macroNames_[i]));

        juce::Array<juce::var> mappingsArray;
        for (const auto& mapping : macroMappings_[i]) {
            mappingsArray.add(mapping.toJson());
        }
        macroObj->setProperty("mappings", juce::var(mappingsArray));
        macrosArray.add(juce::var(macroObj.get()));
    }
    obj->setProperty("macros", juce::var(macrosArray));

    // 当前宏值
    juce::Array<juce::var> valuesArray;
    for (int i = 0; i < kNumMacros; ++i) {
        valuesArray.add(juce::var(macroValues_[i]));
    }
    obj->setProperty("macroValues", juce::var(valuesArray));

    return juce::var(obj.get());
}

void MacroControl::fromJson(const juce::var& json) {
    clearAll();
    if (!json.isObject()) return;

    // 恢复宏映射
    if (auto* macrosArray = json["macros"].getArray()) {
        for (const auto& macroItem : *macrosArray) {
            if (!macroItem.isObject()) continue;

            int index = macroItem["index"];
            if (index < 0 || index >= kNumMacros) continue;

            macroNames_[index] = macroItem["name"].toString().toStdString();

            if (auto* mappingsArray = macroItem["mappings"].getArray()) {
                for (const auto& mappingItem : *mappingsArray) {
                    MacroMapping mapping = MacroMapping::fromJson(mappingItem);
                    macroMappings_[index].push_back(mapping);
                }
            }
        }
    }

    // 恢复宏值 (不触发回调)
    if (auto* valuesArray = json["macroValues"].getArray()) {
        for (int i = 0; i < juce::jmin(valuesArray->size(), kNumMacros); ++i) {
            macroValues_[i] = (*valuesArray)[i];
        }
    }
}

// =============================================================================
// ParameterMapper 实现
// =============================================================================

ParameterMapper::ParameterMapper() {
    // 将 MacroControl 的参数设置回调绑定到 ParameterMapper 的 writeParameterValue
    macroControl_.setParameterSetter([this](const std::string& paramId, float value) {
        writeParameterValue(paramId, value);
    });
}

ParameterMapper::~ParameterMapper() = default;

// =============================================================================
// 回调设置
// =============================================================================

void ParameterMapper::setParameterGetter(ParameterGetter getter) {
    parameterGetter_ = std::move(getter);
}

void ParameterMapper::setParameterSetter(ParameterSetter setter) {
    parameterSetter_ = std::move(setter);
}

// =============================================================================
// 参数注册
// =============================================================================

void ParameterMapper::registerParameter(const std::string& name, float minValue,
                                         float maxValue, float defaultValue) {
    ParameterDef def;
    def.name = name;
    def.minValue = minValue;
    def.maxValue = maxValue;
    def.defaultValue = defaultValue;
    parameterRegistry_[name] = def;

    // 初始化当前值为默认值
    currentValues_[name] = defaultValue;
}

void ParameterMapper::unregisterParameter(const std::string& name) {
    parameterRegistry_.erase(name);
    currentValues_.erase(name);
}

bool ParameterMapper::isParameterRegistered(const std::string& name) const {
    return parameterRegistry_.find(name) != parameterRegistry_.end();
}

const ParameterDef* ParameterMapper::getParameterDef(const std::string& name) const {
    auto it = parameterRegistry_.find(name);
    if (it != parameterRegistry_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<std::string> ParameterMapper::getRegisteredParameters() const {
    std::vector<std::string> result;
    result.reserve(parameterRegistry_.size());
    for (const auto& [name, _] : parameterRegistry_) {
        result.push_back(name);
    }
    return result;
}

// =============================================================================
// 当前参数值读写
// =============================================================================

float ParameterMapper::getCurrentValue(const std::string& paramName) const {
    // 优先从外部回调获取实时值
    if (parameterGetter_) {
        return parameterGetter_(paramName);
    }

    // 回退到内部缓存
    auto it = currentValues_.find(paramName);
    if (it != currentValues_.end()) {
        return it->second;
    }

    return 0.0f;
}

void ParameterMapper::setCurrentValue(const std::string& paramName, float value) {
    // 更新内部缓存
    currentValues_[paramName] = value;

    // 通过回调写入外部
    writeParameterValue(paramName, value);
}

// =============================================================================
// 内部写值辅助
// =============================================================================

void ParameterMapper::writeParameterValue(const std::string& paramId, float value) {
    // 限制在注册范围内
    auto defIt = parameterRegistry_.find(paramId);
    if (defIt != parameterRegistry_.end()) {
        value = clampValue(value, defIt->second.minValue, defIt->second.maxValue);
    }

    // 更新缓存
    currentValues_[paramId] = value;

    // 通过外部回调写入
    if (parameterSetter_) {
        parameterSetter_(paramId, value);
    }
}

// =============================================================================
// 快照管理
// =============================================================================

ParameterSnapshot ParameterMapper::captureSnapshot(const std::string& name,
                                                     const std::string& description) {
    ParameterSnapshot snapshot;
    snapshot.name = name.empty() ? "Snapshot " + std::to_string(snapshots_.size() + 1) : name;
    snapshot.description = description;
    snapshot.timestamp = juce::Time::getCurrentTime();

    // 捕获所有已注册参数的值
    if (parameterGetter_) {
        // 从外部回调获取实时值
        for (const auto& [paramName, _] : parameterRegistry_) {
            float value = parameterGetter_(paramName);
            snapshot.parameters.emplace_back(paramName, value);
        }
    } else {
        // 从内部缓存获取
        for (const auto& [paramName, value] : currentValues_) {
            snapshot.parameters.emplace_back(paramName, value);
        }
    }

    return snapshot;
}

void ParameterMapper::storeSnapshot(const ParameterSnapshot& snapshot) {
    snapshots_.push_back(snapshot);
}

void ParameterMapper::applySnapshot(const ParameterSnapshot& snapshot, float morphTime) {
    if (morphTime <= 0.0f) {
        // 立即应用: 直接设置所有参数值
        for (const auto& [paramName, value] : snapshot.parameters) {
            writeParameterValue(paramName, value);
        }
    } else {
        // 渐变过渡: 先捕获当前状态为快照，然后进行morph
        // 注意: 实际的渐变需要在音频线程或定时器中逐帧执行
        // 这里我们立即捕获当前快照并存储，供外部定时器使用
        ParameterSnapshot currentSnapshot = captureSnapshot("__morph_current__", "Morph source");
        ParameterSnapshot morphedSnapshot = currentSnapshot.morphTo(snapshot, 0.0f);

        // 存储源快照和目标快照供外部使用
        // 外部代码可以调用 morphTo 逐步推进 progress
        storeSnapshot(currentSnapshot);  // 源快照 (index = N-2)
        storeSnapshot(snapshot);         // 目标快照 (index = N-1, 即最后一个)

        // 立即应用初始插值结果
        for (const auto& [paramName, value] : morphedSnapshot.parameters) {
            writeParameterValue(paramName, value);
        }
    }
}

const std::vector<ParameterSnapshot>& ParameterMapper::getSnapshots() const {
    return snapshots_;
}

const ParameterSnapshot* ParameterMapper::findSnapshot(const std::string& name) const {
    for (const auto& snapshot : snapshots_) {
        if (snapshot.name == name) {
            return &snapshot;
        }
    }
    return nullptr;
}

void ParameterMapper::clearSnapshots() {
    snapshots_.clear();
}

// =============================================================================
// 自动化管理
// =============================================================================

void ParameterMapper::addAutomationPoint(const std::string& paramName, double time,
                                          float value, AutomationCurveType curveType) {
    automation_.addBreakpoint(paramName, time, value, curveType);
}

float ParameterMapper::getAutomationValue(const std::string& paramName, double time) const {
    return automation_.evaluate(paramName, time);
}

void ParameterMapper::removeAutomationPoint(const std::string& paramName, double time) {
    automation_.removeBreakpoint(paramName, time);
}

void ParameterMapper::clearAutomation(const std::string& paramName) {
    automation_.clearParameter(paramName);
}

void ParameterMapper::clearAllAutomation() {
    automation_.clearAll();
}

const ParameterAutomation& ParameterMapper::getAutomation() const {
    return automation_;
}

// =============================================================================
// 宏控制管理
// =============================================================================

void ParameterMapper::addMacroMapping(int macroIndex, const std::string& paramName,
                                       float minValue, float maxValue, float curve) {
    macroControl_.addMapping(macroIndex, paramName, minValue, maxValue, curve);
}

void ParameterMapper::setMacroValue(int macroIndex, float normalizedValue) {
    macroControl_.setMacroValue(macroIndex, normalizedValue);
}

float ParameterMapper::getMacroValue(int macroIndex) const {
    return macroControl_.getMacroValue(macroIndex);
}

void ParameterMapper::setMacroName(int macroIndex, const std::string& name) {
    macroControl_.setMacroName(macroIndex, name);
}

std::string ParameterMapper::getMacroName(int macroIndex) const {
    return macroControl_.getMacroName(macroIndex);
}

const MacroControl& ParameterMapper::getMacroControl() const {
    return macroControl_;
}

// =============================================================================
// 全局序列化
// =============================================================================

juce::var ParameterMapper::toJson() const {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();

    // 参数注册表
    juce::Array<juce::var> registryArray;
    for (const auto& [name, def] : parameterRegistry_) {
        juce::DynamicObject::Ptr paramObj = new juce::DynamicObject();
        paramObj->setProperty("name", juce::String(name));
        paramObj->setProperty("minValue", def.minValue);
        paramObj->setProperty("maxValue", def.maxValue);
        paramObj->setProperty("defaultValue", def.defaultValue);
        registryArray.add(juce::var(paramObj.get()));
    }
    obj->setProperty("parameterRegistry", juce::var(registryArray));

    // 当前值
    juce::Array<juce::var> valuesArray;
    for (const auto& [name, value] : currentValues_) {
        juce::DynamicObject::Ptr valObj = new juce::DynamicObject();
        valObj->setProperty("name", juce::String(name));
        valObj->setProperty("value", value);
        valuesArray.add(juce::var(valObj.get()));
    }
    obj->setProperty("currentValues", juce::var(valuesArray));

    // 快照列表
    juce::Array<juce::var> snapshotsArray;
    for (const auto& snapshot : snapshots_) {
        snapshotsArray.add(snapshot.toJson());
    }
    obj->setProperty("snapshots", juce::var(snapshotsArray));

    // 自动化数据
    obj->setProperty("automation", automation_.toJson());

    // 宏控制数据
    obj->setProperty("macroControl", macroControl_.toJson());

    return juce::var(obj.get());
}

void ParameterMapper::fromJson(const juce::var& json) {
    if (!json.isObject()) return;

    // 恢复参数注册表
    parameterRegistry_.clear();
    if (auto* registryArray = json["parameterRegistry"].getArray()) {
        for (const auto& item : *registryArray) {
            if (!item.isObject()) continue;
            ParameterDef def;
            def.name = item["name"].toString().toStdString();
            def.minValue = item["minValue"];
            def.maxValue = item["maxValue"];
            def.defaultValue = item["defaultValue"];
            parameterRegistry_[def.name] = def;
        }
    }

    // 恢复当前值
    currentValues_.clear();
    if (auto* valuesArray = json["currentValues"].getArray()) {
        for (const auto& item : *valuesArray) {
            if (!item.isObject()) continue;
            std::string name = item["name"].toString().toStdString();
            float value = item["value"];
            currentValues_[name] = value;
        }
    }

    // 恢复快照
    snapshots_.clear();
    if (auto* snapshotsArray = json["snapshots"].getArray()) {
        for (const auto& item : *snapshotsArray) {
            snapshots_.push_back(ParameterSnapshot::fromJson(item));
        }
    }

    // 恢复自动化
    automation_.fromJson(json["automation"]);

    // 恢复宏控制
    macroControl_.fromJson(json["macroControl"]);

    // 重新绑定宏控制回调 (fromJson可能清除了旧的回调绑定)
    macroControl_.setParameterSetter([this](const std::string& paramId, float value) {
        writeParameterValue(paramId, value);
    });
}

bool ParameterMapper::saveToFile(const juce::File& file) const {
    juce::var json = toJson();
    juce::String jsonString = juce::JSON::toString(json, false, 2); // 缩进2空格，便于阅读

    juce::FileOutputStream stream(file);
    if (!stream.openedOk()) return false;

    stream.setPosition(0);
    stream.truncate();
    return stream.writeText(jsonString, false, false, nullptr);
}

bool ParameterMapper::loadFromFile(const juce::File& file) {
    if (!file.existsAsFile()) return false;

    juce::String jsonString = file.loadFileAsString();
    juce::var json = juce::JSON::parse(jsonString);

    if (json.isVoid()) return false;

    fromJson(json);
    return true;
}

} // namespace LianCore