// LianCore V3 - EffectsChainPreset 效果器链预设系统
// 允许用户保存/加载完整效果器链配置（12个效果器参数、顺序、路由、干湿比）
// 支持JSON序列化，兼容PresetManager
#pragma once
#include <JuceHeader.h>
#include <vector>
#include <string>
#include <algorithm>

namespace LianCore {

// 效果器链预设数据结构
struct EffectsChainPreset {
    std::string name;
    std::string category;
    std::string author;
    std::string description;
    
    // 12个效果器的参数（每个效果器有多个参数，扁平化存储）
    // 索引: effectIndex * paramsPerEffect + paramIndex
    std::vector<float> effectsParams;
    
    // 效果器处理顺序（0-11，可重新排列）
    std::vector<int> effectsOrder;
    
    // 路由模式: 0=串行, 1=并行, 2=混合
    int routingMode = 0;
    
    // 每个效果器的干湿比 (0.0 = 全干, 1.0 = 全湿)
    std::vector<float> wetDryMixes;
    
    // 每个效果器的启用状态
    std::vector<bool> enabled;
    
    // 全局增益补偿 (dB)
    float outputGain = 0.0f;
    
    // 预设版本号
    int version = 1;
    
    // 序列化为JSON
    juce::var toJson() const {
        auto obj = new juce::DynamicObject();
        obj->setProperty("name", juce::String(name));
        obj->setProperty("category", juce::String(category));
        obj->setProperty("author", juce::String(author));
        obj->setProperty("description", juce::String(description));
        obj->setProperty("routingMode", routingMode);
        obj->setProperty("outputGain", outputGain);
        obj->setProperty("version", version);
        
        juce::Array<juce::var> params;
        for (auto v : effectsParams) params.add(v);
        obj->setProperty("effectsParams", params);
        
        juce::Array<juce::var> order;
        for (auto v : effectsOrder) order.add(v);
        obj->setProperty("effectsOrder", order);
        
        juce::Array<juce::var> mixes;
        for (auto v : wetDryMixes) mixes.add(v);
        obj->setProperty("wetDryMixes", mixes);
        
        juce::Array<juce::var> en;
        for (auto v : enabled) en.add(v ? 1 : 0);
        obj->setProperty("enabled", en);
        
        return juce::var(obj);
    }
    
    // 从JSON反序列化
    void fromJson(const juce::var& json) {
        if (auto* obj = json.getDynamicObject()) {
            name = obj->getProperty("name").toString().toStdString();
            category = obj->getProperty("category").toString().toStdString();
            author = obj->getProperty("author").toString().toStdString();
            description = obj->getProperty("description").toString().toStdString();
            routingMode = (int)obj->getProperty("routingMode");
            outputGain = (float)obj->getProperty("outputGain");
            version = (int)obj->getProperty("version");
            
            effectsParams.clear();
            if (auto* arr = obj->getProperty("effectsParams").getArray()) {
                for (auto& v : *arr) effectsParams.push_back((float)(double)v);
            }
            
            effectsOrder.clear();
            if (auto* arr = obj->getProperty("effectsOrder").getArray()) {
                for (auto& v : *arr) effectsOrder.push_back((int)(double)v);
            }
            
            wetDryMixes.clear();
            if (auto* arr = obj->getProperty("wetDryMixes").getArray()) {
                for (auto& v : *arr) wetDryMixes.push_back((float)(double)v);
            }
            
            enabled.clear();
            if (auto* arr = obj->getProperty("enabled").getArray()) {
                for (auto& v : *arr) enabled.push_back((int)(double)v > 0);
            }
        }
    }
    
    // 验证预设有效性
    bool isValid() const {
        if (name.empty()) return false;
        if (effectsParams.size() < 12) return false;  // 至少12个参数
        if (effectsOrder.size() < 12) return false;
        if (wetDryMixes.size() < 12) return false;
        if (enabled.size() < 12) return false;
        return true;
    }
};

// 效果器链预设管理器
class EffectsChainPresetManager {
public:
    EffectsChainPresetManager() {}
    
