# LianCore 情感滑块 AI 对接方案设计

> **版本**: v1.0 | **日期**: 2026-07-12 | **状态**: 设计阶段

---

## 1. 概述

### 1.1 背景

LianCore Web UI 极简模式提供了三颗情感滑块：**温暖度 (Warmth)**、**能量感 (Energy)**、**紧张度 (Tension)**。当前滑块仅作为 UI 控件存在，未与 AI 推理引擎或参数空间对接。本方案设计情感滑块到合成器参数空间的完整映射机制。

### 1.2 目标

- 将三颗情感滑块 (0.0~1.0) 映射到 128 维参数空间
- 支持与 AI 文本生成协同工作（文本 + 情感向量的联合推理）
- 支持独立的情感→参数快速映射（无需 AI 推理）
- 保证映射的实时性 (< 1ms)

---

## 2. 架构设计

### 2.1 整体架构

```
┌─────────────────────────────────────────────────┐
│                    Web UI                        │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐           │
│  │ Warmth  │ │ Energy  │ │ Tension │           │
│  │ 0.0~1.0 │ │ 0.0~1.0 │ │ 0.0~1.0 │           │
│  └────┬────┘ └────┬────┘ └────┬────┘           │
│       │           │           │                  │
│       └───────────┼───────────┘                  │
│                   │ WebSocket                    │
│              {type:"emotion", warmth,energy,tension}
└───────────────────┼─────────────────────────────┘
                    │
┌───────────────────▼─────────────────────────────┐
│           EmotionToParameterMapper (新增)         │
│                                                   │
│  ┌─────────────────────────────────────────────┐ │
│  │         情感向量 → 参数空间映射               │ │
│  │  Warmth  → [cutoff↑, resonance↓, drive↓,   │ │
│  │             EQ lowGain↑, filterMode...]     │ │
│  │  Energy  → [attack↓, volume↑, unison↑,     │ │
│  │             drive↑, compressor ratio...]    │ │
│  │  Tension → [detune↑, fmDepth↑, rate↑,      │ │
│  │             delay↑, reverb↓, warp↑...]      │ │
│  └─────────────────────────────────────────────┘ │
│                                                   │
│  ┌─────────────────────────────────────────────┐ │
│  │         情感预设数据库 (Emotion Preset DB)     │ │
│  │  8个锚点预设 (立方体8个顶点)                  │ │
│  │  + 插值生成任意情感位置的参数                  │ │
│  └─────────────────────────────────────────────┘ │
└───────────────────┬─────────────────────────────┘
                    │
┌───────────────────▼─────────────────────────────┐
│           AIInferenceEngine (已有)                │
│  generateParameters(text, audio, styleTags)       │
│  + 新增: generateParametersWithEmotion(           │
│      text, emotionVector)                        │
└─────────────────────────────────────────────────┘
```

### 2.2 新增组件: EmotionToParameterMapper

**文件**: `src/ai/EmotionToParameterMapper.h/.cpp`

**职责**: 将三维情感向量 (warmth, energy, tension) 映射到合成器参数空间

**核心方法**:
- `mapEmotionToParameters(warmth, energy, tension)` → `std::vector<ParameterMapping>`
- `getAnchorPreset(warmth, energy, tension)` → 获取最近的锚点预设
- `setAnchorPreset(index, preset)` → 设置锚点预设

---

## 3. 参数映射规则

### 3.1 温暖度 (Warmth) → 频谱色彩

| 参数 | 低温暖 (0.0) | 高温暖 (1.0) | 映射公式 |
|------|-------------|-------------|---------|
| filter.cutoff | 0.3 (暗) | 0.8 (亮) | `cutoff = 0.3 + warmth * 0.5` |
| filter.resonance | 0.1 (平) | 0.5 (暖峰) | `res = 0.1 + warmth * 0.4` |
| distortion.drive | 0.0 | 0.4 (温和驱动) | `drive = warmth * 0.4` |
| EQ.band1Gain (低频) | -6dB | +3dB | `gain = (warmth-0.5) * 9` |
| EQ.band3Gain (中频) | 0dB | -2dB | `gain = -warmth * 2` |
| EQ.band5Gain (高频) | 0dB | -3dB (去刺) | `gain = -warmth * 3` |
| reverb.mix | 0.1 | 0.4 (空间感) | `mix = 0.1 + warmth * 0.3` |

### 3.2 能量感 (Energy) → 动态响应

| 参数 | 低能量 (0.0) | 高能量 (1.0) | 映射公式 |
|------|-------------|-------------|---------|
| envelope.attack | 0.5 (慢) | 0.05 (快) | `attack = 0.5 - energy * 0.45` |
| envelope.release | 0.6 (长) | 0.1 (短) | `release = 0.6 - energy * 0.5` |
| oscillator.volume | 0.5 | 0.9 | `vol = 0.5 + energy * 0.4` |
| oscillator.unison | 0.0 | 0.7 (加厚) | `unison = energy * 0.7` |
| compressor.ratio | 2:1 | 4:1 | `ratio = 0.3 + energy * 0.5` |
| distortion.drive | 0.0 | 0.7 (激进) | `drive = energy * 0.7` |

