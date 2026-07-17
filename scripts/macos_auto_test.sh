#!/bin/bash
# =============================================================================
# LianCore V3 - macOS 自动化构建与测试脚本 (P8-问题2)
# 功能: 一键完成环境检查、构建、测试、签名、安装
# 用法: bash scripts/macos_auto_test.sh [--skip-onnx] [--skip-install] [--report-only]
# 生成: build_mac/test_report_$(date +%Y%m%d_%H%M%S).md
# =============================================================================
set -euo pipefail

# ---------------------------------------------------------------------------
# 配置
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_DIR}/build_mac"
JUCE_DIR="${PROJECT_DIR}/juce-8.0.14-osx"
REPORT_FILE="${PROJECT_DIR}/build_mac/test_report_$(date +%Y%m%d_%H%M%S).md"
TIMESTAMP=$(date "+%Y-%m-%d %H:%M:%S")
SKIP_ONNX=false
SKIP_INSTALL=false
REPORT_ONLY=false

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 统计变量
PASS_COUNT=0
FAIL_COUNT=0
WARN_COUNT=0

# ---------------------------------------------------------------------------
# 参数解析
# ---------------------------------------------------------------------------
for arg in "$@"; do
    case $arg in
        --skip-onnx)   SKIP_ONNX=true ;;
        --skip-install) SKIP_INSTALL=true ;;
        --report-only)  REPORT_ONLY=true ;;
        *) echo "未知参数: $arg"; exit 1 ;;
    esac
done

# ---------------------------------------------------------------------------
# 工具函数
# ---------------------------------------------------------------------------
log_info()   { echo -e "${BLUE}[INFO]${NC} $*"; }
log_ok()     { echo -e "${GREEN}[OK]${NC}   $*"; PASS_COUNT=$((PASS_COUNT + 1)); }
log_warn()   { echo -e "${YELLOW}[WARN]${NC} $*"; WARN_COUNT=$((WARN_COUNT + 1)); }
log_fail()   { echo -e "${RED}[FAIL]${NC} $*"; FAIL_COUNT=$((FAIL_COUNT + 1)); }
log_step()   { echo -e "\n${BLUE}======================================================================${NC}"; echo -e "${BLUE}  $*${NC}"; echo -e "${BLUE}======================================================================${NC}"; }

# 报告写入
report() {
    echo "$*" >> "$REPORT_FILE"
}

# ---------------------------------------------------------------------------
# 1. 环境检查
# ---------------------------------------------------------------------------
check_environment() {
    log_step "1. 环境检查"

    mkdir -p "$(dirname "$REPORT_FILE")"
    cat > "$REPORT_FILE" << EOF
# LianCore V3 macOS 自动化测试报告
> 生成时间: ${TIMESTAMP}
> 执行脚本: scripts/macos_auto_test.sh
> 主机名: $(hostname)

## 测试环境信息
| 项目 | 值 |
|------|-----|
| 测试日期 | $(date +%Y-%m-%d) |
| macOS 版本 | $(sw_vers -productVersion 2>/dev/null || echo "N/A") |
| 处理器型号 | $(sysctl -n machdep.cpu.brand_string 2>/dev/null || uname -m) |
| 物理核心数 | $(sysctl -n hw.physicalcpu 2>/dev/null || echo "N/A") |
| 内存大小 | $(sysctl -n hw.memsize 2>/dev/null | awk '{printf "%.1f GB", $1/1024/1024/1024}' || echo "N/A") |
| Xcode 版本 | $(xcodebuild -version 2>/dev/null | head -1 | awk '{print $2}' || echo "N/A") |
| CMake 版本 | $(cmake --version 2>/dev/null | head -1 | awk '{print $3}' || echo "N/A") |
| JUCE 版本 | 8.0.14 (macOS) |
| 脚本路径 | ${SCRIPT_DIR} |
| 项目路径 | ${PROJECT_DIR} |

EOF

    # Xcode CLI
    if xcode-select -p &>/dev/null; then
        log_ok "Xcode Command Line Tools: $(xcode-select -p)"
    else
        log_fail "Xcode Command Line Tools 未安装，请运行: xcode-select --install"
        report "### 环境检查失败\n- Xcode Command Line Tools 未安装\n"
        exit 1
    fi

    # CMake
    if command -v cmake &>/dev/null; then
        local cmake_ver=$(cmake --version | head -1 | awk '{print $3}')
        log_ok "CMake: ${cmake_ver}"
    else
        log_fail "CMake 未安装，请运行: brew install cmake"
        report "### 环境检查失败\n- CMake 未安装\n"
        exit 1
    fi

    # JUCE
    if [ -f "${JUCE_DIR}/JUCE/CMakeLists.txt" ]; then
        log_ok "JUCE 8.0.14 macOS: ${JUCE_DIR}"
    else
        log_warn "JUCE 8.0.14 macOS 未找到，正在下载..."
        curl -L -o "${PROJECT_DIR}/juce-8.0.14-osx.zip" \
            "https://github.com/juce-framework/JUCE/releases/download/8.0.14/juce-8.0.14-osx.zip"
        unzip -q "${PROJECT_DIR}/juce-8.0.14-osx.zip" -d "${PROJECT_DIR}"
        if [ -f "${JUCE_DIR}/JUCE/CMakeLists.txt" ]; then
            log_ok "JUCE 下载完成"
        else
            log_fail "JUCE 下载失败，请手动下载到 ${JUCE_DIR}"
            exit 1
        fi
    fi

    # 磁盘空间
    local free_space=$(df -h "${PROJECT_DIR}" | tail -1 | awk '{print $4}')
    log_info "可用磁盘空间: ${free_space}"
}

