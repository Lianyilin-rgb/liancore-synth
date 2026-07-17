# LianCore V3 - AAX (Avid Audio eXtension) 构建指南

## 前置条件

### 1. Avid 开发者账号
AAX SDK 需要 Avid 开发者账号，可在 developer.avid.com 注册。

### 2. 下载 AAX SDK
1. 登录 Avid Developer Portal
2. 导航到 Downloads → AAX SDK
3. 下载 AAX SDK 2.9.0+（推荐）
4. 解压到以下任一位置:
   - 项目根目录: `aax-sdk-2-9-0/`
   - Windows: `F:\aax-sdk-2-9-0\`
   - macOS: `/Library/Developer/AAX/`
   - 或设置环境变量 `AAX_SDK_DIR`

### 3. Pro Tools 安装 (测试用)
AAX 插件只能在 Pro Tools 12.0+ 中加载。

## 构建步骤

### Windows (Visual Studio 2022)

```powershell
# 方式1: 环境变量
$env:AAX_SDK_DIR = "F:\aax-sdk-2-9-0"
cmake -B build_vst3 -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -S C:\LianCoreSrc
cmake --build build_vst3 --config Release -j 8

# 方式2: CMake 变量
cmake -B build_vst3 -G "Visual Studio 17 2022" -A x64 -DAAX_SDK_DIR="F:\aax-sdk-2-9-0" -S C:\LianCoreSrc
cmake --build build_vst3 --config Release -j 8
```

### macOS (Xcode) - 本地开发

```bash
export AAX_SDK_DIR="/Library/Developer/AAX"
cmake -B build_m -G Xcode -DCMAKE_BUILD_TYPE=Release
cmake --build build_m --config Release --parallel $(sysctl -n hw.logicalcpu)
```

### macOS (CI/CD) - GitHub Actions

```yaml
# CI 配置已更新为 Xcode 26.6.0
# .github/workflows/macos-build.yml 已包含 AAX 构建
# 注意: AAX SDK 受 NDA 保护，CI 中通过 GitHub Secrets 管理
```

## AAX SDK 2.9.0 注意事项

SDK 2.9.0+ 与旧版的重要区别：

| 特性 | 旧版 SDK (< 2.9.0) | 新版 SDK (2.9.0+) |
|------|-------------------|-------------------|
| 库文件 | 预编译 AAXLibrary.lib | 源码编译 AAX_Exports.cpp |
| 头文件 | Interfaces/ | Interfaces/ + Interfaces/ACF/ |
| CMake 集成 | 链接预编译库 | 源码编译到插件中 |

`cmake/FindAAX.cmake` 已自动适配两种模式。

## 构建产物

### 构建成功后的产物路径

| 格式 | 路径 | 大小 |
|------|------|------|
| VST3 | `build_vst3/LianCore_artefacts/Release/VST3/LianCore.vst3/` | ~6.5 MB |
| AAX | `build_vst3/LianCore_artefacts/Release/AAX/LianCore.aaxplugin/` | ~6.8 MB |

## 安装路径

| 平台 | 格式 | 路径 |
|------|------|------|
| Windows | VST3 | `C:\Program Files\Common Files\VST3\LianCore.vst3\` |
| Windows | AAX | `C:\Program Files\Common Files\Avid\Audio\Plug-Ins\LianCore.aaxplugin\` |
| macOS | VST3 | `~/Library/Audio/Plug-Ins/VST3/LianCore.vst3/` |
| macOS | AU | `~/Library/Audio/Plug-Ins/Components/LianCore.component/` |
| macOS | AAX | `/Library/Application Support/Avid/Audio/Plug-Ins/LianCore.aaxplugin/` |

## 安装包生成

### Windows 安装包

```powershell
# 1. 打包所有产物
python scripts/package_release.py

# 2. 生成 NSIS 安装包 (需要安装 NSIS)
makensis release/installer.nsi

# 3. 签名安装包 (需要代码签名证书)
powershell -File release/sign.ps1
```

打包脚本 `scripts/package_release.py` 会自动搜索以下构建目录：
- `build/LianCore_artefacts/Release/VST3/` (F:\ 项目构建)
- `C:/LianCoreSrc/build_vst3/LianCore_artefacts/Release/VST3/` (ASCII 路径构建)
- `build/LianCore_artefacts/Release/AAX/` (F:\ 项目构建)
- `C:/LianCoreSrc/build_vst3/LianCore_artefacts/Release/AAX/` (ASCII 路径构建)

## 验证

1. 启动 Pro Tools
2. 创建新乐器轨道
3. 在插件列表中找到 "LianCore" (分类: Instrument)
4. 加载并测试音频输出
5. 验证所有预设可正常加载

## 注意事项

- AAX SDK 受 Avid NDA 保护，不可公开分发
- AAX 插件需要 Pro Tools 12.0+
- 首次加载可能需要 Pro Tools 重新扫描插件
- CMakeLists.txt 已配置 FORMATS VST3 AAX AU
- AAX SDK 查找模块: `cmake/FindAAX.cmake`
- 构建路径必须使用纯 ASCII 字符（避免中文路径）
- 如果无 AAX SDK，构建会自动跳过 AAX 格式（仅构建 VST3/AU）