### 3.3 紧张度 (Tension) → 调制与不和谐度

| 参数 | 低紧张 (0.0) | 高紧张 (1.0) | 映射公式 |
|------|-------------|-------------|---------|
| oscillator.detune | 0.0 | 0.4 (失谐) | `detune = tension * 0.4` |
| oscillator.fmDepth | 0.0 | 0.6 | `fm = tension * 0.6` |
| lfo.rate | 0.1 (慢) | 0.7 (快) | `rate = 0.1 + tension * 0.6` |
| lfo.depth | 0.0 | 0.5 | `depth = tension * 0.5` |
| delay.feedback | 0.1 | 0.6 | `fb = 0.1 + tension * 0.5` |
| reverb.decay | 1.0 (短) | 0.3 (长) | `decay = 1.0 - tension * 0.7` |
| filter.resonance | 0.1 | 0.7 (尖锐) | `res = 0.1 + tension * 0.6` |
| warp.amount | 0.0 | 0.5 (变形) | `warp = tension * 0.5` |

---

## 4. 情感锚点预设系统

### 4.1 立方体模型

将三颗滑块视为三维空间 (Warmth, Energy, Tension)，8 个顶点为锚点：

| # | Warmth | Energy | Tension | 预设名称 | 典型场景 |
|---|--------|--------|---------|---------|---------|
| 0 | 0 | 0 | 0 | 暗柔铺底 | 氛围音乐、冥想 |
| 1 | 0 | 0 | 1 | 紧张暗铺底 | 恐怖配乐、悬疑 |
| 2 | 0 | 1 | 0 | 强力低音 | EDM、Dubstep |
| 3 | 0 | 1 | 1 | 激进失真 | 工业、金属 |
| 4 | 1 | 0 | 0 | 温暖铺底 | Lo-Fi、爵士 |
| 5 | 1 | 0 | 1 | 紧张氛围 | 科幻配乐 |
| 6 | 1 | 1 | 0 | 明亮主音 | 流行、Trance |
| 7 | 1 | 1 | 1 | 复杂合成 | 实验、IDM |

### 4.2 插值算法

任意情感位置 (w, e, t) 的参数通过三线性插值计算：
```
P(w,e,t) = Σ(i,j,k∈{0,1}) P_ijk * w_i * e_j * t_k
```

---

## 5. AI 推理集成

### 5.1 新增 API

```cpp
// AIInferenceEngine 新增方法
GenerationResult generateParametersWithEmotion(
    const juce::String& textPrompt,           // 文本描述
    float warmth, float energy, float tension, // 情感向量
    const std::vector<juce::String>& styleTags = {}
);
```

### 5.2 推理流程

```
1. 文本 → 特征向量 (128维, 已有)
2. 情感向量 (3维) → 情感偏置 (128维)
   warmth → 加热频谱色彩参数
   energy → 加动态响应参数
   tension → 加调制深度参数
3. 特征向量 + 情感偏置 → 最终参数向量
4. 最终参数向量 → ParameterSpaceMapper → 合成器参数
```

### 5.3 情感向量权重

文本生成为主，情感滑块为偏置：
- 纯文本生成: `final = text_features * 0.7 + emotion_bias * 0.3`
- 纯情感调节: `final = emotion_bias * 1.0`
- 混合模式: `final = text_features * (1 - emotion_weight) + emotion_bias * emotion_weight`

---

## 6. WebSocket 协议扩展

### 6.1 新消息类型

```json
{
  "type": "emotion",
  "payload": {
    "warmth": 0.75,
    "energy": 0.50,
    "tension": 0.25
  }
}
```

### 6.2 联合生成消息

```json
{
  "type": "generate",
  "payload": {
    "text": "温暖的合成器铺底",
    "emotion": {
      "warmth": 0.8,
      "energy": 0.3,
      "tension": 0.1
    },
    "styleTags": ["温暖", "铺底", "复古"]
  }
}
```

---

## 7. 实现计划

| 阶段 | 内容 | 文件 |
|------|------|------|
| Phase 1 | `EmotionToParameterMapper` 类 | `src/ai/EmotionToParameterMapper.h/.cpp` |
| Phase 2 | 8 个锚点预设定义 | 同上 |
| Phase 3 | 三线性插值算法 | 同上 |
| Phase 4 | `AIInferenceEngine::generateParametersWithEmotion` | `src/ai/AIInferenceEngine.cpp` |
| Phase 5 | WebSocket 消息处理 | `src/plugin/PluginEditor.cpp` |
| Phase 6 | UI 联动 (滑块变化实时推送) | `ui/src/App.tsx` |

---

## 8. 风险与对策

| 风险 | 对策 |
|------|------|
| 锚点预设质量不足 | 使用人工审核 + AI 辅助优化锚点预设 |
| 三线性插值可能产生不自然音色 | 限制插值空间，添加约束条件 |
| 实时性不足 | 插值算法 O(1)，预计算所有锚点参数 |
| 与已有参数系统冲突 | 情感参数作为"偏置"叠加，不覆盖用户手动设置 |

---

> **下一步**: 进入 writing-plans 阶段，生成详细实现任务列表