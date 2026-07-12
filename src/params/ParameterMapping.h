// =============================================================================
// LianCore - ParameterMapping 参数映射系统
// 快照管理、自动化包络、宏控制、参数映射器
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>

namespace LianCore {

// =============================================================================
// 前向声明
// =============================================================================
class ParameterMapper;

// =============================================================================
// 曲线类型枚举
// =============================================================================
enum class AutomationCurveType {
    Linear,      // 线性插值
    Smooth,      // 余弦平滑插值
    Exponential, // 指数曲线
    Hold         // 保持上一个值 (阶跃)
};

// =============================================================================
// 自动化断点
// =============================================================================
struct AutomationBreakpoint {
    double time = 0.0;                      // 时间 (秒)
    float value = 0.0f;                     // 参数值
    AutomationCurveType curveType = AutomationCurveType::Linear;

    juce::var toJson() const;
    static AutomationBreakpoint fromJson(const juce::var& json);
};

// =============================================================================
// ParameterSnapshot - 参数快照
// 记录某一时刻所有参数的值，支持序列化和渐变过渡
// =============================================================================
struct ParameterSnapshot {
    // 参数名 -> 值 的映射
    std::vector<std::pair<std::string, float>> parameters;

    // 元数据
    std::string name;
    std::string description;
    juce::Time timestamp;

    // =========================================================================
    // 序列化
    // =========================================================================
    juce::var toJson() const;
    static ParameterSnapshot fromJson(const juce::var& json);

    // =========================================================================
    // 渐变过渡 - 在两个快照之间插值
    // progress: 0.0 = 完全保留当前快照, 1.0 = 完全变为目标快照
    // 返回一个新的插值快照
    // =========================================================================
    ParameterSnapshot morphTo(const ParameterSnapshot& target, float progress) const;
};

// =============================================================================
// ParameterAutomation - 参数自动化包络
// 每个参数可以有多个断点，通过时间轴驱动参数变化
// =============================================================================
class ParameterAutomation {
public:
    ParameterAutomation() = default;
    ~ParameterAutomation() = default;

    // =========================================================================
    // 断点管理
    // =========================================================================
    // 添加断点 (自动按时间排序)
    void addBreakpoint(const std::string& paramName, double time, float value,
                       AutomationCurveType curveType = AutomationCurveType::Linear);

    // 移除参数的某个断点
    void removeBreakpoint(const std::string& paramName, double time);

    // 清除某个参数的所有断点
    void clearParameter(const std::string& paramName);

    // 清除所有断点
    void clearAll();

    // 获取参数的所有断点 (只读)
    const std::vector<AutomationBreakpoint>* getBreakpoints(const std::string& paramName) const;

    // 获取所有有自动化数据的参数名
    std::vector<std::string> getAutomatedParameters() const;

    // 获取断点总数
    int getTotalBreakpointCount() const;

    // =========================================================================
    // 求值 - 在指定时间获取参数值
    // =========================================================================
    // 根据时间和曲线类型插值计算
    float evaluate(const std::string& paramName, double time) const;

    // 检查参数在指定时间是否有自动化数据
    bool hasAutomation(const std::string& paramName) const;

    // =========================================================================
    // 序列化
    // =========================================================================
    juce::var toJson() const;
    void fromJson(const juce::var& json);

private:
    // 参数名 -> 断点列表 (按时间排序)
    std::unordered_map<std::string, std::vector<AutomationBreakpoint>> automationData_;

    // 插值辅助函数
    static float interpolateLinear(float v0, float v1, float t);
    static float interpolateSmooth(float v0, float v1, float t);
    static float interpolateExponential(float v0, float v1, float t);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterAutomation)
};

// =============================================================================
// MacroControl - 宏控制旋钮系统 (8个宏)
// 每个宏旋钮可以映射到多个目标参数，支持范围和曲线控制
// =============================================================================
class MacroControl {
public:
    // 宏映射定义
    struct MacroMapping {
        std::string parameterId;    // 目标参数ID
        float minValue = 0.0f;      // 宏值为0时对应的参数值
        float maxValue = 1.0f;      // 宏值为1时对应的参数值
        float curve = 0.0f;         // 曲线: -1.0(对数) ~ 0.0(线性) ~ 1.0(指数)

        juce::var toJson() const;
        static MacroMapping fromJson(const juce::var& json);
    };

    // 宏数量
    static constexpr int kNumMacros = 8;

    // 回调类型: 设置参数值
    using ParameterSetter = std::function<void(const std::string& paramId, float value)>;

    MacroControl() = default;
    ~MacroControl() = default;

    // =========================================================================
    // 设置参数回调 (必须设置才能使用宏)
    // =========================================================================
    void setParameterSetter(ParameterSetter setter);

    // =========================================================================
    // 宏映射管理
    // =========================================================================
    // 为指定宏添加参数映射
    void addMapping(int macroIndex, const std::string& paramId,
                    float minValue, float maxValue, float curve = 0.0f);

    // 移除指定宏的某个参数映射
    void removeMapping(int macroIndex, const std::string& paramId);

    // 清除指定宏的所有映射
    void clearMacro(int macroIndex);

    // 清除所有宏映射
    void clearAll();

