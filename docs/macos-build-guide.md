# LianCore V3 macOS 构建指南

## 系统要求

- macOS 13 (Ventura) 或更高版本
- Apple Silicon (M1/M2/M3/M4) 或 Intel (x86_64) 处理器
- Xcode 15.0+ (含 Command Line Tools)
- CMake 3.24+

## 依赖安装

### 1. Xcode Command Line Tools

```bash
xcode-select --install
```

### 2. CMake

```bash
brew install cmake
```

### 3. JUCE 8.0.14 (macOS)

```bash
# 下载 JUCE 8.0.14 macOS 版本
cd <项目根目录>
curl -L -o juce-8.0.14-osx.zip \
  "https://github.com/juce-framework/JUCE/releases/download/8.0.14/juce-8.0.14-osx.zip"
unzip juce-8.0.14-osx.zip
# 解压后目录结构: juce-8.0.14-osx/JUCE/CMakeLists.txt
```

### 4. ONNX Runtime 1.18.1 (可选，AI功能)

CMake 配置时会自动下载 ONNX Runtime：
- Apple Silicon: `onnxruntime-osx-arm64-1.18.1.tgz`
- Intel: `onnxruntime-osx-x64-1.18.1.tgz`

如需手动下载：
```bash
# Apple Silicon
curl -L -o onnxruntime.tgz \
  "https://github.com/microsoft/onnxruntime/releases/download/v1.18.1/onnxruntime-osx-arm64-1.18.1.tgz"

# Intel
curl -L -o onnxruntime.tgz \
  "https://github.com/microsoft/onnxruntime/releases/download/v1.18.1/onnxruntime-osx-x64-1.18.1.tgz"
```

## 构建步骤

### 1. 配置 CMake

```bash
cd <项目根目录>

# 创建构建目录
cmake -B build_mac -G Xcode -DCMAKE_BUILD_TYPE=Release

# 如果 CMake 找不到 JUCE，手动指定路径:
# cmake -B build_mac -G Xcode \
#   -DCMAKE_BUILD_TYPE=Release \
#   -DJUCE_DIR="<项目根目录>/juce-8.0.14-osx/JUCE"
```

### 2. 构建 VST3 + AU 插件

```bash
# 使用 Xcode 构建
cmake --build build_mac --config Release

# 或打开 Xcode 项目
open build_mac/LianCore.xcodeproj
```

### 3. 构建产物

构建成功后，产物位于 `build_mac/LianCore_artefacts/Release/`：

| 格式 | 产物路径 | 安装路径 |
|------|---------|---------|
| VST3 | `LianCore_artefacts/Release/VST3/LianCore.vst3` | `~/Library/Audio/Plug-Ins/VST3/` |
| AU | `LianCore_artefacts/Release/AU/LianCore.component` | `~/Library/Audio/Plug-Ins/Components/` |

### 4. 安装插件

```bash
# 安装 VST3
cp -R build_mac/LianCore_artefacts/Release/VST3/LianCore.vst3 \
  ~/Library/Audio/Plug-Ins/VST3/

# 安装 AU
cp -R build_mac/LianCore_artefacts/Release/AU/LianCore.component \
  ~/Library/Audio/Plug-Ins/Components/

# 刷新 AU 缓存
killall -9 AudioComponentRegistrar 2>/dev/null
```

## 代码签名 (可选)

### 开发者签名 (本地使用)

```bash
# 使用 ad-hoc 签名 (仅本地开发)
codesign --force --deep --sign - \
  build_mac/LianCore_artefacts/Release/VST3/LianCore.vst3

codesign --force --deep --sign - \
  build_mac/LianCore_artefacts/Release/AU/LianCore.component
```

### 分发签名 (需 Apple Developer 账号)

```bash
# 设置签名身份
export CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAM_ID)"

# 签名 VST3
codesign --force --deep --sign "$CODESIGN_IDENTITY" \
  --options runtime --timestamp \
  build_mac/LianCore_artefacts/Release/VST3/LianCore.vst3

# 签名 AU
codesign --force --deep --sign "$CODESIGN_IDENTITY" \
  --options runtime --timestamp \
  build_mac/LianCore_artefacts/Release/AU/LianCore.component

# 公证 (需 Apple Notary Service)
xcrun notarytool submit LianCore_vst3_signed.zip \
  --apple-id "your@email.com" \
  --team-id "TEAM_ID" \
  --password "@keychain:AC_PASSWORD" \
  --wait
```

## 运行单元测试

```bash
# 构建测试
cmake --build build_mac --target LianCoreTests --config Release

# 运行测试
./build_mac/tests/Release/LianCoreTests

# 运行特定测试类别
./build_mac/tests/Release/LianCoreTests "[oversampling]"
./build_mac/tests/Release/LianCoreTests "[spectral]"
```

## 常见问题

### Q: CMake 找不到 JUCE
```bash
# 确保 juce-8.0.14-osx 目录存在且包含 JUCE 子目录
ls juce-8.0.14-osx/JUCE/CMakeLists.txt
# 如果不存在，请重新下载 JUCE
```

### Q: 构建失败 - "AAX format not supported"
JUCE 在 macOS 上自动跳过 AAX 格式。如果仍有问题，可以修改 `CMakeLists.txt`：
```cmake
# 将 FORMATS VST3 AAX AU 改为:
FORMATS VST3 AU
```

### Q: ONNX Runtime 下载失败
AI 功能需要 ONNX Runtime。如果下载失败，可以禁用 AI 功能：
```bash
cmake -B build_mac -G Xcode -DCMAKE_BUILD_TYPE=Release -DLIANCORE_BUILD_TESTS=OFF
```

### Q: VST3 插件在 DAW 中不显示
```bash
# 检查 VST3 是否安装正确
ls -la ~/Library/Audio/Plug-Ins/VST3/LianCore.vst3

# 重启 DAW 或清除插件缓存
# Logic Pro: ~/Library/Caches/AudioUnitCache/
```

### Q: Apple Silicon 架构
项目默认构建为通用二进制 (arm64 + x86_64)。如需仅构建 arm64：
```bash
cmake -B build_mac -G Xcode \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64
```

## macOS 兼容性说明

本项目代码已针对 macOS 进行兼容性处理：

| 特性 | 状态 |
|------|------|
| VST3 格式 | 支持 |
| Audio Unit (AU) 格式 | 支持 |
| Apple Silicon (arm64) | 支持 |
| Intel (x86_64) | 支持 |
| 通用二进制 | 支持 |
| ONNX Runtime AI 推理 | 支持 |
| 宽字符路径 (Windows 特定) | 已通过 `#ifdef _WIN32` 隔离 |
| SQLite (内嵌) | 已通过 SQLite 内置跨平台支持 |
| JUCE 8.0.14 | 已通过 `juce-8.0.14-osx` 路径配置 |