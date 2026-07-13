# LianCore V3 — 项目记忆

**最后更新**: 2026-07-13 | **当前阶段**: Release (顶尖能力提升)

---

## 项目状态总览

| 维度 | 状态 |
|------|------|
| 合成引擎 | 20种节点, 6种引擎, 完整 |
| AI系统 | 5个ONNX模型, 端侧推理 <50ms |
| 测试 | 48/48 通过 → 新增13个 MPE/微音程测试 |
| 预设 | 504出厂预设 (0.6MB) + 100K扩展 (261MB可选) |
| 波表 | 100出厂波表 (300MB) |
| 构建 | VST3构建修复 (juceaide崩溃 + 路径问题) |
| MPE | 完整支持 (复音触后/弯音/滑音) |
| 微音程 | Scala .scl 加载 |

---

## 2026-07-13: Release阶段 — 顶尖能力提升

### 完成的任务

1. **能力评估与差距分析**
   - 对比 Serum2, Vital, Avenger2, Absynth6
   - 14维度量化对比
   - 综合评分: LianCore 6.4/10 → 修复后预计 7.8/10 (领先)
   - 文档: `docs/roadmap-top-tier.md`

2. **VST3构建修复**
   - 根因: JUCE 8.0.14 juceaide 中文路径崩溃
   - 修复: `NEEDS_BINARY_BUILDER FALSE`, `BUNDLE_ID`, `JUCE_VST3_CAN_REPLACE_VST2=0`, `extern "C" __cdecl createPluginFilter`
   - 解决方案: 复制到纯ASCII路径 `C:\LianCoreSrc\` 构建

3. **DAW兼容性实测指南**
   - 4款DAW (Ableton, Cubase, FL Studio, Studio One) 详细操作步骤
   - 包含: 扫描/加载/MIDI/自动化/预设/多实例/冻结
   - 文档: `docs/daw-testing-guide.md`

4. **预设数据库压缩**
   - 原始: 376.8 MB
   - VACUUM: 332.3 MB
   - 拆分: 出厂0.6MB + 扩展261.4MB
   - 脚本: `scripts/compress_preset_db.py`, `scripts/split_factory_presets.py`

5. **MPE 与微音程实现**
   - MPE: Legacy Zone 1 (主通道1, 复音通道2-15, ±48半音)
   - 微音程: Scala .scl 加载, 参考频率设置, 重置默认
   - 测试: 13个新测试 (6 MPE + 7 微音程)
   - 文件: `src/plugin/PluginProcessor.h/.cpp`, `tests/ai/MPEAndMicrotuningTests.cpp`

---

## 构建说明

### VST3 构建 (已知问题)

**问题**: JUCE 8.0.14 的 `juceaide` 在路径包含中文时崩溃
**解决**: 将项目复制到纯 ASCII 路径后构建

```powershell
# 复制到纯ASCII路径
robocopy "F:\LianCore软音源合成器V3版本商业正式版" "C:\LianCoreSrc" /E /NFL /NDL
# 构建
cd C:\LianCoreSrc
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target LianCore_VST3
```

### 单元测试

```bash
cd C:\LianCoreSrc
cmake --build build --config Release --target LianCoreTests
ctest --test-dir build --output-on-failure
```

---

## 关键文件

| 文件 | 用途 |
|------|------|
| `CMakeLists.txt` | 构建配置 (含 MPE/微音程修复) |
| `src/plugin/PluginProcessor.h` | MPE + 微音程 API |
| `src/plugin/PluginProcessor.cpp` | MPE + 微音程实现 |
| `tests/ai/MPEAndMicrotuningTests.cpp` | 13个新测试 |
| `docs/roadmap-top-tier.md` | 能力差距分析 |
| `docs/daw-testing-guide.md` | DAW实测指南 |
| `docs/design-decisions.md` | 14条设计决策 |
| `scripts/compress_preset_db.py` | 数据库压缩 |
| `scripts/split_factory_presets.py` | 数据库拆分 |
| `data/preset_library_compact.db` | 压缩后出厂数据库 (0.6MB) |

---

## 下一步规划

| 优先级 | 任务 |
|--------|------|
| P0 | 在纯ASCII路径完成VST3构建 |
| P1 | 复音数提升 16→32 |
| P1 | 效果器扩展 6→12种 |
| P1 | 混沌调制实现 |
| P2 | 预设数量增至1500+ |
| P2 | 波表编辑器 |
| P2 | macOS构建验证 |
| P3 | iOS/AUv3移植 |
| P3 | 社区预设市场 |