    // 保存单个预设到文件
    bool savePreset(const EffectsChainPreset& preset, const juce::File& file) {
        auto json = preset.toJson();
        juce::FileOutputStream stream(file);
        if (!stream.openedOk()) return false;
        stream.writeText(json.toString(), false, false, "\n");
        return true;
    }
    
    // 从文件加载单个预设
    bool loadPreset(EffectsChainPreset& preset, const juce::File& file) {
        if (!file.existsAsFile()) return false;
        juce::String content = file.loadFileAsString();
        auto json = juce::JSON::parse(content);
        if (json.isVoid()) return false;
        preset.fromJson(json);
        return preset.isValid();
    }
    
    // 保存到预设管理器目录
    bool saveToPresetManager(const EffectsChainPreset& preset, const juce::File& presetDir) {
        auto file = presetDir.getChildFile(preset.name + ".fxchain");
        return savePreset(preset, file);
    }
    
    // 从预设管理器目录加载
    bool loadFromPresetManager(EffectsChainPreset& preset, const std::string& name, 
                                const juce::File& presetDir) {
        auto file = presetDir.getChildFile(name + ".fxchain");
        return loadPreset(preset, file);
    }
    
    // 列出所有预设名称
    std::vector<std::string> listPresets(const juce::File& presetDir) const {
        std::vector<std::string> names;
        auto files = presetDir.findChildFiles(juce::File::findFiles, false, "*.fxchain");
        for (auto& f : files) {
            names.push_back(f.getFileNameWithoutExtension().toStdString());
        }
        std::sort(names.begin(), names.end());
        return names;
    }
    
    // 删除预设
    bool deletePreset(const std::string& name, const juce::File& presetDir) {
        auto file = presetDir.getChildFile(name + ".fxchain");
        return file.deleteFile();
    }
    
    // 复制预设
    bool copyPreset(const std::string& sourceName, const std::string& destName, 
                    const juce::File& presetDir) {
        EffectsChainPreset preset;
        if (!loadFromPresetManager(preset, sourceName, presetDir)) return false;
        preset.name = destName;
        return saveToPresetManager(preset, presetDir);
    }
    
    // 导出预设为JSON字符串
    juce::String exportToString(const EffectsChainPreset& preset) {
        return preset.toJson().toString();
    }
    
    // 从JSON字符串导入预设
    bool importFromString(EffectsChainPreset& preset, const juce::String& jsonStr) {
        auto json = juce::JSON::parse(jsonStr);
        if (json.isVoid()) return false;
        preset.fromJson(json);
        return preset.isValid();
    }
    
    // 创建默认预设（所有效果器直通）
    static EffectsChainPreset createDefault() {
        EffectsChainPreset p;
        p.name = "Default";
        p.category = "Basic";
        p.author = "LianCore";
        p.description = "Default effects chain with all effects bypassed";
        p.routingMode = 0;
        p.outputGain = 0.0f;
        p.version = 1;
        for (int i = 0; i < 12; ++i) {
            p.effectsParams.push_back(0.5f);   // 默认中间值
            p.effectsOrder.push_back(i);       // 默认顺序
            p.wetDryMixes.push_back(0.5f);     // 50%干湿比
            p.enabled.push_back(i == 0);       // 仅第一个效果器启用
        }
        return p;
    }
    
    // 创建"干净"预设（所有效果器旁路）
    static EffectsChainPreset createCleanBypass() {
        EffectsChainPreset p;
        p.name = "Clean Bypass";
        p.category = "Basic";
        p.author = "LianCore";
        p.description = "All effects bypassed for clean signal";
        p.routingMode = 0;
        p.outputGain = 0.0f;
        p.version = 1;
        for (int i = 0; i < 12; ++i) {
            p.effectsParams.push_back(0.5f);
            p.effectsOrder.push_back(i);
            p.wetDryMixes.push_back(0.0f);     // 全干
            p.enabled.push_back(false);         // 全部禁用
        }
        return p;
    }
};

} // namespace LianCore