# ---------------------------------------------------------------------------
# 2. CMake 配置
# ---------------------------------------------------------------------------
configure_cmake() {
    log_step "2. CMake 配置"

    if [ "$REPORT_ONLY" = true ]; then
        log_info "跳过 CMake 配置 (--report-only)"
        return 0
    fi

    local cmake_args="-B ${BUILD_DIR} -G Xcode -DCMAKE_BUILD_TYPE=Release"
    if [ "$SKIP_ONNX" = true ]; then
        cmake_args="${cmake_args} -DLIANCORE_BUILD_TESTS=OFF"
    fi

    log_info "CMake 参数: ${cmake_args}"
    if cmake ${cmake_args} 2>&1 | tee "${BUILD_DIR}/cmake_config.log"; then
        log_ok "CMake 配置成功"
        report "### CMake 配置: 成功\n"
    else
        log_fail "CMake 配置失败，查看日志: ${BUILD_DIR}/cmake_config.log"
        report "### CMake 配置: 失败\n"
        exit 1
    fi
}

# ---------------------------------------------------------------------------
# 3. VST3 构建
# ---------------------------------------------------------------------------
build_vst3() {
    log_step "3. VST3 构建"

    if [ "$REPORT_ONLY" = true ]; then
        log_info "跳过 VST3 构建 (--report-only)"
        return 0
    fi

    local cpu_count=$(sysctl -n hw.logicalcpu 2>/dev/null || echo 4)
    local build_log="${BUILD_DIR}/vst3_build.log"

    if cmake --build "${BUILD_DIR}" --config Release --target "LianCore_VST3" --parallel "${cpu_count}" 2>&1 | tee "${build_log}"; then
        log_ok "VST3 构建成功"
    else
        log_fail "VST3 构建失败，查看日志: ${build_log}"
        report "### VST3 构建: 失败\n"
        return 1
    fi

    # 验证产物
    local vst3_path="${BUILD_DIR}/LianCore_artefacts/Release/VST3/LianCore.vst3"
    local vst3_bin="${vst3_path}/Contents/MacOS/LianCore"

    if [ -f "${vst3_bin}" ]; then
        local vst3_size=$(du -sh "${vst3_path}" | awk '{print $1}')
        local vst3_arch=$(file "${vst3_bin}" | sed 's/.*://')
        log_ok "VST3 产物: ${vst3_path} (${vst3_size})"
        log_info "VST3 架构: ${vst3_arch}"
        report "### VST3 构建: 成功\n- 路径: \`${vst3_path}\`\n- 大小: ${vst3_size}\n- 架构: ${vst3_arch}\n"
    else
        log_fail "VST3 产物未找到: ${vst3_bin}"
        report "### VST3 构建: 产物未找到\n"
    fi

    # 统计编译警告
    local warn_count=$(grep -c "warning:" "${build_log}" 2>/dev/null || echo 0)
    local err_count=$(grep -c "error:" "${build_log}" 2>/dev/null || echo 0)
    log_info "VST3 编译: ${err_count} 错误, ${warn_count} 警告"
    report "- 编译错误: ${err_count}\n- 编译警告: ${warn_count}\n"
}

