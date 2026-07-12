# LianCore 详细技术规格与验收文档

> **版本**: v1.0.0  
> **日期**: 2026-07-12  
> **项目代号**: LianCore V3  
> **文档类型**: 技术规格与验收标准 (Specification & Acceptance)

---

## 目录

1. [项目概述](#1-项目概述)
2. [系统架构总览](#2-系统架构总览)
3. [模块划分与接口定义](#3-模块划分与接口定义)
4. [详细功能验收标准](#4-详细功能验收标准)
5. [Alpha 阶段开发任务清单](#5-alpha-阶段开发任务清单)
6. [技术栈与依赖](#6-技术栈与依赖)
7. [技能使用计划](#7-技能使用计划)

---

## 1. 项目概述

### 1.1 核心定位
LianCore 是一款商业端侧 AI 合成器软音源（VST3/AAX/AU），融合以下四款经典合成器的标志性功能：

| 参考合成器 | 核心功能 |
|-----------|---------|
| **Serum 2** | 双波表振荡器、256帧波表、wav导入/导出、AI波表生成、实时morphing |
| **Vital** | 频谱振荡器、FFT重合成、频谱拉伸/移调、AI频谱解析映射 |
| **Absynth** | 粒子采样播放器、改进Karplus-Strong物理模型、AI降噪、动态粒子密度 |
| **VPS Avenger 2** | 多采样映射器、鼓切片触发、步进音序器、AI切片探测、.avpr扩展包转换 |

### 1.2 关键性能指标

| 指标 | 目标值 |
|------|-------|
| 闲置CPU占用 | <1% |
| 演奏典型音色CPU占用 | <1.5% |
| 690轨叠加内存占用 | 每实例≤8MB，总计≤5.4GB（8GB RAM需求） |
| 启动时间 | ≤2.5s |
| 音色加载时间 | ≤0.8s |
| AI推理延迟 | <8ms（i3-4130 CPU） |
| MIDI延迟 | <5ms |
| AI模型大小 | ≤200MB（ONNX格式） |

### 1.3 分阶段交付计划

| 阶段 | 周期 | 核心交付物 |
|------|------|-----------|
| **Alpha** | 8周 | 基础节点式音频图、双波表+虚拟模拟振荡器、滤波器、VST3原型（无UI） |
| **Beta** | 12周 | 集成所有合成引擎、AI ONNX模型、基础Web UI（极简模式）、参数映射 |
| **Release** | 8周 | 专业模式UI、预制库、多语言、安装包、全套测试报告 |

---

## 2. 系统架构总览

### 2.1 整体架构图

```
┌──────────────────────────────────────────────────────────────────┐
│                        LianCore 系统架构                          │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                    UI 层 (React + WebGL)                   │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  │   │
│  │  │ 极简模式  │  │ 专业模式  │  │ 预制浏览器 │  │ AI 生成栏 │  │   │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────┘  │   │
│  └───────────────────────┬──────────────────────────────────┘   │
│                          │ IPC (WebSocket / Shared Memory)       │
│  ┌───────────────────────┴──────────────────────────────────┐   │
│  │                  C++ 核心层 (JUCE Framework)               │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  │   │
│  │  │ 音频图引擎 │  │ 合成引擎组 │  │ 调制矩阵  │  │ 参数树   │  │   │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────┘  │   │
│  └───────────────────────┬──────────────────────────────────┘   │
│                          │                                       │
│  ┌───────────────────────┴──────────────────────────────────┐   │
│  │                  AI 推理层 (ONNX Runtime)                  │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐               │   │
│  │  │ 文本→参数  │  │ 音频→频谱  │  │ 波表生成  │               │   │
│  │  └──────────┘  └──────────┘  └──────────┘               │   │
│  └───────────────────────┬──────────────────────────────────┘   │
│                          │                                       │
│  ┌───────────────────────┴──────────────────────────────────┐   │
│  │                   数据层 (SQLite + 文件系统)               │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐               │   │
│  │  │ 预制库    │  │ 波表缓存  │  │ 采样缓存  │               │   │
│  │  └──────────┘  └──────────┘  └──────────┘               │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

### 2.2 数据流图

```
用户输入 (文本/音频/MIDI)
    │
    ├──→ AI 推理引擎 ──→ 参数映射 ──→ 参数树更新 ──→ UI 刷新
    │
    └──→ MIDI 处理器 ──→ 音频图引擎 ──→ 合成引擎 ──→ 音频输出
                              │
                              └──→ 调制矩阵 ──→ 实时参数调制
```

---

## 3. 模块划分与接口定义

### 3.1 音频图引擎 (AudioGraphEngine)

#### 3.1.1 模块概述
统一节点式音频图架构，所有音频处理单元以节点(Node)形式存在，通过连接(Connection)建立信号流。

#### 3.1.2 核心类接口

```cpp
// 音频节点基类
class AudioNode {
public:
    virtual void prepareToPlay(double sampleRate, int maxSamplesPerBlock) = 0;
    virtual void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) = 0;
    virtual void releaseResources() = 0;
    
    // 节点标识
    juce::String getNodeId() const;
    NodeType getNodeType() const;
    
    // 端口管理
    int getNumInputPorts() const;
    int getNumOutputPorts() const;
    PortDescriptor getPortDescriptor(int portIndex, bool isInput) const;
    
protected:
    juce::String nodeId_;
    NodeType nodeType_;
    std::vector<AudioPort> inputPorts_;
    std::vector<AudioPort> outputPorts_;
};

// 音频图管理器
class AudioGraphEngine {
public:
    // 节点管理
    NodeId addNode(NodeType type, const juce::String& name);
    void removeNode(NodeId id);
    AudioNode* getNode(NodeId id);
    
    // 连接管理
    ConnectionId connect(NodeId src, int srcPort, NodeId dst, int dstPort);
    void disconnect(ConnectionId id);
    
    // 拓扑排序与处理
    void prepareToPlay(double sampleRate, int maxSamplesPerBlock);
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);
    void releaseResources();
    
    // 序列化
    juce::var toJson() const;
    void fromJson(const juce::var& json);
    
    // 性能监控
    double getCurrentCpuUsage() const;
    double getAverageCpuUsage() const;
    
private:
    std::unordered_map<NodeId, std::unique_ptr<AudioNode>> nodes_;
    std::vector<AudioConnection> connections_;
    std::vector<AudioNode*> processingOrder_; // 拓扑排序结果
};
```

#### 3.1.3 节点类型枚举

```cpp
enum class NodeType {
    // 合成引擎
    WavetableOscillator,    // 波表振荡器
    SpectralOscillator,     // 频谱振荡器
    GranularPlayer,         // 粒子播放器
    WaveguideResonator,     // 波导谐振器
    SamplerPlayer,          // 多采样播放器
    DrumSlicer,             // 鼓切片触发器
    
    // 信号处理
    Filter,                 // 滤波器
    Distortion,             // 失真
    Delay,                  // 延迟
    Reverb,                 // 混响
    Compressor,             // 压缩器
    EQ,                     // 均衡器
    
    // 调制器
    LFO,                    // 低频振荡器
    Envelope,               // 包络
    MacroControl,           // 宏控制
    StepSequencer,          // 步进音序器
    
    // 路由
    Mixer,                  // 混合器
    Splitter,               // 分离器
    AudioInput,             // 音频输入
    AudioOutput,            // 音频输出
};
```

### 3.2 合成引擎模块

#### 3.2.1 波表引擎 (WavetableEngine - 对标 Serum 2)

```cpp
class WavetableOscillator : public AudioNode {
public:
    // 波表管理
    void loadWavetable(const juce::File& wavFile);
    void loadWavetable(const juce::AudioSampleBuffer& wavetable, int frameCount);
    void saveWavetable(const juce::File& wavFile);
    void generateWavetable(const std::vector<float>& aiParams); // AI生成
    
    // 播放控制
    void setFrequency(float hz);
    void setFrameIndex(float frame); // 0.0 - 255.0 (支持插值)
    void setUnisonVoices(int count);
    void setUnisonDetune(float cents);
    void setUnisonBlend(float blend);
    
    // 变形控制
    void setMorphTarget(const juce::AudioSampleBuffer& target, float amount);
    void setWarpMode(WarpMode mode);
    
    // 效果
    void setBendMode(BendMode mode); // FM, AM, RM, Sync, etc.
    void setBendAmount(float amount);
    
private:
    // 双波表振荡器
    WavetableBank wavetableA_;
    WavetableBank wavetableB_;
    float morphAmount_; // A↔B混合量
    
    // 256帧波表数据
    static constexpr int kMaxFrames = 256;
    static constexpr int kFrameSize = 2048; // 每帧采样数
    
    // SIMD优化的插值
    alignas(64) float phaseBuffer_[kMaxBlockSize];
};
```

#### 3.2.2 频谱引擎 (SpectralEngine - 对标 Vital)

```cpp
class SpectralOscillator : public AudioNode {
public:
    // FFT参数
    void setFftSize(int size); // 256-4096
    void setOverlap(int overlap); // 2x, 4x, 8x
    
    // 频谱操作
    void setSpectralStretch(float amount); // 频谱拉伸
    void setSpectralShift(float semitones); // 频谱移调
    void setHarmonicBlend(float blend); // 谐波混合
    
    // AI频谱映射
    void loadReferenceSpectrum(const juce::AudioSampleBuffer& audio);
    void applyAiSpectralMapping(const std::vector<float>& aiHarmonicParams);
    
    // 共振峰
    void setFormantShift(float amount);
    void setFormantPreset(int index);
    
private:
    juce::dsp::FFT fft_;
    juce::dsp::WindowingFunction<float> window_;
    std::vector<float> spectralData_;
    std::vector<float> harmonicTemplate_;
};
```

#### 3.2.3 粒子/波导引擎 (GranularWaveguideEngine - 对标 Absynth)

```cpp
class GranularPlayer : public AudioNode {
public:
    // 粒子参数
    void setGrainSize(float ms); // 1-500ms
    void setGrainDensity(float grainsPerSec); // 1-200
    void setGrainPosition(float normalizedPos); // 0.0-1.0
    void setGrainPitchRandom(float semitones);
    void setGrainPanRandom(float amount);
    
    // 粒子包络
    void setGrainEnvelope(EnvelopeType type);
    void setGrainEnvelopeLength(float ms);
    
    // AI动态调整
    void enableAiDensityControl(bool enabled);
    void setAiDenoiseLevel(float level);
    
private:
    juce::AudioSampleBuffer sourceBuffer_;
    std::vector<Grain> activeGrains_;
    static constexpr int kMaxGrains = 512;
};

class WaveguideResonator : public AudioNode {
public:
    // Karplus-Strong改进模型
    void setFrequency(float hz);
    void setDecay(float decay); // 0.0-1.0
    void setDamping(float damping);
    void setBodyResonance(float amount);
    void setExcitationType(ExcitationType type); // 噪声/脉冲/采样
    
    // 物理参数
    void setStringTension(float tension);
    void setStringInharmonicity(float amount);
    void setPickupPosition(float pos); // 0.0-1.0
    
private:
    juce::dsp::DelayLine<float> delayLine_;
    juce::dsp::IIR::Filter<float> dampingFilter_;
    static constexpr int kMaxDelaySamples = 48000; // 1秒@48kHz
};
```

#### 3.2.4 混合引擎 (HybridEngine - 对标 VPS Avenger 2)

```cpp
class MultiSampler : public AudioNode {
public:
    // 采样管理
    void addSample(const juce::File& file, int rootNote, int lowNote, int highNote);
    void removeSample(int index);
    void clearSamples();
    
    // 键位映射
    void setKeyRange(int sampleIndex, int lowNote, int highNote);
    void setVelocityRange(int sampleIndex, int lowVel, int highVel);
    void setRootNote(int sampleIndex, int note);
    
    // AI自动映射
    void autoMapSamples(const std::vector<juce::File>& files);
    void detectSlices(const juce::File& audioFile, float threshold);
    
    // 播放
    void setLoopMode(LoopMode mode);
    void setCrossfadeLength(int samples);
    
private:
    std::vector<SampleZone> sampleZones_;
    std::unordered_map<int, juce::AudioSampleBuffer> sampleCache_;
};

class DrumSlicer : public AudioNode {
public:
    // 切片管理
    void loadLoop(const juce::File& file);
    void detectSlices(float threshold, int minSliceLength);
    void addManualSlice(int sample);
    void removeSlice(int index);
    
    // 切片触发
    void triggerSlice(int index, float velocity);
    void setSlicePitch(int index, float semitones);
    void setSliceDirection(int index, PlayDirection dir);
    
    // MIDI映射
    void setSliceNote(int index, int midiNote);
    void autoMapToMidi();
    
private:
    juce::AudioSampleBuffer loopBuffer_;
    std::vector<Slice> slices_;
    
    // 每个切片独立播放器
    struct SlicePlayer {
        float position;
        float pitch;
        PlayDirection direction;
        int midiNote;
    };
    std::vector<SlicePlayer> slicePlayers_;
};

class StepSequencer : public AudioNode {
public:
    // 序列管理
    void setStepCount(int count); // 1-64
    void setStepValue(int step, float value); // 0.0-1.0
    void setStepGate(int step, bool gate);
    void setStepTie(int step, bool tie);
    
    // 播放控制
    void setRate(float rate); // 相对BPM
    void setSwing(float amount);
    void setDirection(PlayDirection dir);
    void setRandomMode(bool enabled);
    
    // 输出
    void setOutputMode(SeqOutputMode mode); // 音高/门限/CC/调制
    void setRange(float min, float max);
    
private:
    std::vector<float> steps_;
    std::vector<bool> gates_;
    int currentStep_;
    double sampleRate_;
};
```

### 3.3 调制矩阵 (ModulationMatrix)

```cpp
class ModulationMatrix {
public:
    // 调制源注册
    void registerSource(const juce::String& id, ModulationSource* source);
    void unregisterSource(const juce::String& id);
    
    // 调制目标注册
    void registerTarget(const juce::String& id, ModulationTarget* target);
    void unregisterTarget(const juce::String& id);
    
    // 调制路由
    void addModulation(const juce::String& sourceId, 
                       const juce::String& targetId, 
                       float amount);
    void removeModulation(int index);
    void setModulationAmount(int index, float amount);
    
    // 每采样处理
    void processBlock(int numSamples);
    
    // 调制源查询
    float getModulationValue(const juce::String& sourceId) const;
    
    // 可视化
    struct ModulationSnapshot {
        juce::String sourceId;
        juce::String targetId;
        float amount;
        float currentValue;
    };
    std::vector<ModulationSnapshot> getSnapshot() const;
    
private:
    struct ModulationRoute {
        ModulationSource* source;
        ModulationTarget* target;
        float amount;
        float lastValue;
    };
    std::vector<ModulationRoute> routes_;
    std::unordered_map<juce::String, ModulationSource*> sources_;
    std::unordered_map<juce::String, ModulationTarget*> targets_;
};

// 调制源接口
class ModulationSource {
public:
    virtual float getValue() const = 0;
    virtual juce::String getName() const = 0;
    virtual juce::String getUnit() const { return ""; }
    virtual bool isBipolar() const { return false; }
};

// 调制目标接口
class ModulationTarget {
public:
    virtual void applyModulation(float normalizedValue) = 0;
    virtual juce::String getName() const = 0;
    virtual float getCurrentValue() const = 0;
    virtual float getMinValue() const = 0;
    virtual float getMaxValue() const = 0;
};
```

### 3.4 AI 推理层 (AIInferenceEngine)

```cpp
class AIInferenceEngine {
public:
    // 模型管理
    bool loadModel(const juce::File& onnxFile);
    void unloadModel();
    bool isModelLoaded() const;
    
    // 推理模式
    enum class GenerationMode {
        TextOnly,           // 纯文本描述
        TextWithAudio,      // 文本+音频参考
        TextWithStyle,      // 文本+风格标签
    };
    
    // 核心推理
    struct GenerationResult {
        std::vector<ParameterMapping> parameters;
        std::vector<float> explanationEmbeddings; // 用于悬停解释
        std::vector<float> wavetableData;         // 可选：AI生成的波表
        juce::String presetName;
        float confidence;
    };
    
    GenerationResult generateParameters(
        const juce::String& textPrompt,
        const juce::AudioSampleBuffer* audioReference = nullptr,
        const std::vector<juce::String>& styleTags = {}
    );
    
    // 波表生成
    juce::AudioSampleBuffer generateWavetable(
        const juce::String& description,
        int numFrames = 256,
        int frameSize = 2048
    );
    
    // 频谱分析
    std::vector<float> analyzeReferenceSpectrum(
        const juce::AudioSampleBuffer& audio
    );
    
    // 音频参考嵌入
    std::vector<float> extractAudioEmbedding(
        const juce::AudioSampleBuffer& audio
    );
    
    // 解释生成
    juce::String generateParameterExplanation(
        const juce::String& parameterName,
        float currentValue,
        const juce::String& contextPrompt
    );
    
    // 性能
    double getLastInferenceTimeMs() const;
    size_t getModelMemoryUsage() const;
    
private:
    Ort::Env ortEnv_;
    std::unique_ptr<Ort::Session> session_;
    std::vector<const char*> inputNames_;
    std::vector<const char*> outputNames_;
    
    // 标记器
    std::unique_ptr<TextTokenizer> tokenizer_;
    
    // 参数空间映射
    ParameterSpaceMapper paramMapper_;
    
    // 推理缓存
    std::unordered_map<std::string, GenerationResult> resultCache_;
    static constexpr int kMaxCacheSize = 128;
};
```

### 3.5 参数树 (ParameterTree)

```cpp
class LianCoreParameterTree {
public:
    // 参数注册
    void registerParameter(const juce::String& id, 
                          const juce::String& name,
                          float defaultValue, 
                          float minValue, 
                          float maxValue,
                          float step = 0.0f,
                          const juce::String& unit = "");
    
    // 参数分组
    void beginGroup(const juce::String& groupId, const juce::String& groupName);
    void endGroup();
    
    // 参数访问
    float getParameterValue(const juce::String& id) const;
    void setParameterValue(const juce::String& id, float value);
    juce::RangedAudioParameter& getParameter(const juce::String& id);
    
    // 批量更新（AI推理结果）
    void applyParameterBatch(const std::vector<ParameterMapping>& mappings);
    
    // 预设管理
    void savePreset(const juce::String& name, const juce::String& category);
    void loadPreset(int presetId);
    juce::var getPresetAsJson() const;
    void restorePresetFromJson(const juce::var& json);
    
    // 版本历史（最多50步回退）
    void pushUndoState();
    bool undo();
    bool redo();
    int getUndoDepth() const;
    int getRedoDepth() const;
    
    // 通知
    void addListener(ParameterTreeListener* listener);
    void removeListener(ParameterTreeListener* listener);
    
private:
    juce::AudioProcessorValueTreeState state_;
    std::vector<juce::var> undoStack_;
    std::vector<juce::var> redoStack_;
    static constexpr int kMaxUndoSteps = 50;
    
    // 参数元数据（用于AI解释）
    struct ParameterMeta {
        juce::String id;
        juce::String name;
        juce::String description;
        juce::String category;
        juce::String unit;
        float defaultValue;
        float minValue;
        float maxValue;
    };
    std::vector<ParameterMeta> parameterMeta_;
};
```

### 3.6 UI 层接口

#### 3.6.1 前后端通信协议

```typescript
// WebSocket 消息格式
interface LianCoreMessage {
  type: MessageType;
  payload: any;
  timestamp: number;
  requestId?: string;
}

enum MessageType {
  // 参数同步
  PARAMETER_CHANGE = 'param_change',
  PARAMETER_BATCH = 'param_batch',
  PARAMETER_TREE_SYNC = 'param_tree_sync',
  
  // AI 推理
  AI_GENERATE_REQUEST = 'ai_generate',
  AI_GENERATE_RESULT = 'ai_generate_result',
  AI_GENERATE_PROGRESS = 'ai_generate_progress',
  AI_EXPLANATION = 'ai_explanation',
  
  // 预设管理
  PRESET_LIST = 'preset_list',
  PRESET_LOAD = 'preset_load',
  PRESET_SAVE = 'preset_save',
  PRESET_SEARCH = 'preset_search',
  
  // 音频图
  GRAPH_UPDATE = 'graph_update',
  GRAPH_ADD_NODE = 'graph_add_node',
  GRAPH_REMOVE_NODE = 'graph_remove_node',
  GRAPH_CONNECT = 'graph_connect',
  
  // 可视化数据
  OSCILLOSCOPE_DATA = 'oscilloscope_data',
  FFT_DATA = 'fft_data',
  LFO_VISUAL = 'lfo_visual',
  WAVETABLE_VISUAL = 'wavetable_visual',
  
  // 状态
  CPU_USAGE = 'cpu_usage',
  MEMORY_USAGE = 'memory_usage',
  LATENCY_REPORT = 'latency_report',
  
  // 系统
  ERROR = 'error',
  HEARTBEAT = 'heartbeat',
  LANGUAGE_CHANGE = 'language_change',
}
```

#### 3.6.2 React 组件树

```
<App>
  <TopBar>
    <AIGenerationBar />          {/* AI生成栏 - 固定顶部 */}
      <TextInput />
      <VoiceInput />             {/* 语音输入 */}
      <AudioReferenceUpload />
      <StyleTagSelector />
      <GenerateButton />
      <GenerationProgress />
    <PresetBrowser />
    <LanguageSelector />
    <ModeToggle />               {/* 极简↔专业切换 */}
  </TopBar>
  
  <MainContent>
    <MinimalMode>                {/* 极简模式 */}
      <EmotionSliders />         {/* 三颗情感滑块 */}
      <PresetGrid />
      <BasicControls />
    </MinimalMode>
    
    <ProfessionalMode>           {/* 专业模式 */}
      <SplitPane>
        <NodeGraph />            {/* 节点图 */}
        <PropertiesPanel>
          <ParameterControls />
          <WaveformVisualizer />  {/* 波形可视化 */}
          <FFTVisualizer />      {/* FFT可视化 */}
          <LFOVisualizer />      {/* LFO可视化 */}
          <ModulationMatrix />
        </PropertiesPanel>
      </SplitPane>
    </ProfessionalMode>
  </MainContent>
  
  <BottomBar>
    <CPUUsageIndicator />
    <MemoryUsageIndicator />
    <LatencyIndicator />
    <UndoRedoControls />
  </BottomBar>
</App>
```

### 3.7 数据层

#### 3.7.1 SQLite 预制库 Schema

```sql
-- 预制表
CREATE TABLE presets (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    category TEXT,
    tags TEXT,                    -- JSON数组
    description TEXT,
    author TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    json_data TEXT NOT NULL,      -- 完整参数JSON
    ai_prompt TEXT,               -- 如果由AI生成，记录提示词
    ai_confidence REAL,           -- AI置信度
    rating INTEGER DEFAULT 0,     -- 用户评分 1-5
    usage_count INTEGER DEFAULT 0
);

-- 标签表
CREATE TABLE tags (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT UNIQUE NOT NULL,
    category TEXT                 -- instrument, style, mood, etc.
);

-- 预制-标签关联
CREATE TABLE preset_tags (
    preset_id INTEGER,
    tag_id INTEGER,
    PRIMARY KEY (preset_id, tag_id),
    FOREIGN KEY (preset_id) REFERENCES presets(id),
    FOREIGN KEY (tag_id) REFERENCES tags(id)
);

-- 版本历史表
CREATE TABLE preset_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    preset_id INTEGER,
    version INTEGER,
    json_data TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (preset_id) REFERENCES presets(id)
);

-- 全文搜索索引
CREATE VIRTUAL TABLE presets_fts USING fts5(
    name, category, tags, description, ai_prompt
);
```

#### 3.7.2 文件系统布局

```
LianCore/
├── VST3/
│   └── LianCore.vst3/
│       ├── Contents/
│       │   ├── x86_64-win/
│       │   │   └── LianCore.vst3
│       │   ├── Resources/
│       │   │   ├── ui/                    # Web UI 资源
│       │   │   │   ├── index.html
│       │   │   │   ├── assets/
│       │   │   │   └── locales/           # 语言包
│       │   │   │       ├── zh-CN.json
│       │   │   │       ├── zh-TW.json
│       │   │   │       └── en.json
│       │   │   └── models/                # AI模型
│       │   │       ├── text_to_params.onnx
│       │   │       ├── wavetable_gen.onnx
│       │   │       └── audio_encoder.onnx
│       │   └── moduleinfo.json
│
├── Presets/                               # 默认预设库
│   ├── liancore_presets.db
│   ├── wavetables/
│   │   └── *.wav
│   └── samples/
│       └── *.wav
│
└── UserData/                              # 用户数据（可自定义路径）
    ├── user_presets.db
    ├── wavetables/
    ├── samples/
    └── config.json
```

---

## 4. 详细功能验收标准

### 4.1 音频图引擎验收

| 编号 | 验收项 | 验收标准 | 测试方法 |
|------|--------|---------|---------|
| AE-001 | 节点创建/删除 | 支持创建全部15种节点类型，删除后内存完全释放 | 创建100个节点后全部删除，检查内存是否恢复 |
| AE-002 | 节点连接/断开 | 支持任意两节点间连接，断开后信号链路中断 | 用测试信号验证连接有效性 |
| AE-003 | 拓扑排序 | DAG拓扑排序正确，信号处理顺序无环 | 构建复杂图（30+节点），验证处理顺序 |
| AE-004 | 实时处理 | processBlock()在48kHz/256采样块下 <1ms | 用计时器测量1000次调用平均值 |
| AE-005 | 序列化/反序列化 | JSON序列化后反序列化，还原100%参数 | 对比序列化前后所有参数值 |
| AE-006 | CPU占用 | 闲置<1%，普通音色<1.5% | 任务管理器+内部计时器双重测量 |
| AE-007 | 690轨压力测试 | 690实例叠加，不崩溃，每实例<8MB | 自动化脚本创建690轨，运行30分钟 |

### 4.2 波表引擎验收 (对标 Serum 2)

| 编号 | 验收项 | 验收标准 | 测试方法 |
|------|--------|---------|---------|
| WT-001 | 波表加载 | 支持WAV格式，256帧2048采样，加载<50ms | 加载100个不同波表文件 |
| WT-002 | 波表导出 | 导出为WAV，与原文件逐采样对比一致 | 加载→导出→重新加载→对比 |
| WT-003 | 双波表混合 | A/B波表0-100%平滑混合，无相位跳变 | 录制混合过程音频，分析波形连续性 |
| WT-004 | 帧间插值 | 256帧间线性/三次插值，无混叠 | 频谱分析仪检测混叠产物 |
| WT-005 | Unison | 支持1-16复音，detune范围±50cents | 频谱仪验证谐波结构 |
| WT-006 | FM/AM/RM/Sync | 四种Bend模式，参数范围0-100%，线性响应 | 输入正弦波验证调制效果 |
| WT-007 | AI波表生成 | 输入文本描述，输出256帧波表，<500ms | 测试50个不同描述，人工评估 |
| WT-008 | 实时Morphing | 两波表间实时变形，无音频断裂 | 录制30秒连续变形，检查零交叉点 |

### 4.3 频谱引擎验收 (对标 Vital)

| 编号 | 验收项 | 验收标准 | 测试方法 |
|------|--------|---------|---------|
| SP-001 | FFT重合成 | 256-4096点FFT，重叠2x-8x，延迟<3ms | 输入正弦波，检查输出频谱纯度 |
| SP-002 | 频谱拉伸 | 0.5x-4x拉伸，音高不变，谐波结构改变 | 频谱图对比拉伸前后 |
| SP-003 | 频谱移调 | ±24半音，谐波保持 | 钢琴音阶测试，每半音验证 |
| SP-004 | 谐波混合 | 0-100%混合，线性过渡 | 双音色混合，频谱平滑过渡 |
| SP-005 | AI频谱映射 | 输入参考音频，输出谐波参数，<200ms | 50个参考音频，对比映射结果 |
| SP-006 | 共振峰移调 | ±12半音，保持共振峰 | 元音音频测试，人耳判别 |

### 4.4 粒子/波导引擎验收 (对标 Absynth)

| 编号 | 验收项 | 验收标准 | 测试方法 |
|------|--------|---------|---------|
| GR-001 | 粒子大小 | 1-500ms可调，精度1ms | 录制输出，测量粒子持续时间 |
| GR-002 | 粒子密度 | 1-200粒/秒，最大512并发 | 在200粒/秒下验证无丢粒 |
| GR-003 | 粒子位置 | 0-100%归一化位置，随机偏移±50% | 固定位置录制，验证时间对齐 |
| GR-004 | 粒子包络 | 支持5种包络类型，平滑起止 | 单粒子录制，测量ADSR |
| GR-005 | Karplus-Strong | 频率20Hz-8kHz，延迟线<1秒 | 频率计测量输出音高 |
| GR-006 | 弦参数 | 张力/非线性/拾音位置，实时响应 | 连续扫参录制，分析参数-音色关系 |
| GR-007 | AI降噪 | 输入含噪采样，输出降噪后，SNR提升>10dB | 对比降噪前后频谱 |
| GR-008 | AI密度控制 | 基于音频内容动态调整密度 | 不同内容音频测试密度变化 |

### 4.5 混合引擎验收 (对标 VPS Avenger 2)

| 编号 | 验收项 | 验收标准 | 测试方法 |
|------|--------|---------|---------|
| HY-001 | 多采样映射 | 支持128个采样区域，键位/力度映射 | MIDI键盘全音域测试 |
| HY-002 | 鼓切片检测 | 自动检测瞬态切片，阈值可调 | 100个鼓循环测试检测精度 |
| HY-003 | 切片触发 | 16个切片同时触发，无相位问题 | 触发所有切片，检查叠加输出 |
| HY-004 | 步进音序器 | 1-64步，每步值/门限/tie独立 | 64步循环录制，验证每步 |
| HY-005 | AI自动映射 | 多采样自动检测音高并映射 | 50组多采样测试映射正确率 |
| HY-006 | .avpr转换 | 读取Avenger扩展包，转换参数 | 测试5个已知参数.avpr文件 |

### 4.6 调制矩阵验收

| 编号 | 验收项 | 验收标准 | 测试方法 |
|------|--------|---------|---------|
| MM-001 | 调制源注册 | 支持无限调制源注册 | 注册1000个源，无性能下降 |
| MM-002 | 调制目标注册 | 支持无限调制目标注册 | 注册1000个目标，无性能下降 |
| MM-003 | 调制路由 | 每采样计算，延迟<0.1ms | 100条路由同时计算 |
| MM-004 | 调制精度 | 调制值精度0.001，无累积误差 | 长时间运行（10分钟），检查漂移 |
| MM-005 | 双向调制 | 支持bipolar调制源 | ±1范围调制，验证负值效果 |

### 4.7 AI 推理层验收

| 编号 | 验收项 | 验收标准 | 测试方法 |
|------|--------|---------|---------|
| AI-001 | 模型加载 | ONNX模型≤200MB，加载<1s | 文件大小检查+加载时间测量 |
| AI-002 | CPU推理延迟 | i3-4130上<8ms | 1000次推理取P99延迟 |
| AI-003 | 文本→参数 | 输入文本，输出100%映射到参数树 | 100个提示词测试，检查参数有效性 |
| AI-004 | 文本+音频 | 同时输入文本和音频，融合推理 | 50组测试，人工评估音色准确性 |
| AI-005 | 文本+风格 | 风格标签影响输出 | 同一文本不同标签，输出不同音色 |
| AI-006 | 波表生成 | 文本描述生成256帧波表，<500ms | 50个描述测试波表可用性 |
| AI-007 | 参数解释 | 悬停参数时显示AI生成原因 | 每个参数验证解释文本合理性 |
| AI-008 | 无版权 | 训练数据均为无版权素材 | 数据集审计报告 |

### 4.8 UI/UX 验收

| 编号 | 验收项 | 验收标准 | 测试方法 |
|------|--------|---------|---------|
| UI-001 | 极简模式 | AI输入栏+预制浏览器+三颗情感滑块 | 手动测试，检查UI完整性 |
| UI-002 | 专业模式 | 节点图+参数面板+可视化 | 手动测试+截图对比 |
| UI-003 | 模式切换 | 极简↔专业切换<500ms，无闪烁 | 计时器+屏幕录制 |
| UI-004 | 语音输入 | 调用系统API，识别准确率>90% | 50句测试语音 |
| UI-005 | 波形可视化 | 实时波形，刷新率>30fps | 帧率计数器 |
| UI-006 | FFT可视化 | 实时频谱，刷新率>30fps | 帧率计数器 |
| UI-007 | LFO可视化 | 实时LFO曲线，刷新率>60fps | 帧率计数器 |
| UI-008 | AI生成栏 | 固定顶部，始终可见 | 滚动测试 |
| UI-009 | 预制浏览器 | 支持标签搜索，结果<200ms | 搜索10万条记录测试 |
| UI-010 | 版本历史 | 最多50步回退，撤销<100ms | 连续撤销50次测试 |

### 4.9 多语言验收

| 编号 | 验收项 | 验收标准 | 测试方法 |
|------|--------|---------|---------|
| ML-001 | 中文简体 | 100%翻译覆盖 | 遍历所有UI文本 |
| ML-002 | 中文繁体 | 100%翻译覆盖 | 遍历所有UI文本 |
| ML-003 | 英文 | 100%翻译覆盖 | 遍历所有UI文本 |
| ML-004 | 热切换 | 切换语言不重载核心，零崩溃 | 100次切换压力测试 |
| ML-005 | JSON格式 | 语言包为JSON，格式统一 | 验证JSON schema |

### 4.10 安装部署验收

| 编号 | 验收项 | 验收标准 | 测试方法 |
|------|--------|---------|---------|
| DP-001 | CMake构建 | 一键构建，支持Debug/Release | 多平台构建测试 |
| DP-002 | CPack打包 | 生成NSIS安装包 | 安装包验证 |
| DP-003 | VST3路径 | 正确安装至Common Files\VST3 | 检查安装路径 |
| DP-004 | VC++运行库 | 自动检测/安装VC++运行库 | 干净Windows 10测试 |
| DP-005 | 预设路径 | 支持自定义预设库路径 | 修改路径后重启验证 |
| DP-006 | DAW识别 | 主流DAW（Ableton/FL/Cubase/Logic）正确识别 | 5个DAW扫描测试 |

### 4.11 MIDI/MPE 验收

| 编号 | 验收项 | 验收标准 | 测试方法 |
|------|--------|---------|---------|
| MD-001 | MIDI键盘 | 标准MIDI键盘全音域响应 | 88键逐一测试 |
| MD-002 | MPE | 支持MPE复音触后/弯音/滑音 | MPE控制器测试 |
| MD-003 | 延迟 | 按键到发声<5ms | 音频测量设备 |
| MD-004 | 钢琴卷帘 | DAW钢琴卷帘音符同步 | 录制MIDI回放验证 |

### 4.12 性能与稳定性验收

| 编号 | 验收项 | 验收标准 | 测试方法 |
|------|--------|---------|---------|
| PF-001 | 启动时间 | 冷启动≤2.5s | 10次启动取平均值 |
| PF-002 | 音色加载 | 任意音色≤0.8s | 100个不同音色测试 |
| PF-003 | 内存不足 | 8GB RAM下自动切换流模式 | 限制内存测试 |
| PF-004 | 长期运行 | 24小时连续运行不崩溃 | 自动化测试+监控 |
| PF-005 | 热插拔 | DAW中热插拔不崩溃 | 100次插拔测试 |
| PF-006 | 采样率切换 | 44.1k/48k/96k/192k自动适配 | 每种采样率测试 |

---

## 5. Alpha 阶段开发任务清单

### 5.1 任务总览

| 阶段 | 任务数 | 预计工时 | 关键交付物 |
|------|--------|---------|-----------|
| 准备工作 | 5 | 1周 | 项目骨架、CMake、CI/CD |
| 音频核心 | 8 | 3周 | 音频图引擎、节点系统 |
| 合成引擎 | 10 | 3周 | 波表振荡器、虚拟模拟、滤波器 |
| VST3封装 | 4 | 1周 | VST3插件、DAW兼容 |
| **总计** | **27** | **8周** | **VST3原型（无UI）** |

### 5.2 详细任务清单

#### 第1周：准备工作

| 任务ID | 任务名称 | 描述 | 预估工时 | 依赖 |
|--------|---------|------|---------|------|
| SETUP-001 | 项目骨架搭建 | CMake项目结构、JUCE集成、目录结构 | 4h | - |
| SETUP-002 | 构建系统配置 | CMake+CPack配置，NSIS打包脚本 | 4h | SETUP-001 |
| SETUP-003 | CI/CD流水线 | GitHub Actions，自动构建+测试 | 4h | SETUP-002 |
| SETUP-004 | 音频工具类 | 基础音频缓冲区、SIMD封装、数学工具 | 6h | SETUP-001 |
| SETUP-005 | 测试框架 | Catch2集成，Mock音频接口 | 4h | SETUP-001 |

#### 第2-3周：音频图引擎

| 任务ID | 任务名称 | 描述 | 预估工时 | 依赖 |
|--------|---------|------|---------|------|
| AGRAPH-001 | AudioNode基类 | 纯虚接口、端口管理、生命周期 | 6h | SETUP-004 |
| AGRAPH-002 | AudioGraphEngine | 节点管理、连接管理、拓扑排序 | 10h | AGRAPH-001 |
| AGRAPH-003 | 音频输出节点 | 汇总输出、音量控制、静音 | 4h | AGRAPH-002 |
| AGRAPH-004 | 混合器节点 | 多路输入混合、增益控制 | 4h | AGRAPH-002 |
| AGRAPH-005 | 序列化系统 | 图结构JSON序列化/反序列化 | 6h | AGRAPH-003 |
| AGRAPH-006 | 性能监控 | CPU占用测量、内存跟踪 | 4h | AGRAPH-002 |
| AGRAPH-007 | 测试：节点系统 | 100节点创建/删除/连接测试 | 6h | AGRAPH-005 |
| AGRAPH-008 | 测试：拓扑排序 | 30+节点复杂图排序验证 | 4h | AGRAPH-005 |

#### 第4-5周：合成引擎

| 任务ID | 任务名称 | 描述 | 预估工时 | 依赖 |
|--------|---------|------|---------|------|
| SYNTH-001 | WavetableBank | 波表数据管理、256帧存储、WAV导入 | 8h | AGRAPH-002 |
| SYNTH-002 | WavetableOscillator | 双波表振荡器、帧插值、Unison | 12h | SYNTH-001 |
| SYNTH-003 | 虚拟模拟振荡器 | 锯齿/方波/三角/正弦，抗混叠 | 6h | AGRAPH-002 |
| SYNTH-004 | 噪声发生器 | 白/粉/棕色噪声，采样保持 | 3h | AGRAPH-002 |
| SYNTH-005 | 滤波器 | 多模式滤波器（LP/HP/BP/BR/Peak） | 10h | AGRAPH-002 |
| SYNTH-006 | 包络发生器 | ADSR包络，对数/线性曲线 | 6h | AGRAPH-002 |
| SYNTH-007 | LFO | 多波形LFO，频率0.01-100Hz，Tempo同步 | 6h | AGRAPH-002 |
| SYNTH-008 | 测试：波表引擎 | 波表加载/播放/混合测试 | 6h | SYNTH-002 |
| SYNTH-009 | 测试：滤波器 | 频率响应、共振测试 | 4h | SYNTH-005 |
| SYNTH-010 | 测试：调制器 | LFO/包络精度测试 | 4h | SYNTH-007 |

#### 第6-7周：调制矩阵与参数系统

| 任务ID | 任务名称 | 描述 | 预估工时 | 依赖 |
|--------|---------|------|---------|------|
| MOD-001 | ModulationSource | 调制源接口实现 | 4h | SYNTH-007 |
| MOD-002 | ModulationTarget | 调制目标接口实现 | 4h | SYNTH-002 |
| MOD-003 | ModulationMatrix | 路由管理、每采样计算 | 8h | MOD-001, MOD-002 |
| MOD-004 | ParameterTree | 参数注册、分组、批量更新 | 10h | AGRAPH-002 |
| MOD-005 | 预设系统 | 预设保存/加载、版本历史 | 8h | MOD-004 |
| MOD-006 | 测试：调制矩阵 | 100路由性能测试 | 4h | MOD-003 |
| MOD-007 | 测试：参数系统 | 参数批量更新、撤销/重做测试 | 4h | MOD-005 |

#### 第8周：VST3封装与集成

| 任务ID | 任务名称 | 描述 | 预估工时 | 依赖 |
|--------|---------|------|---------|------|
| VST3-001 | AudioProcessor | VST3处理器封装、MIDI处理 | 8h | MOD-004 |
| VST3-002 | AudioProcessorEditor | 基础编辑器（无UI） | 4h | VST3-001 |
| VST3-003 | DAW兼容测试 | 主流DAW识别和加载测试 | 6h | VST3-002 |
| VST3-004 | 打包测试 | NSIS安装包生成、安装验证 | 4h | VST3-003 |

### 5.3 Alpha 阶段验收标准

- [ ] 双波表振荡器能产生可听声音
- [ ] 虚拟模拟振荡器（锯齿/方波/三角/正弦）正常工作
- [ ] 多模式滤波器（LP/HP/BP/BR/Peak）正常工作
- [ ] ADSR包络和LFO能调制目标参数
- [ ] 调制矩阵支持至少10条同时路由
- [ ] 参数树支持预设保存/加载
- [ ] VST3插件在Ableton Live/FL Studio/Cubase中正确识别和加载
- [ ] 所有27个单元测试通过
- [ ] 闲置CPU<1%，普通音色<1.5%
- [ ] 安装包在干净Windows 10/11上成功安装

---

## 6. 技术栈与依赖

### 6.1 核心技术

| 层级 | 技术 | 版本 | 用途 |
|------|------|------|------|
| 音频框架 | JUCE | 8.x | VST3/AAX/AU框架、音频处理 |
| 构建系统 | CMake | 3.24+ | 跨平台构建 |
| 打包工具 | CPack + NSIS | 3.30+ | Windows安装包 |
| AI推理 | ONNX Runtime | 1.18+ | 端侧模型推理 |
| 数据库 | SQLite | 3.45+ | 预制库存储 |
| UI框架 | React | 18.x | 用户界面 |
| 可视化 | WebGL + Canvas | - | 波形/频谱可视化 |
| 嵌入浏览器 | CEF (Chromium Embedded Framework) | 120+ | Web UI嵌入 |
| 测试框架 | Catch2 | 3.x | C++单元测试 |
| Web测试 | Playwright | 1.x | UI自动化测试 |

### 6.2 编译器要求

| 平台 | 编译器 | 最低版本 |
|------|--------|---------|
| Windows | MSVC | Visual Studio 2022 (17.8+) |
| macOS | Clang | Xcode 15.0+ |
| 通用 | GCC | 13.0+ (Linux) |

### 6.3 运行时依赖

| 依赖 | 来源 | 说明 |
|------|------|------|
| VC++ Redistributable | Microsoft | Windows运行库 |
| ASIO SDK | Steinberg | 低延迟音频驱动 |
| VST3 SDK | Steinberg | 插件格式 |

---

## 7. 技能使用计划

### 7.1 开发阶段技能映射

| 开发阶段 | 使用技能 | 触发时机 |
|---------|---------|---------|
| **需求细化** | Spec 技能 | 项目启动时，生成详细技术规格 |
| **制定计划** | Plan 技能 | 需求确认后，产出分步执行计划 |
| **竞品研究** | agent-browser + research-documentation | 设计阶段，搜索竞品架构 |
| **创意讨论** | brainstorming | 调制矩阵设计、AI训练策略 |
| **UI原型** | frontend-design | 极简/专业模式HTML原型 |
| **品牌设计** | canvas-design | Logo与视觉方案 |
| **可视化组件** | algorithmic-art | 波表/频谱p5.js示例 |
| **C++核心编码** | test-driven-development | 编写测试 → 实现代码 |
| **框架搭建** | executing-plans + git-commit | 按阶段执行，规范提交 |
| **AI模型训练** | data-analysis | 数据集分析、Python预处理 |
| **UI层开发** | composition-patterns + electron | React组件设计、UI测试 |
| **自动测试** | webapp-testing + dogfood | Playwright测试、DAW问题记录 |
| **性能监控** | screenshot | 任务管理器数据截图 |
| **安全审查** | security-best-practices | 加密与代码漏洞检查 |
| **文档撰写** | doc-coauthoring + writing-plans | 技术文档、打包脚本 |
| **测试报告** | report-generator | 整合测试报告 |
| **知识管理** | knowledge-capture / notion-cli | 存储设计决策 |
| **汇报** | slides | 制作PPT |
| **版本控制** | gh-cli + git-commit | 仓库管理、智能提交 |
| **代码优化** | react-best-practices + redis-development | 性能优化 |

### 7.2 Alpha 阶段技能使用时序

```
Week 1:  Spec → Plan → brainstorming → agent-browser
Week 2:  test-driven-development → executing-plans → git-commit
Week 3:  test-driven-development → executing-plans → git-commit
Week 4:  test-driven-development → algorithmic-art → git-commit
Week 5:  test-driven-development → executing-plans → git-commit
Week 6:  test-driven-development → executing-plans → git-commit
Week 7:  test-driven-development → executing-plans → git-commit
Week 8:  webapp-testing → screenshot → report-generator → slides
```

---

## 附录

### A. 术语表

| 术语 | 英文 | 说明 |
|------|------|------|
| 波表 | Wavetable | 多个单周期波形组成的表，用于合成 |
| 频谱振荡器 | Spectral Oscillator | 基于FFT重合成的振荡器 |
| 粒子合成 | Granular Synthesis | 将音频分割为微小粒子重组 |
| Karplus-Strong | Karplus-Strong | 物理建模弦乐合成算法 |
| MPE | MIDI Polyphonic Expression | MIDI复音表情 |
| ONNX | Open Neural Network Exchange | 开放神经网络交换格式 |

### B. 参考文档

- JUCE Framework Documentation: https://juce.com/learn/
- VST3 SDK Documentation: https://steinbergmedia.github.io/vst3_doc/
- ONNX Runtime Documentation: https://onnxruntime.ai/docs/
- Steinberg ASIO SDK: https://www.steinberg.net/developers/