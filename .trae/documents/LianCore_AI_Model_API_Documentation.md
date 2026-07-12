# LianCore AI 模型接口文档

> **版本**: V3 Beta | **更新**: 2026-07-12 | **仓库**: github.com/Lianyilin-rgb/liancore-synth
>
> 本文档面向 AI 模型开发者、集成工程师和音频研究人员，详细描述 LianCore 的 AI 推理引擎、模型训练管线、ONNX 导出格式和 API 接口。

---

## 目录

1. [AI 架构概览](#1-ai-架构概览)
2. [AIInferenceEngine API](#2-aiinferenceengine-api)
3. [AIModelTrainer API](#3-aimodeltrainer-api)
4. [ONNX 模型格式](#4-onnx-模型格式)
5. [训练数据格式](#5-训练数据格式)
6. [文本特征提取算法](#6-文本特征提取算法)
7. [参数空间映射](#7-参数空间映射)

---

## 1. AI 架构概览

```
┌──────────────────────────────────────────────────────┐
│                    用户输入                           │
│         文本描述 / 音频参考 / 风格标签                 │
├──────────────────────────────────────────────────────┤
│                    Tokenizer                         │
│             字符串 → 分词列表 → 编码                  │
├──────────────────────────────────────────────────────┤
│              AIInferenceEngine                       │
│  ┌─────────────────┐  ┌──────────────────────────┐  │
│  │  ONNX Runtime    │  │  规则引擎 (回退)          │  │
│  │  (优先)          │  │  30+ 关键词参数映射       │  │
│  └────────┬────────┘  └───────────┬──────────────┘  │
│           └───────────┬───────────┘                  │
│                       ▼                              │
│                GenerationResult                      │
│         参数列表 / 预设名称 / 置信度                   │
├──────────────────────────────────────────────────────┤
│              ParameterSpaceMapper                    │
│            嵌入向量 → 参数空间映射                    │
└──────────────────────────────────────────────────────┘
```

### 1.1 双引擎策略

| 引擎 | 条件 | 性能 | 适用场景 |
|------|------|------|----------|
| ONNX Runtime | 模型已加载 | 推理 < 10ms, 置信度 0.85+ | 精确音色生成 |
| 规则引擎 | 回退模式 | 推理 < 2ms, 置信度 0.6+ | 快速原型 / 提示 |

### 1.2 推理流程

```
1. 用户输入文本 (如 "温暖的模拟合成器铺底")
2. TextTokenizer 分词 → 编码
3. 检查 ONNX 模型是否加载
   ├─ 是 → 文本特征提取 → ONNX 推理 → 128维参数向量
   └─ 否 → 关键词匹配 → 累积参数映射
4. 参数向量 → ParameterSpaceMapper → 实际参数值
5. 返回 GenerationResult { parameters, presetName, confidence }
```

---

## 2. AIInferenceEngine API

### 2.1 类定义

```cpp
namespace LianCore {

class AIInferenceEngine {
public:
    static constexpr int kMaxCacheSize = 128;

    enum class GenerationMode {
        TextOnly,       // 纯文本描述
        TextWithAudio,  // 文本 + 音频参考
        TextWithStyle,  // 文本 + 风格标签
    };

    struct GenerationResult {
        std::vector<ParameterMapping> parameters;
        std::vector<float> explanationEmbeddings;
        std::vector<float> wavetableData;
        juce::String presetName;
        float confidence;
    };
};
```

### 2.2 模型管理

#### `loadModel(const juce::File& onnxFile) → bool`
加载 ONNX 模型文件。

**参数**:
- `onnxFile`: ONNX 模型文件路径 (.onnx)

**返回**: 加载成功返回 true

**行为**:
1. 卸载已有模型
2. 创建 ONNX Runtime Session
3. 提取输入/输出名称和形状
4. 设置模型内存使用量

**示例**:
```cpp
auto& ai = processor.getAIEngine();
ai.loadModel(File("C:/models/liancore_v1.onnx"));
```

#### `unloadModel()`
卸载当前模型，释放 ONNX Runtime 资源，清空推理缓存。

#### `isModelLoaded() → bool`
检查 ONNX 模型是否已加载。

#### `getModelInfo() → String`
返回当前模型信息字符串，如 "ONNX模型已加载 (12.5MB)" 或 "规则引擎模式"。

### 2.3 核心推理

#### `generateParameters(text, audioRef, styleTags) → GenerationResult`

主推理接口，文本→参数映射。

**参数**:
- `textPrompt`: 文本描述，如 "明亮的电子合成器主音"
- `audioReference`: 可选音频参考，用于频谱分析
- `styleTags`: 可选风格标签列表

**返回**:
```cpp
GenerationResult {
    .parameters = [
        { "filter_cutoff", 0.8, "关键词触发" },
        { "env_attack", 0.1, "关键词触发" },
        ...
    ],
    .presetName = "AI_明亮的电子合成器主音",
    .confidence = 0.85
}
```

**推理优先级**:
1. 检查缓存 (128条 LRU)
2. ONNX Runtime 推理 (置信度 > 0.5 时使用)
3. 规则引擎回退 (置信度 0.6)
4. 音频参考增强 (置信度 +0.1)

#### `generateWavetable(description, numFrames, frameSize) → AudioSampleBuffer`

文本描述→波表数据生成。

**参数**:
- `description`: 波形描述 ("锯齿" / "方波" / "三角" / "噪声")
- `numFrames`: 波表帧数 (默认 256)
- `frameSize`: 每帧采样数 (默认 2048)

**返回**: 1 通道 AudioSampleBuffer，大小 = numFrames × frameSize

#### `analyzeReferenceSpectrum(audio) → vector<float>`

FFT 频谱分析。

**算法**: 实输入 FFT → 幅度谱 → 返回半频谱

#### `extractAudioEmbedding(audio) → vector<float>`

音频嵌入提取，128 维向量。

**算法**: 频谱分析 → 降采样 128 维

#### `generateParameterExplanation(name, value, context) → String`

AI 生成参数调整解释文本。

**示例**:
```
输入: ("filter_cutoff", 0.8, "温暖")
输出: "AI自动提高截止频率以增加温暖"
```

### 2.4 性能指标

| 方法 | 获取方式 |
|------|---------|
| 推理时间 | `getLastInferenceTimeMs()` |
| 模型内存 | `getModelMemoryUsage()` |

---

## 3. AIModelTrainer API

### 3.1 数据结构

```cpp
struct TrainingData {
    std::vector<std::string> textPrompts;          // 文本提示词
    std::vector<std::vector<float>> parameterVectors;  // 128维参数向量
    std::vector<std::vector<float>> audioEmbeddings;   // 128维音频嵌入
    std::vector<std::string> styleTags;            // 风格标签

    void loadFromCSV(const juce::File& file);
    void saveToCSV(const juce::File& file);
    void addExample(text, params, embed, tags);
};

struct TrainingConfig {
    int epochs = 100;               // 训练轮数
    int batchSize = 32;             // 批量大小
    float learningRate = 0.001f;    // 学习率
    float validationSplit = 0.2f;   // 验证集比例
    int earlyStoppingPatience = 10; // 早停耐心值
    float l2Regularization = 0.0f;  // L2正则化
};

struct ModelMetrics {
    float mse;   // 均方误差
    float mae;   // 平均绝对误差
    float r2;    // R²决定系数
    float rmse;  // 均方根误差
};
```

### 3.2 训练接口

#### `addTrainingExample(text, params, embed, tags)`
添加单个训练样本到数据集。

#### `loadTrainingData(csvFile) → bool`
从 CSV 文件加载训练数据。CSV 格式: 257 列 (1 文本 + 128 参数 + 128 嵌入 + 1 标签)。

#### `trainSimpleLinearModel(config) → bool`

训练简单线性模型: 文本特征 → 参数映射。

**模型架构**:
```
输入: 128维文本特征向量 x
权重: W (128×128 矩阵)
偏置: b (128 向量)
输出: y = W·x + b
```

**训练流程**:
1. Xavier 初始化 W 和 b
2. 预计算所有文本特征向量
3. 随机分割训练集/验证集 (80/20)
4. Mini-batch SGD:
   - 梯度: `dL/dW[i][j] = 2*(ŷ-yt)*x[j]/128`
   - 梯度: `dL/db[i] = 2*(ŷ-yt)/128`
5. 每个 epoch 随机打乱数据
6. 早停: 验证损失连续 10 轮无改善

#### `evaluateModel() → ModelMetrics`

评估训练好的模型，返回 MSE/MAE/R²/RMSE。

**R² 计算**: `R² = 1 - Σ(y-ŷ)² / Σ(y-ȳ)²`

#### `predict(text) → vector<float>`

使用训练好的模型进行前向推理。

#### `exportONNXModel(outputPath) → bool`

导出模型为 ONNX 格式 (.onnx 文件)。

**导出内容**:
- 输入: `text_features` [batch, 128] float32
- 输出: `parameters` [batch, 128] float32
- 算子: `Gemm` (alpha=1.0, beta=0.0)
- 权重: W 和 b 作为初始化器嵌入
- 元数据: producer="LianCore", opset=13

### 3.3 状态查询

| 方法 | 说明 |
|------|------|
| `getTrainingProgress()` | 训练进度 0.0-1.0 |
| `getModelMetrics()` | 当前模型指标 |
| `isModelTrained()` | 模型是否已训练 |
| `getSampleCount()` | 训练样本数 |

---

## 4. ONNX 模型格式

### 4.1 模型协议

LianCore 导出的 ONNX 模型遵循标准 ONNX 规范 v7:

```
文件结构:
┌──────────────────────────────────┐
│  Magic Number (8 bytes)          │
│  "ONNX" + version bytes          │
├──────────────────────────────────┤
│  IR Version (4 bytes)            │
│  IR_VERSION = 7                  │
├──────────────────────────────────┤
│  Protobuf Length (4 bytes)       │
│  Little-endian uint32            │
├──────────────────────────────────┤
│  ModelProto (protobuf encoded)   │
│  ├─ GraphProto                   │
│  │  ├─ NodeProto (Gemm)         │
│  │  ├─ ValueInfoProto (input)   │
│  │  ├─ ValueInfoProto (output)  │
│  │  └─ TensorProto (W, b)       │
│  └─ OpsetImportProto (opset=13) │
└──────────────────────────────────┘
```

### 4.2 模型规范

| 属性 | 值 |
|------|-----|
| 输入名称 | `text_features` |
| 输入形状 | [batch_size, 128] |
| 输入类型 | float32 |
| 输出名称 | `parameters` |
| 输出形状 | [batch_size, 128] |
| 输出类型 | float32 |
| 算子 | `Gemm` |
| Opset | 13 |
| Producer | "LianCore" |

### 4.3 加载示例

```cpp
// C++ 加载
Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "LianCore");
Ort::SessionOptions opts;
auto session = Ort::Session(env, "model.onnx", opts);

// 推理
std::vector<float> textFeatures(128);  // 文本特征
std::vector<int64_t> shape = {1, 128};
auto input = Ort::Value::CreateTensor<float>(
    memoryInfo, textFeatures.data(), 128, shape.data(), 2);
auto output = session.Run({}, {"text_features"}, &input, 1, {"parameters"}, 1);
float* params = output[0].GetTensorMutableData<float>();
```

### 4.4 Python 推理

```python
import onnxruntime as ort
import numpy as np

session = ort.InferenceSession("liancore_v1.onnx")
text_features = np.random.randn(1, 128).astype(np.float32)
output = session.run(["parameters"], {"text_features": text_features})
params = output[0]  # shape: (1, 128)
```

---

## 5. 训练数据格式

### 5.1 CSV 格式

```
列布局 (257 列):
  列 0: 文本提示词 (双引号包裹)
  列 1-128: 参数向量 (128 个 float)
  列 129-256: 音频嵌入 (128 个 float)
  列 257: 风格标签 (逗号分隔)

示例:
"温暖的模拟合成器铺底",0.3,0.2,0.5,...,0.7,0.1,0.4,...,"模拟,温暖,铺底"
```

### 5.2 JSON 格式

```json
{
  "textPrompts": [
    "温暖的模拟合成器铺底",
    "明亮的电子合成器主音"
  ],
  "parameterVectors": [
    [0.3, 0.2, 0.5, ...],
    [0.8, 0.1, 0.6, ...]
  ],
  "audioEmbeddings": [
    [0.1, 0.4, 0.2, ...],
    [0.5, 0.3, 0.7, ...]
  ],
  "styleTags": [
    "模拟,温暖,铺底",
    "电子,明亮,主音"
  ]
}
```

### 5.3 数据采集建议

| 来源 | 方法 | 说明 |
|------|------|------|
| 用户预设 | 收集用户保存的预设名+参数 | 最准确 |
| 人工标注 | 音频工程师标注音色描述 | 高质量 |
| 合成数据 | 规则引擎生成配对数据 | 大规模 |
| 在线语料 | 爬取合成器论坛描述 | 补充 |

---

## 6. 文本特征提取算法

### 6.1 算法流程

```
输入文本 "明亮的电子合成器主音"
  │
  ├─ 字符级哈希 (每字符 mod 128 → 累加)
  │   '明'(26126) → 26126 % 128 = 22 → feature[22] += 1/255
  │   '亮'(20142) → 20142 % 128 = 54 → feature[54] += 1/255
  │   ...
  │
  ├─ 位置加权 (1/(i+1) 衰减)
  │   feature[hash % 128] += 1/(255 * (i+1))
  │
  ├─ Bigram 特征 (两字符组合, 0.5 权重)
  │   "明亮" → hash → feature[idx] += 0.5/255
  │
  ├─ Trigram 特征 (三字符组合, 0.25 权重)
  │   "明亮的" → hash → feature[idx] += 0.25/255
  │
  └─ 词级 FNV-1a 哈希 (前 32 词)
       "明亮的" → FNV-1a → feature[idx] += 1.0/255

输出: 128维 float 向量, L2归一化
```

### 6.2 哈希函数

```cpp
// FNV-1a 词级哈希
uint32_t hash = 2166136261;
for (char c : word) {
    hash ^= static_cast<uint32_t>(c);
    hash *= 16777619;
}
return hash % 128;
```

---

## 7. 参数空间映射

### 7.1 ParameterSpaceMapper

将 AI 推理输出的 128 维嵌入向量映射到实际插件参数。

**映射策略**:
- 直接映射: 嵌入维度 → 参数 (1:1 对应)
- 加权映射: 多维度加权求和 → 参数
- 范围映射: 嵌入值 [0,1] → 参数范围 [min, max]

### 7.2 关键词规则引擎 (回退)

30+ 关键词规则覆盖:

| 类别 | 关键词 | 目标参数 | 示例 |
|------|--------|----------|------|
| 音色 | 明亮/温暖/暗/尖锐/柔和 | filter_cutoff | "明亮" → cutoff=0.8 |
| 动态 | 快速/慢速/长音/短促/打击 | env_attack/release | "打击" → attack=0.05 |
| 风格 | 复古/现代/电子/经典/管弦 | osc_waveform | "复古" → waveform=0.25 |
| 空间 | 大厅/房间/环境 | reverb_size | "大厅" → reverb=0.8 |
| 音高 | 低音/高音 | osc_pitch | "低音" → pitch=0.0 |
| 质感 | 噪声/纯净 | noise_level | "噪声" → noise=0.3 |

### 7.3 参数解释生成

```cpp
// 当前值解释
if (value > 0.8) → "AI自动提高{参数}以增加{上下文}"
if (value < 0.2) → "AI自动降低{参数}以营造{上下文}效果"
else → "AI根据'{上下文}'调整了{参数}"
```

---

## 附录

### A. 错误码

| 错误 | 说明 | 处理 |
|------|------|------|
| 模型加载失败 | ONNX 文件不存在或损坏 | 回退到规则引擎 |
| 推理超时 | ONNX 推理 > 100ms | 取消推理，使用缓存 |
| 缓存满 | 128 条缓存已满 | LRU 淘汰最旧条目 |
| 训练数据不足 | 样本数 < 10 | 跳过训练，返回错误 |

### B. 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 3.0-beta | 2026-07-12 | 初始 ONNX Runtime 集成 + 规则引擎回退 + 线性模型训练 |

### C. 参考

- [ONNX Runtime 文档](https://onnxruntime.ai/docs/)
- [RBJ Audio EQ Cookbook](https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html)
- [FNV Hash](https://en.wikipedia.org/wiki/Fowler-Noll-Vo_hash_function)

---

> **文档维护**: 本文档随 AI 模块代码同步更新。新增模型或变更接口后请更新对应章节。
> **相关文档**: [核心引擎技术文档](./LianCore_Core_Engine_Technical_Documentation.md)