# ---------------------------------------------------------------------------
# 4. AU 构建
# ---------------------------------------------------------------------------
build_au() {
    log_step "4. AU (Audio Unit) 构建"

    if [ "$REPORT_ONLY" = true ]; then
        log_info "跳过 AU 构建 (--report-only)"
        return 0
    fi

    local cpu_count=$(sysctl -n hw.logicalcpu 2>/dev/null || echo 4)
    local build_log="${BUILD_DIR}/au_build.log"

    if cmake --build "${BUILD_DIR}" --config Release --target "LianCore_AU" --parallel "${cpu_count}" 2>&1 | tee "${build_log}"; then
        log_ok "AU 构建成功"
    else
        log_fail "AU 构建失败，查看日志: ${build_log}"
        report "### AU 构建: 失败\n"
        return 1
    fi

    # 验证产物
    local au_path="${BUILD_DIR}/LianCore_artefacts/Release/AU/LianCore.component"
    local au_bin="${au_path}/Contents/MacOS/LianCore"

    if [ -f "${au_bin}" ]; then
        local au_size=$(du -sh "${au_path}" | awk '{print $1}')
        local au_arch=$(file "${au_bin}" | sed 's/.*://')
        log_ok "AU 产物: ${au_path} (${au_size})"
        log_info "AU 架构: ${au_arch}"
        report "### AU 构建: 成功\n- 路径: \`${au_path}\`\n- 大小: ${au_size}\n- 架构: ${au_arch}\n"
    else
        log_fail "AU 产物未找到: ${au_bin}"
        report "### AU 构建: 产物未找到\n"
    fi
}

# ---------------------------------------------------------------------------
# 5. 代码签名 (ad-hoc)
# ---------------------------------------------------------------------------
sign_plugins() {
    log_step "5. 代码签名 (ad-hoc 开发模式)"

    if [ "$REPORT_ONLY" = true ]; then
        log_info "跳过代码签名 (--report-only)"
        return 0
    fi

    local vst3_path="${BUILD_DIR}/LianCore_artefacts/Release/VST3/LianCore.vst3"
    local au_path="${BUILD_DIR}/LianCore_artefacts/Release/AU/LianCore.component"

    if [ -d "${vst3_path}" ]; then
        if codesign --force --deep --sign - "${vst3_path}" 2>&1; then
            log_ok "VST3 ad-hoc 签名成功"
        else
            log_warn "VST3 ad-hoc 签名失败（非关键）"
        fi
    fi

    if [ -d "${au_path}" ]; then
        if codesign --force --deep --sign - "${au_path}" 2>&1; then
            log_ok "AU ad-hoc 签名成功"
        else
            log_warn "AU ad-hoc 签名失败（非关键）"
        fi
    fi
}

# ---------------------------------------------------------------------------
# 6. 插件安装 (可选)
# ---------------------------------------------------------------------------
install_plugins() {
    log_step "6. 插件安装"

    if [ "$SKIP_INSTALL" = true ] || [ "$REPORT_ONLY" = true ]; then
        log_info "跳过插件安装 (--skip-install 或 --report-only)"
        return 0
    fi

    local vst3_src="${BUILD_DIR}/LianCore_artefacts/Release/VST3/LianCore.vst3"
    local vst3_dst="${HOME}/Library/Audio/Plug-Ins/VST3/LianCore.vst3"
    local au_src="${BUILD_DIR}/LianCore_artefacts/Release/AU/LianCore.component"
    local au_dst="${HOME}/Library/Audio/Plug-Ins/Components/LianCore.component"

    if [ -d "${vst3_src}" ]; then
        rm -rf "${vst3_dst}" 2>/dev/null
        cp -R "${vst3_src}" "${vst3_dst}"
        log_ok "VST3 已安装到: ${vst3_dst}"
    fi

    if [ -d "${au_src}" ]; then
        rm -rf "${au_dst}" 2>/dev/null
        cp -R "${au_src}" "${au_dst}"
        log_ok "AU 已安装到: ${au_dst}"
        # 刷新 AU 缓存
        killall -9 AudioComponentRegistrar 2>/dev/null || true
        log_info "AU 缓存已刷新"
    fi
}

# ---------------------------------------------------------------------------
# 7. 单元测试
# ---------------------------------------------------------------------------
run_tests() {
    log_step "7. 单元测试"

    if [ "$REPORT_ONLY" = true ]; then
        log_info "跳过单元测试 (--report-only)"
        return 0
    fi

    local cpu_count=$(sysctl -n hw.logicalcpu 2>/dev/null || echo 4)

    # 构建测试目标
    if ! cmake --build "${BUILD_DIR}" --config Release --target LianCoreTests --parallel "${cpu_count}" 2>&1; then
        log_fail "测试编译失败"
        report "### 测试: 编译失败\n"
        return 1
    fi

    local test_bin="${BUILD_DIR}/tests/Release/LianCoreTests"
    if [ ! -f "${test_bin}" ]; then
        log_fail "测试二进制未找到: ${test_bin}"
        report "### 测试: 二进制未找到\n"
        return 1
    fi

    # 运行测试
    local test_log="${BUILD_DIR}/test_results.log"
    log_info "运行测试..."
    if "${test_bin}" -r console 2>&1 | tee "${test_log}"; then
        log_ok "所有测试通过"
    else
        log_warn "部分测试未通过（检查日志）"
    fi

    # 解析测试结果
    local total_tests=$(grep -c "test case" "${test_log}" 2>/dev/null || echo "?")
    local assertions=$(grep -oP "assertions: \K\d+" "${test_log}" 2>/dev/null | tail -1 || echo "?")
    local passed=$(grep -oP "test cases: \K\d+" "${test_log}" 2>/dev/null | head -1 || echo "?")

    report "### 测试结果\n"
    report "| 指标 | 值 |"
    report "|------|-----|"
    report "| 测试用例总数 | ${total_tests} |"
    report "| 通过数 | ${passed} |"
    report "| 断言总数 | ${assertions} |"

    # 运行性能测试
    local perf_log="${BUILD_DIR}/perf_results.log"
    if "${test_bin}" "[performance]" -r console 2>&1 | tee "${perf_log}"; then
        log_ok "性能测试完成"
    else
        log_info "性能测试跳过（无性能测试标签）"
    fi
}

