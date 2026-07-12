# LianCore V3 设计决策与架构知识库

> **版本**: V3 Beta Week 3 | **日期**: 2026-07-12 | **仓库**: github.com/Lianyilin-rgb/liancore-synth
>
> 本文档沉淀 LianCore 项目从 Alpha 到 Beta 阶段的所有关键设计决策、架构选型、技术权衡和经验教训。

---

## 目录

1. [核心架构决策](#1-核心架构决策)
2. [合成引擎决策](#2-合成引擎决策)
3. [AI 子系统决策](#3-ai-子系统决策)
4. [UI 架构决策](#4-ui-架构决策)
5. [构建与部署决策](#5-构建与部署决策)
6. [编码规范与约定](#6-编码规范与约定)
7. [技术债务与未来方向](#7-技术债务与未来方向)

---

## 1. 核心架构决策

### 1.1 节点式音频图引擎

**决策**: 采用基于 `AudioNode` 抽象基类的节点图架构，所有合成器、效果器、调制源均继承同一基类。

**理由**:
- 统一生命周期管理（`prepare`/`process`/`reset`）
- 统一端口连接机制（`AudioPort` 输入/输出）
- 支持用户自定义信号链，灵活组合音频处理模块
- 便于序列化/反序列化预设

**替代方案及拒绝理由**:
- 固定信号链: 不够灵活，用户无法自定义路由
- 模块化插件: 复杂度高，难以实现跨模块调制

**代码位置**: [`src/core/AudioNode.h`](file:///f:/LianCore软音源合成器V3版本商业正式版/src/core/AudioNode.h)

---

### 1.2 Kahn 拓扑排序执行

**决策**: 音频图引擎使用 Kahn 算法计算节点执行顺序。

**理由**:
- O(V+E) 线性时间复杂度，适合实时音频处理
- 天然支持循环检测（反馈连接检测）
- 非递归实现，避免栈溢出风险

**替代方案及拒绝理由**:
- DFS 拓扑排序: 递归深度可能超限，错误处理复杂
- 固定执行顺序: 不支持动态节点图

**代码位置**: [`src/core/AudioGraphEngine.cpp`](file:///f:/LianCore软音源合成器V3版本商业正式版/src/core/AudioGraphEngine.cpp)

---

### 1.3 调制矩阵路由表

**决策**: 使用 32 槽位的调制矩阵路由表（`ModulationMatrix`），每个槽位包含源/目标/深度/曲线类型。

**理由**:
- 灵活的调制配置，支持一对多/多对一调制
- 固定槽位大小便于内存预分配和实时处理
- 易于序列化到预设文件
- 32 槽位在灵活性和性能之间取得平衡

**替代方案及拒绝理由**:
- 全连接调制矩阵: 内存开销大（N*N），大多数连接闲置
- 动态分配调制: 实时性能不稳定

**代码位置**: [`src/modulation/ModulationMatrix.h`](file:///f:/LianCore软音源合成器V3版本商业正式版/src/modulation/ModulationMatrix.h)

---

### 1.4 参数系统分层

**决策**: 参数系统采用三层架构:
1. **ParameterTree**: 基于 JUCE `AudioProcessorValueTreeState`，负责参数存储和 DAW 自动化
2. **ParameterMapper**: 参数快照/自动化/宏控制（8 个宏旋钮）
3. **PresetManager**: SQLite 持久化预设管理

**理由**:
- 与 DAW 宿主自动化系统无缝集成（JUCE APVTS）
- 宏控制提供演出级别的快速参数映射
- SQLite 支持大规模预设库管理

**代码位置**: [`src/params/`](file:///f:/LianCore软音源合成器V3版本商业正式版/src/params/)

---

## 2. 合成引擎决策

### 2.1 波表合成器

**决策**: 采用预计算波表 + 双线性插值的方式实现波表合成。

**理由**:
- 预计算避免实时波形生成，降低 CPU 负载
- 双线性插值在质量和性能之间取得平衡
- 支持波表帧间变形（wavetable morphing）

**实现细节**:
- 波表大小: 2048 采样点/帧
- 最大帧数: 256 帧
- 插值: 双线性（帧内 + 帧间）
- 支持自定义波表导入

**代码位置**: [`src/synthesis/WavetableOscillator.h`](file:///f:/LianCore软音源合成器V3版本商业正式版/src/synthesis/WavetableOscillator.h)

---

### 2.2 虚拟模拟振荡器

**决策**: 实现经典模拟合成器波形（锯齿/方波/三角/脉冲），带抗混叠处理。

**理由**:
- 满足模拟合成器用户的音色需求
- 抗混叠处理避免数字伪影
- 脉冲宽度调制（PWM）支持

**实现细节**:
- 使用 BLEP (Band-Limited Step) 抗混叠
- 支持硬同步（hard sync）
- Unison 模式: 最多 8 复音叠加

**代码位置**: [`src/synthesis/VirtualAnalogOscillator.h`](file:///f:/LianCore软音源合成器V3版本商业正式版/src/synthesis/VirtualAnalogOscillator.h)

---

### 2.3 滤波器设计

**决策**: 使用转置直接 II 型 Biquad 结构实现所有滤波器类型。

**理由**:
- 数值稳定性优于直接 I 型
- 最少状态变量（2 个延迟线）
- 适合实时音频处理的低精度环境

**支持的滤波器类型**:
- 低通 (LPF): 12dB/24dB 斜率
- 高通 (HPF): 12dB/24dB
- 带通 (BPF)
- 带阻 (Notch)
- 峰值 (Peak)
- 低架 (Low Shelf)
- 高架 (High Shelf)

**代码位置**: [`src/synthesis/FilterProcessor.h`](file:///f:/LianCore软音源合成器V3版本商业正式版/src/synthesis/FilterProcessor.h)

---

### 2.4 效果器链架构

**决策**: 5 个独立效果器模块，每个作为独立 `AudioNode` 实现，可自由插入信号链任意位置。

**理由**:
- 模块化设计，每个效果器独立开发和测试
- 用户可自由排列效果器顺序
- 与节点图架构一致

**效果器列表**:
1. **Distortion**: 6 种失真类型 (SoftClip/HardClip/Tube/Fuzz/Bitcrush/Foldback)
2. **Delay**: 立体声延迟 + Ping-Pong，HP/LP 反馈滤波，12 种拍速同步
3. **Reverb**: Schroeder-Moorer 算法混响 (4 Comb + 2 All-Pass)
4. **Compressor**: RMS 前馈压缩器，软拐点
5. **EQ**: 8 段参数均衡器，7 种滤波器类型，RBJ 公式

**代码位置**: [`src/synthesis/Distortion.h`](file:///f:/LianCore软音源合成器V3版本商业正式版/src/synthesis/Distortion.h) 等

---

## 3. AI 子系统决策

### 3.1 合成数据驱动训练策略

**决策**: 采用合成数据（synthetic data）驱动 AI 模型训练，而非真实用户数据。

**理由**:
- 无需收集用户数据，保护隐私
- 可精确控制训练数据分布
- 支持快速迭代和回测
- 训练数据生成可自动化

**替代方案及拒绝理由**:
- 真实用户数据: 隐私风险、收集困难、分布不均
- 预训练模型微调: 模型过大，不适合端侧部署

**代码位置**: [`src/ai/AIModelTrainer.h`](file:///f:/LianCore软音源合成器V3版本商业正式版/src/ai/AIModelTrainer.h)

---

### 3.2 ONNX Runtime + 规则引擎回退

**决策**: AI 推理引擎采用双通道架构:
- 主通道: ONNX Runtime 推理（需 ONNX Runtime 库）
- 回退通道: 关键词规则引擎（纯 C++ 实现，零依赖）

**理由**:
- 兼容无 ONNX 环境的部署场景
- 保证基础 AI 功能 100% 可用
- 渐进式升级路径

**模型规范**:
- 格式: ONNX v7，Gemm 算子
- 输入: 128 维文本特征向量
- 输出: 128 维参数向量
- 推理延迟: < 1ms (CPU)

**代码位置**: [`src/ai/AIInferenceEngine.h`](file:///f:/LianCore软音源合成器V3版本商业正式版/src/ai/AIInferenceEngine.h)

---

### 3.3 文本特征提取算法

**决策**: 使用 TF-IDF 风格的关键词加权 + 嵌入映射实现文本到特征的转换。

**理由**:
- 轻量级，无需大型 NLP 模型
- 音乐合成领域词汇量有限（~500 关键词）
- 可解释性强，便于调试

**特征提取流程**:
1. 中文分词（基于词典）
2. 关键词加权（TF-IDF）
3. 128 维特征向量映射
4. → ONNX 推理 → 128 维参数向量
5. 参数空间映射到合成器参数

**代码位置**: [`src/ai/Tokenizer.h`](file:///f:/LianCore软音源合成器V3版本商业正式版/src/ai/Tokenizer.h)

---

## 4. UI 架构决策

### 4.1 双模式 Web UI

**决策**: Web UI 提供两种模式:
- **极简模式** (Minimal): 少量关键参数，适合快速调整
- **专业模式** (Pro): 节点图 + 参数面板 + 可视化组件

**理由**:
- 极简模式降低入门门槛，适合新手
- 专业模式提供完整控制，满足高级用户
- 同一套代码库，通过 `<ProModeApp />` 和 `<App />` 切换

**技术栈**:
- React 18 + TypeScript
- Vite 构建
- Canvas 渲染（节点图、波形、频谱、LFO）
- WebSocket 与插件通信

**代码位置**: [`ui/src/App.tsx`](file:///f:/LianCore软音源合成器V3版本商业正式版/ui/src/App.tsx), [`ui/src/ProMode.tsx`](file:///f:/LianCore软音源合成器V3版本商业正式版/ui/src/ProMode.tsx)

---

### 4.2 Canvas 节点图

**决策**: 使用原生 Canvas 2D API 渲染交互式节点图，而非 SVG 或 WebGL。

**理由**:
- 大量节点时 Canvas 性能优于 SVG
- 不需要 WebGL 的复杂性和兼容性问题
- Canvas 2D 足够满足节点图渲染需求

**实现特性**:
- 贝塞尔曲线连接线
- 拖拽/缩放/平移
- 节点选中高亮 + 渐变背景
- 网格背景辅助对齐

**代码位置**: [`ui/src/ProMode.tsx`](file:///f:/LianCore软音源合成器V3版本商业正式版/ui/src/ProMode.tsx#L41-L237)

---

### 4.3 可视化组件

**决策**: 在专业模式下嵌入三个 Canvas 可视化组件:
1. **WaveformVisualizer**: 波形展示（渐变描边 + 降采样）
2. **SpectrumVisualizer**: FFT 频谱柱状图（渐变映射 + alpha 透明度）
3. **LFOVisualizer**: LFO 动态曲线（实时动画 + 历史残影 + 光晕）

**理由**:
- 实时视觉反馈增强用户对音色的理解
- Canvas 动画性能优于 CSS/DOM 方案
- 与 p5.js 算法艺术示例互补

**代码位置**: [`ui/src/ProMode.tsx`](file:///f:/LianCore软音源合成器V3版本商业正式版/ui/src/ProMode.tsx#L257-L414)

---

### 4.4 参数旋钮组件

**决策**: 使用自定义 Canvas/CSS 旋钮组件替代 HTML range input。

**理由**:
- 模拟硬件合成器旋钮手感
- 270° 旋转角度，鼠标拖拽精确调节
- 支持频率对数显示（Hz → kHz 自动转换）

**实现细节**:
- 鼠标 Y 轴移动映射到参数值
- 灵敏度: 0.005/px
- 频率对数映射: 20Hz ~ 20kHz

**代码位置**: [`ui/src/ProMode.tsx`](file:///f:/LianCore软音源合成器V3版本商业正式版/ui/src/ProMode.tsx#L419-L486)

---

### 4.5 p5.js 算法艺术可视化

**决策**: 独立创建 p5.js 交互式可视化页面，包含三种模式:
1. **波表模式**: 谐波分析 + 波形变形 + FM 调制
2. **频谱模式**: 瀑布图 + HSL 色彩映射
3. **LFO 模式**: 多 LFO 叠加 + 残影轨迹 + 光晕

**理由**:
- p5.js 提供丰富的创意编码 API
- 独立页面方便演示和分享
- 与 ProMode 内的 Canvas 组件互补
- 5 种色彩主题支持不同审美偏好

**代码位置**: [`ui/visualizations/algorithmic_art.html`](file:///f:/LianCore软音源合成器V3版本商业正式版/ui/visualizations/algorithmic_art.html)

---

## 5. 构建与部署决策

### 5.1 CMake + JUCE 构建系统

**决策**: 使用 CMake 3.24+ 作为构建系统，通过 JUCE CMake API 管理插件构建。

**理由**:
- JUCE 官方推荐 CMake 构建方式
- 跨平台支持 (Windows/macOS/Linux)
- 支持 VST3/AU/AAX 多格式输出

**编译选项**:
- MSVC: `/arch:AVX2 /fp:fast /MP`
- GCC/Clang: `-march=native -ffast-math`
- C++ 标准: C++20

**代码位置**: [`CMakeLists.txt`](file:///f:/LianCore软音源合成器V3版本商业正式版/CMakeLists.txt)

---

### 5.2 SIMD 加速策略

**决策**: 使用 AVX2 指令集进行 SIMD 加速，覆盖以下操作:
- `buffer_clear`: 缓冲区清零 (256-bit 对齐)
- `buffer_multiply`: 缓冲区乘法
- `buffer_mix`: 缓冲区混合

**理由**:
- AVX2 在现代 x86 CPU 上广泛支持
- 256-bit 寄存器一次处理 8 个 float
- 显著降低音频处理 CPU 占用

**代码位置**: [`src/core/SIMDUtils.h`](file:///f:/LianCore软音源合成器V3版本商业正式版/src/core/SIMDUtils.h)

---

### 5.3 跨平台打包策略

**决策**: 使用 CPack 生成平台原生安装包:
- Windows: NSIS 安装包 (.exe)
- macOS: DMG 镜像 (.dmg)
- 所有依赖静态链接，双击即可安装

**理由**:
- 降低用户安装门槛
- 避免 DLL 版本冲突
- 符合各平台分发规范

---

## 6. 编码规范与约定

### 6.1 命名约定

| 类别 | 约定 | 示例 |
|------|------|------|
| 类名 | PascalCase | `AudioGraphEngine` |
| 方法名 | camelCase | `processBlock()` |
| 成员变量 | snake_case + `_` 后缀 | `sample_rate_` |
| 常量 | kPascalCase | `kMaxVoices` |
| 文件名 | PascalCase | `FilterProcessor.h` |
| 命名空间 | snake_case | `liancore::synthesis` |

### 6.2 文件组织

```
src/
├── core/           # 核心引擎（AudioNode, AudioGraphEngine, NodeFactory, SIMDUtils）
├── synthesis/      # 合成引擎（振荡器, 滤波器, 效果器）
├── modulation/     # 调制系统（调制矩阵, LFO, 包络, 步进音序器）
├── ai/             # AI 子系统（推理引擎, 训练器, 分词器）
├── params/         # 参数系统（参数树, 预设管理, 参数映射, 宏控制）
├── ui/             # 插件 UI（PluginEditor, WebSocket 服务器）
└── plugin/         # 插件入口（PluginProcessor）
ui/
├── src/            # React 源码
│   ├── App.tsx     # 极简模式
│   ├── ProMode.tsx # 专业模式
│   ├── main.tsx    # 入口
│   ├── websocket.ts# WebSocket 通信
│   └── styles.css  # 全局样式
├── visualizations/ # p5.js 算法艺术
└── index.html      # HTML 入口
```

### 6.3 Git 提交规范

使用 Conventional Commits 规范:
- `feat:` 新功能
- `fix:` 修复
- `refactor:` 重构
- `docs:` 文档
- `build:` 构建系统
- `perf:` 性能优化

---

## 7. 技术债务与未来方向

### 7.1 当前技术债务

| 项目 | 优先级 | 说明 |
|------|--------|------|
| 单元测试覆盖率 | 高 | 当前 < 30%，需补充核心模块测试 |
| 错误处理 | 中 | 部分模块缺少异常边界检查 |
| 内存管理 | 中 | 部分地方使用裸指针，建议迁移到智能指针 |
| 文档同步 | 低 | 代码注释与实现需定期同步 |

### 7.2 未来方向

1. **Beta Week 4**: 多采样器 (MultiSampler) 完善、鼓组切片 (DrumSlicer) 增强
2. **Beta Week 5**: 颗粒合成 (GranularPlayer) 完善
3. **Beta Week 6**: 性能优化、内存对齐、缓存友好重构
4. **Release Candidate**: 打包测试、兼容性矩阵验证、用户文档

### 7.3 已知限制

- ONNX Runtime 在部分 ARM 平台不可用（需回退到规则引擎）
- 当前 AI 模型仅支持线性回归，复杂音色映射需深度模型
- Web UI 依赖 WebSocket 连接，离线模式下不可用
- 当前仅支持 48kHz 采样率

---

> **文档维护**: 每次重大设计决策变更后更新本文档。
> **最后更新**: 2026-07-12 Beta Week 3