# LianCore 核心引擎技术文档

> **版本**: V3 Beta | **更新**: 2026-07-12 | **仓库**: github.com/Lianyilin-rgb/liancore-synth
>
> 本文档面向合成器开发者、插件集成工程师和音频技术研究者，详细描述 LianCore 的核心引擎架构、节点系统、音频处理管线、调制矩阵和参数系统。

---

## 目录

1. [架构概览](#1-架构概览)
2. [节点式音频图引擎](#2-节点式音频图引擎)
3. [合成引擎模块](#3-合成引擎模块)
4. [效果器链](#4-效果器链)
5. [调制矩阵](#5-调制矩阵)
6. [参数系统](#6-参数系统)
7. [SIMD 加速与性能优化](#7-simd-加速与性能优化)
8. [序列化与兼容性](#8-序列化与兼容性)

---

## 1. 架构概览

### 1.1 系统分层

```
┌─────────────────────────────────────────────────┐
│                  Plugin Layer                    │
│  PluginProcessor / PluginEditor / Web UI         │
├─────────────────────────────────────────────────┤
│                  AI Inference Layer              │
│  AIInferenceEngine / AIModelTrainer / Tokenizer  │
├─────────────────────────────────────────────────┤
│                  Parameter Layer                 │
│  ParameterTree / PresetManager / ParameterMapper │
├─────────────────────────────────────────────────┤
│                  Modulation Layer                │
│  ModulationMatrix / EnvelopeGenerator / LFO      │
├─────────────────────────────────────────────────┤
│                  Synthesis Layer                 │
│  6 Oscillators / Filter / 5 Effects              │
├─────────────────────────────────────────────────┤
│                  Core Layer                      │
│  AudioGraphEngine / AudioNode / NodeFactory      │
└─────────────────────────────────────────────────┘
```

### 1.2 技术栈

| 组件 | 技术选型 | 版本 |
|------|---------|------|
| 插件框架 | JUCE | 8.0.6 |
| 插件格式 | VST3 / AU / AAX | - |
| 构建系统 | CMake | 3.24+ |
| C++ 标准 | C++17 | - |
| SIMD | AVX2 指令集 | - |
| 测试框架 | Catch2 | 3.x |
| 数据库 | SQLite3 | 3.x |
| AI 推理 | ONNX Runtime | 1.18.1 |
| Web UI | React 18 + TypeScript + Vite | - |
| 打包 | CPack + NSIS (Win) / DMG (Mac) | - |

### 1.3 设计决策

**决策 1: 节点式架构 (AudioNode)**
- 选择: 基于 `AudioNode` 基类的节点图架构，所有合成器和效果器继承同一基类
- 理由: 统一生命周期管理、端口连接、序列化，支持用户自定义信号链
- 替代方案: 固定信号链 (不够灵活) / 模块化插件 (复杂度高)

**决策 2: Kahn 拓扑排序 (AudioGraphEngine)**
- 选择: Kahn 算法进行节点执行顺序排序
- 理由: O(V+E) 复杂度，支持循环检测，适合实时音频处理
- 替代方案: DFS 拓扑排序 (递归深度可能超限)

**决策 3: 矩阵路由表 (ModulationMatrix)**
- 选择: 32 槽位的矩阵路由表，每个槽位包含源/目标/深度
- 理由: 灵活的路由配置，支持一对多/多对一调制，易于序列化
- 替代方案: 全连接调制矩阵 (内存开销大)

**决策 4: ONNX Runtime + 规则引擎回退 (AIInferenceEngine)**
- 选择: 优先使用 ONNX Runtime 推理，不可用时回退到关键词规则引擎
- 理由: 兼容无 ONNX 环境，保证基础 AI 功能可用
- 替代方案: 纯规则引擎 (准确度低) / 强制 ONNX 依赖 (部署复杂)

**决策 5: 转置直接 II 型 (Biquad 滤波器)**
- 选择: 所有滤波器使用转置直接 II 型结构
- 理由: 数值稳定性好，状态变量少，适合实时音频处理
- 替代方案: 直接 I 型 (数值不稳定) / 级联 II 型 (复杂度高)

---

## 2. 节点式音频图引擎

### 2.1 AudioNode 基类

所有音频处理模块的抽象基类，定义统一接口：

```cpp
class AudioNode {
    NodeId id_;                    // 唯一标识符
    NodeType type_;                // 节点类型枚举 (20种)
    juce::String name_;            // 用户可编辑名称
    std::vector<AudioPort> inputs_;  // 输入端口
    std::vector<AudioPort> outputs_; // 输出端口

    // 生命周期
    virtual void prepareToPlay(double sr, int blockSize);
    virtual void processBlock(AudioBuffer<float>&, MidiBuffer&);
    virtual void releaseResources();

    // 参数接口
    virtual int getNumParameters() const;
    virtual float getParameter(int index) const;
    virtual void setParameter(int index, float value);

    // 序列化
    virtual var toJson() const;
    virtual void fromJson(const var& json);
};
```

### 2.2 AudioGraphEngine

节点图管理器，负责调度、连接和序列化：

| 功能 | 方法 | 说明 |
|------|------|------|
| 节点管理 | `addNode()` / `removeNode()` | 通过 NodeFactory 创建/销毁 |
| 连接管理 | `connect()` / `disconnect()` | 端口级连接，支持多对多 |
| 拓扑排序 | 内部 `topologicalSort()` | Kahn 算法，O(V+E) |
| 音频处理 | `processBlock()` | 按拓扑序依次执行各节点 |
| 序列化 | `toJson()` / `fromJson()` | 完整图状态保存/恢复 |

**拓扑排序流程**:
```
1. 计算所有节点的入度
2. 将入度=0的节点加入队列
3. 弹出节点，将其输出连接到目标节点的入度-1
4. 重复直到队列为空
5. 若结果节点数 ≠ 总节点数 → 存在环，拒绝
```

### 2.3 NodeFactory

工厂模式创建节点，自动配置默认端口：

| 节点类型 | 输入端口 | 输出端口 |
|----------|---------|---------|
| 振荡器/合成器 | 0 | 1 音频 |
| 效果器 | 1 音频 | 1 音频 |
| 调制器 | 0 | 1 控制 |
| 步进音序器 | 0 | 1 控制 |
| 混音器 | 4 音频 | 1 音频 |
| 音频输出 | 1 音频 | 0 |

### 2.4 20 种节点类型

| 类别 | 节点 | 对标 |
|------|------|------|
| **振荡器** | WavetableOscillator, VirtualAnalogOscillator, NoiseGenerator, SpectralOscillator, GranularPlayer, WaveguideResonator | Vital / Absynth |
| **采样** | MultiSampler, DrumSlicer | VPS Avenger 2 |
| **音序器** | StepSequencer | VPS Avenger 2 |
| **滤波器** | FilterProcessor | - |
| **效果器** | Distortion, Delay, Reverb, Compressor, EQ | Soundtoys / Valhalla / FabFilter |
| **调制器** | EnvelopeGenerator, LFOGenerator | - |
| **路由** | Mixer, AudioOutput, Splitter, AudioInput | - |

---

## 3. 合成引擎模块

### 3.1 WavetableOscillator (双波表振荡器)

| 特性 | 规格 |
|------|------|
| 波表帧数 | 256 帧 × 2048 采样 |
| 插值 | 三次 Hermite 插值 |
| Unison | 16 声部，可调 Detune/Spread |
| 调制模式 | FM / AM / RM / Sync Bend |
| 波表导入 | WAV 文件导入/导出 |
| AI 生成 | 文本描述→波表生成 |

### 3.2 VirtualAnalogOscillator (虚拟模拟振荡器)

- 5 种波形: 正弦、三角、锯齿、方波、脉冲 (PWM)
- 抗混叠: 多项式修正 (5阶 Taylor 逼近)
- 特点: 无波表存储开销，CPU 极低

### 3.3 SpectralOscillator (频谱振荡器)

- FFT 大小: 256-4096，支持 2/4/8x 重叠
- 频谱拉伸: 0.5x - 4.0x
- 频谱移调: ±24 半音
- 谐波混合: 与 AI 谐波模板混合
- 共振峰滤波: A/E/I/O/U 5 种元音预设
- 重叠-相加: 平滑重合成，无相位断裂

### 3.4 GranularPlayer (粒子播放器)

- 粒子池: 512 个粒子
- 粒子大小: 1-500ms
- 粒子密度: 1-200 粒/秒
- 包络类型: Hann / Hamming / Triangle / Exponential / Rectangular
- 源音频: WAV 加载，最大 480000 采样

### 3.5 WaveguideResonator (波导谐振器)

- 算法: 改进 Karplus-Strong 物理模型
- 激励: 噪声 / 脉冲 / 采样
- 物理参数: 张力 / 非谐波 / 拾音位置
- 体共振: 二倍频谐振器

### 3.6 MultiSampler (多采样器)

- 采样区域: 128 个，支持键位/力度映射
- 复音: 32 声部
- 循环: 无循环 / 正向 / 乒乓
- AI 自动映射: 从文件名检测根音，自动分配键位

### 3.7 DrumSlicer (鼓切片器)

- 切片数: 64 个
- 瞬态检测: 振幅阈值 + 256 样本窗口
- MIDI 映射: 自动从 C4 递增

### 3.8 StepSequencer (步进音序器)

- 步数: 1-64 步
- 门控/连音: 独立控制每步
- 摆动: 0-100% (偶数步延长)
- 方向: 正向 / 反向 / 随机
- 输出平滑: 10ms 指数平滑

---

## 4. 效果器链

### 4.1 Distortion (失真)

| 类型 | 算法 | 特性 |
|------|------|------|
| SoftClip | `tanh(drive * x)` | 平滑饱和，偶次谐波 |
| HardClip | `clamp(x, -1, 1)` | 硬限幅，奇次谐波 |
| Tube | `asymmetric tanh + bias` | 管味，偶次谐波增强 |
| Fuzz | `80x gain → hardclip` | 法兹，极高增益 |
| Bitcrush | `round(x * 2^bits) / 2^bits + S&H` | 数字失真 |
| Foldback | `|x| > threshold → fold` | 波折返失真 |

### 4.2 Delay (立体声延迟)

- 延迟时间: 1ms - 2000ms
- 反馈: 0-95%，含 HP + LP 串联滤波
- Ping-Pong: 交叉声道反馈
- 节拍同步: 12 种音符时值 (1/1 ~ 1/32 + 附点/三连音)
- 平滑过渡: 20ms 一阶平滑

### 4.3 Reverb (Schroeder-Moorer 混响)

- 4 并行梳状滤波器 + 2 串联全通滤波器
- 梳状延迟: 29.7 / 37.1 / 41.1 / 43.7ms (质数)
- 全通延迟: 5.0 / 1.7ms
- RT60 精确: `feedback = 0.001^(delay/RT60)`
- 预延迟: 0-200ms
- 早期反射: 4 抽头

### 4.4 Compressor (RMS 压缩器)

- 检测: RMS 10ms 滑动窗口
- 立体声联动: 取 L/R 最大值
- 软拐点: 二次插值平滑过渡
- 压缩比: 1:1 - 20:1
- 增益补偿: 0-30dB

### 4.5 EQ (8 段参数均衡器)

- 7 种类型: Peaking / LowShelf / HighShelf / LowPass / HighPass / Notch / BandPass
- 系数计算: RBJ Audio EQ Cookbook 公式
- 默认配置: 80Hz 搁架 + 250/500/1k/2k/4k/8k 峰值 + 12kHz 搁架
- 串行处理: 8 段依次串联

---

## 5. 调制矩阵

### 5.1 ModulationMatrix

32 槽位矩阵路由表，每个槽位包含:

```cpp
struct ModulationSlot {
    ModulationSource* source;  // 调制源 (LFO, Envelope, Macro)
    ModulationTarget* target;  // 调制目标 (参数)
    float depth;               // 调制深度 (-1.0 to 1.0, bipolar)
    bool enabled;              // 是否启用
};
```

### 5.2 EnvelopeGenerator (ADSR 包络)

- 5 阶段状态机: Idle → Attack → Decay → Sustain → Release
- 力度灵敏度: 0-100%
- 参数: Attack (0.1ms-10s), Decay (0.1ms-10s), Sustain (0-1), Release (0.1ms-10s)

### 5.3 LFOGenerator

- 6 种波形: Sine / Triangle / Saw / Square / Random / S&H
- 3 种同步: Free / Tempo / KeyTrigger
- 速率: 0.01Hz - 100Hz (自由) / 1/32 - 1/1 (节拍)
- 平滑处理: 防止波形突变

---

## 6. 参数系统

### 6.1 ParameterTree

基于 JUCE `AudioProcessorValueTreeState`:
- 50 步撤销/重做
- 批量更新: `beginParameterChangeGesture()` / `endParameterChangeGesture()`
- 参数通知: `addParameterListener()`

### 6.2 PresetManager (SQLite 预设管理)

- 数据库: SQLite3，自动创建表和索引
- CRUD: 完整预设增删改查
- 版本历史: 预设修改历史追踪
- 标签搜索: 多标签 AND/OR 搜索
- 导入/导出: JSON 格式跨平台兼容

### 6.3 ParameterMapper (参数映射系统)

- **ParameterSnapshot**: 全参数快照，`morphTo()` 渐变过渡
- **ParameterAutomation**: 断点自动化，4 种曲线插值
- **MacroControl**: 8 宏旋钮，多参数映射，曲线塑形
- 回调解耦: 通过 `std::function` 与宿主参数系统隔离

---

## 7. SIMD 加速与性能优化

### 7.1 AVX2 指令集

| 函数 | 操作 | 加速比 |
|------|------|--------|
| `clearBufferSIMD()` | 8 × float 并行清零 | ~4x |
| `multiplyBufferSIMD()` | 8 × float 并行乘法 | ~4x |
| `mixBuffersSIMD()` | 8 × float 并行混合 | ~3x |

### 7.2 性能基准

| 操作 | 目标 | 实测 |
|------|------|------|
| 256 帧波表插值 | < 0.1ms | 待测 |
| 8 段 EQ 处理 | < 0.5ms | 待测 |
| Schroeder 混响 | < 1.0ms | 待测 |
| AI 推理 (规则引擎) | < 10ms | 实测 < 2ms |

---

## 8. 序列化与兼容性

### 8.1 JSON 序列化格式

所有节点和参数支持完整的 JSON 往返序列化:

```json
{
  "version": "3.0-beta",
  "nodes": [
    {
      "id": "node_001",
      "type": "WavetableOscillator",
      "name": "OSC 1",
      "params": { "frequency": 0.5, "volume": 0.8 }
    }
  ],
  "connections": [
    { "src": "node_001", "srcPort": 0, "dst": "node_002", "dstPort": 0 }
  ]
}
```

### 8.2 版本兼容策略

- 向前兼容: 新版本可读取旧版本预设
- 版本标记: 所有 JSON 包含 `version` 字段
- 迁移函数: 旧参数名→新参数名自动映射

---

> **文档维护**: 本文档随代码同步更新。每次架构变更后请更新对应章节。
> **相关文档**: [AI 模型接口文档](./LianCore_AI_Model_API_Documentation.md)