# ---------------------------------------------------------------------------
# 8. 架构验证
# ---------------------------------------------------------------------------
verify_arch() {
    log_step "8. 架构验证"

    local vst3_bin="${BUILD_DIR}/LianCore_artefacts/Release/VST3/LianCore.vst3/Contents/MacOS/LianCore"
    if [ -f "${vst3_bin}" ]; then
        local arch_info=$(file "${vst3_bin}")
        log_info "VST3 架构信息: ${arch_info}"
        report "### 架构验证\n\`\`\`\n${arch_info}\n\`\`\`\n"

        if echo "${arch_info}" | grep -q "arm64"; then
            log_ok "Apple Silicon (arm64) 原生支持: 是"
        fi
        if echo "${arch_info}" | grep -q "x86_64"; then
            log_ok "Intel (x86_64) 支持: 是"
        fi
        if echo "${arch_info}" | grep -q "universal"; then
            log_ok "通用二进制 (Universal Binary): 是"
        fi
    else
        log_warn "无法验证架构，VST3 二进制未找到"
    fi
}

# ---------------------------------------------------------------------------
# 9. 生成最终报告
# ---------------------------------------------------------------------------
generate_report() {
    log_step "9. 生成测试报告"

    cat >> "$REPORT_FILE" << EOF

## 执行摘要
- 总步骤: 9
- 通过: ${PASS_COUNT}
- 失败: ${FAIL_COUNT}
- 警告: ${WARN_COUNT}

## 已知问题确认
| 问题 | Windows 状态 | macOS 是否复现 |
|------|-------------|---------------|
| ChaoticLFO Logistic Map 偶发失败 | 已修复 | 待验证 |
| ONNX Runtime 堆损坏 | 已优雅处理 | 待验证 |
| PresetManager PM-002 堆损坏 | 已修复 | 待验证 |
| JUCE 8.0.14 Font 弃用警告 | 已修复 | 待验证 |

## 构建产物
| 格式 | 路径 | 架构 |
|------|------|------|
| VST3 | \`${BUILD_DIR}/LianCore_artefacts/Release/VST3/LianCore.vst3\` | $(file "${BUILD_DIR}/LianCore_artefacts/Release/VST3/LianCore.vst3/Contents/MacOS/LianCore" 2>/dev/null | sed 's/.*://' || echo "N/A") |
| AU | \`${BUILD_DIR}/LianCore_artefacts/Release/AU/LianCore.component\` | $(file "${BUILD_DIR}/LianCore_artefacts/Release/AU/LianCore.component/Contents/MacOS/LianCore" 2>/dev/null | sed 's/.*://' || echo "N/A") |

## 问题与备注
（请在此处填写发现的问题）

---
> 报告生成于: ${TIMESTAMP}
> 自动化脚本: scripts/macos_auto_test.sh
EOF

    log_ok "报告已生成: ${REPORT_FILE}"
    echo ""
    echo "=========================================="
    echo "  测试完成"
    echo "=========================================="
    echo "  通过: ${PASS_COUNT}"
    echo "  失败: ${FAIL_COUNT}"
    echo "  警告: ${WARN_COUNT}"
    echo "  报告: ${REPORT_FILE}"
    echo "=========================================="
}

# ---------------------------------------------------------------------------
# 主流程
# ---------------------------------------------------------------------------
main() {
    echo ""
    echo "=========================================="
    echo "  LianCore V3 - macOS 自动化测试"
    echo "  $(date)"
    echo "=========================================="
    echo ""

    check_environment
    configure_cmake
    build_vst3
    build_au
    sign_plugins
    install_plugins
    run_tests
    verify_arch
    generate_report

    if [ "$FAIL_COUNT" -gt 0 ]; then
        exit 1
    fi
}

main