    // 获取指定宏的所有映射 (只读)
    const std::vector<MacroMapping>* getMappings(int macroIndex) const;

    // 获取宏总数
    static int getMacroCount() { return kNumMacros; }

    // =========================================================================
    // 宏值操作
    // =========================================================================
    // 设置宏值 (0.0 ~ 1.0), 自动触发所有目标参数更新
    void setMacroValue(int macroIndex, float normalizedValue);

    // 获取当前宏值
    float getMacroValue(int macroIndex) const;

    // 获取所有宏的当前值
    const std::array<float, kNumMacros>& getAllMacroValues() const;

    // 获取宏名称
    void setMacroName(int macroIndex, const std::string& name);
    std::string getMacroName(int macroIndex) const;

    // =========================================================================
    // 序列化
    // =========================================================================
    juce::var toJson() const;
    void fromJson(const juce::var& json);

private:
    // 应用曲线到归一化值
    static float applyCurve(float normalizedValue, float curve);

    // 回调
    ParameterSetter parameterSetter_;

    // 每个宏的映射列表
    std::array<std::vector<MacroMapping>, kNumMacros> macroMappings_;

    // 当前宏值
    std::array<float, kNumMacros> macroValues_ = {};

    // 宏名称
    std::array<std::string, kNumMacros> macroNames_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MacroControl)
};

// =============================================================================
// 参数定义 (用于注册)
// =============================================================================
struct ParameterDef {
    std::string name;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.5f;
};

// =============================================================================
// ParameterMapper - 参数映射器 (主控类)
// 统一管理快照、自动化、宏控制，是参数映射系统的对外接口
// =============================================================================
class ParameterMapper {
public:
    // 回调类型
    using ParameterGetter = std::function<float(const std::string& paramId)>;
    using ParameterSetter = std::function<void(const std::string& paramId, float value)>;

    ParameterMapper();
    ~ParameterMapper();

    // =========================================================================
    // 回调设置 - 必须设置才能读写实际参数
    // =========================================================================
    void setParameterGetter(ParameterGetter getter);
    void setParameterSetter(ParameterSetter setter);

    // =========================================================================
    // 参数注册
    // =========================================================================
    void registerParameter(const std::string& name, float minValue, float maxValue,
                          float defaultValue);
    void unregisterParameter(const std::string& name);
    bool isParameterRegistered(const std::string& name) const;
    const ParameterDef* getParameterDef(const std::string& name) const;
    std::vector<std::string> getRegisteredParameters() const;

    // =========================================================================
    // 当前参数值读写
    // =========================================================================
    float getCurrentValue(const std::string& paramName) const;
    void setCurrentValue(const std::string& paramName, float value);

    // =========================================================================
    // 快照管理
    // =========================================================================
    // 捕获当前所有参数值为快照
    ParameterSnapshot captureSnapshot(const std::string& name = "",
                                      const std::string& description = "");

    // 存储快照到内部列表
    void storeSnapshot(const ParameterSnapshot& snapshot);

    // 应用快照 (morphTime > 0 时触发渐变过渡)
    void applySnapshot(const ParameterSnapshot& snapshot, float morphTime = 0.0f);

    // 获取已存储的快照列表
    const std::vector<ParameterSnapshot>& getSnapshots() const;

    // 按名称查找快照
    const ParameterSnapshot* findSnapshot(const std::string& name) const;

    // 清除所有快照
    void clearSnapshots();

    // =========================================================================
    // 自动化管理
    // =========================================================================
    void addAutomationPoint(const std::string& paramName, double time, float value,
                           AutomationCurveType curveType = AutomationCurveType::Linear);

    float getAutomationValue(const std::string& paramName, double time) const;

    void removeAutomationPoint(const std::string& paramName, double time);

    void clearAutomation(const std::string& paramName);

    void clearAllAutomation();

    const ParameterAutomation& getAutomation() const;

    // =========================================================================
    // 宏控制管理
    // =========================================================================
    void addMacroMapping(int macroIndex, const std::string& paramName,
                        float minValue, float maxValue, float curve = 0.0f);

    void setMacroValue(int macroIndex, float normalizedValue);

    float getMacroValue(int macroIndex) const;

    void setMacroName(int macroIndex, const std::string& name);

    std::string getMacroName(int macroIndex) const;

    const MacroControl& getMacroControl() const;

    // =========================================================================
    // 全局序列化 - 完整状态保存/恢复
    // =========================================================================
    juce::var toJson() const;
    void fromJson(const juce::var& json);

    // 保存到文件 / 从文件加载
    bool saveToFile(const juce::File& file) const;
    bool loadFromFile(const juce::File& file);

private:
    // 参数注册表
    std::unordered_map<std::string, ParameterDef> parameterRegistry_;

    // 当前参数值缓存
    std::unordered_map<std::string, float> currentValues_;

    // 回调
    ParameterGetter parameterGetter_;
    ParameterSetter parameterSetter_;

    // 子系统
    std::vector<ParameterSnapshot> snapshots_;
    ParameterAutomation automation_;
    MacroControl macroControl_;

    // 内部辅助 - 通过回调写值
    void writeParameterValue(const std::string& paramId, float value);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterMapper)
};

} // namespace LianCore