# LianCore V3 macOS 实机验证测试报告模板 (P8)
# 当前环境: Windows，无法执行 macOS 实机构建
# 自动化脚本: scripts/macos_auto_test.sh（一键构建+测试+报告）
# 手动验证: 请在实际 macOS 环境下执行以下步骤并填写报告

## 测试环境信息
| 项目 | 值 |
|------|-----|
| 测试日期 | ___________________ |
| macOS 版本 | ___________________ |
| 处理器型号 | Apple Silicon / Intel |
| Xcode 版本 | ___________________ |
| CMake 版本 | ___________________ |
| JUCE 版本 | 8.0.14 (macOS) |
| ONNX Runtime 版本 | 1.18.1 (macOS) |
| 测试执行人 | ___________________ |

## 构建验证清单

### 1. 环境准备
- [ ] Xcode Command Line Tools 已安装 (`xcode-select --install`)
- [ ] CMake 3.24+ 已安装 (`cmake --version`)
- [ ] JUCE 8.0.14 macOS 已下载并解压到 `juce-8.0.14-osx/`
- [ ] ONNX Runtime 1.18.1 macOS 已下载/自动下载

### 2. CMake 配置
- [ ] `cmake -B build_mac -G Xcode -DCMAKE_BUILD_TYPE=Release` 执行成功
- [ ] ONNX Runtime 检测通过 (`LIANCORE_HAS_ONNX=1`)
- [ ] 无 CMake 错误或警告

命令:
```bash
cd /path/to/LianCore
cmake -B build_mac -G Xcode -DCMAKE_BUILD_TYPE=Release 2>&1 | tee cmake_config.log
```

结果:
```
___________________
```

### 3. VST3 构建
- [ ] `cmake --build build_mac --config Release --target LianCore_VST3` 编译成功
- [ ] VST3 产物存在: `build_mac/LianCore_artefacts/Release/VST3/LianCore.vst3`
- [ ] VST3 文件大小: ______ MB
- [ ] 编译无错误，仅允许 JUCE 相关 warnings

命令:
```bash
cmake --build build_mac --config Release --target LianCore_VST3 2>&1 | tee vst3_build.log
```

错误数: ______
警告数: ______

### 4. AU 构建
- [ ] `cmake --build build_mac --config Release --target LianCore_AU` 编译成功
- [ ] AU 产物存在: `build_mac/LianCore_artefacts/Release/AU/LianCore.component`
- [ ] AU 文件大小: ______ MB

命令:
```bash
cmake --build build_mac --config Release --target LianCore_AU 2>&1 | tee au_build.log
```

### 5. 代码签名 (开发模式)
- [ ] VST3 ad-hoc 签名成功
- [ ] AU ad-hoc 签名成功

命令:
```bash
codesign --force --deep --sign - build_mac/LianCore_artefacts/Release/VST3/LianCore.vst3
codesign --force --deep --sign - build_mac/LianCore_artefacts/Release/AU/LianCore.component
```

### 6. 插件安装
- [ ] VST3 复制到 `~/Library/Audio/Plug-Ins/VST3/`
- [ ] AU 复制到 `~/Library/Audio/Plug-Ins/Components/`

### 7. 测试套件
- [ ] `cmake --build build_mac --config Release --target LianCoreTests` 编译成功
- [ ] 测试执行: `./build_mac/tests/Release/LianCoreTests`

命令:
```bash
cmake --build build_mac --config Release --target LianCoreTests 2>&1
./build_mac/tests/Release/LianCoreTests -r console 2>&1 | tee test_results.log
```

预期结果:
| 指标 | 预期值 | 实际值 |
|------|--------|--------|
| 测试用例总数 | 230 | ______ |
| 通过数 | 230 | ______ |
| 断言总数 | ~19439 | ______ |

### 8. DAW 兼容性测试
- [ ] 在 Logic Pro 中扫描并加载 LianCore VST3
- [ ] 在 Logic Pro 中扫描并加载 LianCore AU
- [ ] 在 Ableton Live 中扫描并加载 LianCore VST3 (如果可用)
- [ ] 在 Reaper 中扫描并加载 LianCore VST3 (如果可用)
- [ ] 插件 GUI 正常显示
- [ ] 参数控制正常响应
- [ ] 音频正常输出
- [ ] 预设加载/保存正常
- [ ] MPE 录制 UI 正常
- [ ] 粒子合成 UI 正常
- [ ] 效果链 UI 正常

DAW 测试结果:
| DAW | 格式 | 能否扫描 | 能否加载 | 能否播放 | 备注 |
|-----|------|---------|---------|---------|------|
| Logic Pro | VST3 | ______ | ______ | ______ | |
| Logic Pro | AU | ______ | ______ | ______ | |
| Ableton Live | VST3 | ______ | ______ | ______ | |
| Reaper | VST3 | ______ | ______ | ______ | |

### 9. 架构验证
- [ ] Apple Silicon (arm64) 原生构建
- [ ] Intel (x86_64) Rosetta 2 兼容性测试
- [ ] 通用二进制 (Universal Binary) 文件信息

命令:
```bash
file build_mac/LianCore_artefacts/Release/VST3/LianCore.vst3/Contents/MacOS/LianCore
```

预期输出:
```
Mach-O universal binary with 2 architectures: [arm64] [x86_64]
```

实际输出:
```
___________________
```

### 10. 性能基准
- [ ] 运行性能测试 (如可用)

命令:
```bash
./build_mac/tests/Release/LianCoreTests "[performance]" -r console 2>&1
```

## 已知问题确认
| 问题 | Windows 状态 | macOS 是否复现 |
|------|-------------|---------------|
| ChaoticLFO Logistic Map 偶发失败 | 已修复 | ______ |
| ONNX Runtime 堆损坏 (0xc0000374) | 已通过 try-catch 优雅处理 | ______ |
| PresetManager PM-002 堆损坏 | 已修复（栈→堆分配） | ______ |
| JUCE 8.0.14 Font 弃用警告 | 已修复 | ______ |

## 问题与备注
```
___________________
```

## 签名确认
测试执行人: ___________________
日期: ___________________