#!/usr/bin/env python3
# =============================================================================
# LianCore V3 - DAW 兼容性测试 (P8-3)
# 验证 VST3 插件在主流 DAW 中的兼容性
# =============================================================================
import os
import sys
import json
import struct
import subprocess
import datetime
from collections import OrderedDict

# ---------------------------------------------------------------------------
# 配置
# ---------------------------------------------------------------------------
VST3_PATH = "build/LianCore_artefacts/Release/VST3/LianCore.vst3"
REPORT_FILE = "docs/daw_compatibility_report.json"
DAW_LIST = ["Ableton Live", "FL Studio", "Cubase", "Studio One", "Reaper", "Bitwig Studio"]

# 检查项定义
CHECK_ITEMS = OrderedDict([
    ("vst3_binary_exists", "VST3 二进制文件存在"),
    ("vst3_architecture", "架构验证 (x86_64)"),
    ("vst3_module_info", "模块信息完整性"),
    ("vst3_factory", "插件工厂函数"),
    ("plugin_scan", "DAW 插件扫描"),
    ("plugin_load", "插件加载"),
    ("gui_display", "GUI 正常显示"),
    ("param_control", "参数控制响应"),
    ("audio_output", "音频输出正常"),
    ("preset_switch", "预设切换"),
    ("automation", "自动化参数"),
    ("midi_learn", "MIDI 学习"),
    ("project_save", "工程保存/恢复"),
    ("plugin_unload", "插件卸载"),
    ("memory_leak", "无内存泄漏"),
])


# ---------------------------------------------------------------------------
# VST3 二进制分析
# ---------------------------------------------------------------------------
class VST3Analyzer:
    """分析 VST3 二进制文件，检查兼容性"""

    def __init__(self, vst3_path):
        self.vst3_path = vst3_path
        self.results = {}
        self.issues = []
        self.binary_path = None

    def find_binary(self):
        """查找 VST3 包内的实际二进制文件"""
        if not os.path.exists(self.vst3_path):
            return None

        if os.path.isdir(self.vst3_path):
            # VST3 是 macOS bundle 或 Windows 目录
            for root, dirs, files in os.walk(self.vst3_path):
                for f in files:
                    if f.endswith('.vst3') or f == 'LianCore':
                        full = os.path.join(root, f)
                        if os.path.getsize(full) > 100000:  # > 100KB
                            self.binary_path = full
                            return full
                        # 也检查 .dll 扩展名
                    if f.endswith('.dll') and os.path.getsize(os.path.join(root, f)) > 100000:
                        self.binary_path = os.path.join(root, f)
                        return self.binary_path
        elif os.path.isfile(self.vst3_path):
            if self.vst3_path.endswith('.vst3') or self.vst3_path.endswith('.dll'):
                self.binary_path = self.vst3_path
                return self.binary_path

        return None

    def check_architecture(self):
        """检查二进制架构"""
        if not self.binary_path:
            return {"status": "FAIL", "detail": "未找到二进制文件"}

        try:
            with open(self.binary_path, 'rb') as f:
                header = f.read(2)
                if header == b'MZ':
                    # PE 文件 (Windows)
                    f.seek(0x3C)
                    pe_offset = struct.unpack('<I', f.read(4))[0]
                    f.seek(pe_offset + 4)
                    machine = struct.unpack('<H', f.read(2))[0]
                    arch_map = {0x8664: "x86_64", 0x014C: "x86", 0xAA64: "ARM64"}
                    arch = arch_map.get(machine, f"Unknown (0x{machine:04X})")
                    return {"status": "PASS", "detail": arch}
                elif header[:4] == b'\xcf\xfa\xed\xfe':
                    # Mach-O 64-bit
                    f.seek(4)
                    cpu_type = struct.unpack('<I', f.read(4))[0]
                    arch_map = {0x01000007: "x86_64", 0x0100000C: "arm64",
                               0x0100000D: "arm64e", 0x01000017: "x86_64h"}
                    arch = arch_map.get(cpu_type, f"Unknown (0x{cpu_type:08X})")
                    return {"status": "PASS", "detail": arch}
                else:
                    return {"status": "WARN", "detail": f"未知二进制格式"}
        except Exception as e:
            return {"status": "FAIL", "detail": str(e)}

    def check_factory_export(self):
        """检查 VST3 工厂函数导出"""
        if not self.binary_path:
            return {"status": "FAIL", "detail": "未找到二进制文件"}

        try:
            with open(self.binary_path, 'rb') as f:
                content = f.read()

            # 检查关键导出符号
            exports = [
                b"GetPluginFactory",
                b"ModuleEntry",
                b"InitDll",
                b"ExitDll",
            ]
            found = []
            for exp in exports:
                if exp in content:
                    found.append(exp.decode('ascii'))

            if found:
                return {"status": "PASS", "detail": f"找到 {len(found)} 个导出: {', '.join(found)}"}
            else:
                return {"status": "WARN", "detail": "未找到标准 VST3 导出符号"}
        except Exception as e:
            return {"status": "FAIL", "detail": str(e)}

    def check_module_info(self):
        """检查 VST3 moduleinfo.json"""
        info_path = os.path.join(self.vst3_path, "moduleinfo.json")
        if not os.path.exists(info_path):
            info_path = os.path.join(os.path.dirname(self.vst3_path), "moduleinfo.json")

        if not os.path.exists(info_path):
            return {"status": "WARN", "detail": "未找到 moduleinfo.json"}

        try:
            with open(info_path, 'r') as f:
                info = json.load(f)
            name = info.get("Name", "Unknown")
            version = info.get("Version", "Unknown")
            factory = info.get("Factory Info", {}).get("PFactoryInfo", {})
            return {
                "status": "PASS",
                "detail": f"{name} v{version}",
                "info": {
                    "name": name,
                    "version": version,
                    "vendor": factory.get("Vendor", "Unknown"),
                    "email": factory.get("Email", "Unknown"),
                    "url": factory.get("URL", "Unknown"),
                }
            }
        except Exception as e:
            return {"status": "FAIL", "detail": str(e)}

    def check_dependencies(self):
        """检查依赖 DLL"""
        if not self.binary_path:
            return {"status": "FAIL", "detail": "未找到二进制文件"}

        try:
            with open(self.binary_path, 'rb') as f:
                content = f.read()

            deps = []
            # 检查常见依赖
            dep_checks = [
                (b"VCRUNTIME", "Visual C++ Runtime"),
                (b"MSVCP", "MSVC Standard Library"),
                (b"onnxruntime", "ONNX Runtime"),
                (b"sqlite3", "SQLite"),
                (b"juce", "JUCE"),
            ]
            for pattern, name in dep_checks:
                if pattern in content:
                    deps.append(name)

            return {"status": "PASS", "detail": f"依赖: {', '.join(deps) if deps else '静态链接'}"}
        except Exception as e:
            return {"status": "FAIL", "detail": str(e)}

    def run_all_checks(self):
        """运行所有检查"""
        self.results["vst3_binary_exists"] = {
            "status": "PASS" if self.find_binary() else "FAIL",
            "detail": self.binary_path or "未找到二进制文件"
        }
        self.results["vst3_architecture"] = self.check_architecture()
        self.results["vst3_module_info"] = self.check_module_info()
        self.results["vst3_factory"] = self.check_factory_export()
        self.results["vst3_dependencies"] = self.check_dependencies()
        return self.results


# ---------------------------------------------------------------------------
# DAW 兼容性测试清单
# ---------------------------------------------------------------------------
def generate_daw_checklist():
    """生成 DAW 兼容性测试清单"""
    checklist = OrderedDict()

    for daw in DAW_LIST:
        checklist[daw] = OrderedDict()
        for item_id, item_name in CHECK_ITEMS.items():
            if item_id.startswith("vst3_") and "binary" in item_id:
                continue  # 跳过二进制层面的检查，仅保留 DAW 层面
            if item_id.startswith("vst3_"):
                continue
            checklist[daw][item_id] = {
                "name": item_name,
                "status": "PENDING",
                "notes": "",
                "daw_version": "",
                "os_version": "",
            }

    return checklist


# ---------------------------------------------------------------------------
# 兼容性已知问题检查
# ---------------------------------------------------------------------------
KNOWN_ISSUES = {
    "Ableton Live": [
        "VST3 插件扫描路径：C:\\Program Files\\Common Files\\VST3\\",
        "Live 11+ 对 VST3 支持良好，无需额外配置",
        "注意：Live 的 VST3 扫描在启动时进行，安装后需重启 Live",
    ],
    "FL Studio": [
        "FL Studio 21+ 完整支持 VST3",
        "插件管理器 → 查找插件 → 添加路径",
        "FL Studio 20 及以下版本对 VST3 支持有限",
    ],
    "Cubase": [
        "Cubase 是 VST3 标准的制定者，兼容性最佳",
        "VST3 插件自动识别，无需手动扫描",
        "Cubase 12+ 完全支持 VST3",
    ],
    "Studio One": [
        "Studio One 5+ 完整支持 VST3",
        "选项 → 位置 → VST 插件 → 重置黑名单",
        "如果插件未显示，检查 VST3 路径是否正确",
    ],
    "Reaper": [
        "Reaper 6+ 完整支持 VST3",
        "选项 → 首选项 → VST → 重新扫描",
        "Reaper 的 VST3 兼容性极佳，适合测试",
    ],
    "Bitwig Studio": [
        "Bitwig 原生支持 VST3",
        "Bitwig 的沙箱机制可能阻止某些文件访问",
        "预设库路径需要确保在 Bitwig 可访问的位置",
    ],
}


# ---------------------------------------------------------------------------
# 主流程
# ---------------------------------------------------------------------------
def main():
    print("=" * 60)
    print("  LianCore V3 - DAW 兼容性测试 (P8-3)")
    print("=" * 60)

    # 1. VST3 二进制分析
    print("\n[1] VST3 二进制分析")
    print("-" * 40)
    analyzer = VST3Analyzer(VST3_PATH)
    results = analyzer.run_all_checks()

    for check_id, result in results.items():
        status = result["status"]
        icon = "[OK]" if status == "PASS" else "[WARN]" if status == "WARN" else "[FAIL]"
        print(f"  {icon} {CHECK_ITEMS.get(check_id, check_id)}: {result['detail']}")
        if "info" in result:
            for k, v in result["info"].items():
                print(f"      {k}: {v}")

    # 2. DAW 兼容性清单
    print("\n[2] DAW 兼容性测试清单")
    print("-" * 40)
    checklist = generate_daw_checklist()

    for daw, items in checklist.items():
        print(f"\n  --- {daw} ---")
        if daw in KNOWN_ISSUES:
            for note in KNOWN_ISSUES[daw]:
                print(f"  [NOTE] {note}")
        for item_id, item in items.items():
            print(f"  [PENDING] {item['name']}")

    # 3. 生成测试报告
    # 合并所有结果
    all_results = {
        "title": "LianCore V3 DAW 兼容性测试报告 (P8-3)",
        "version": "3.0.0",
        "test_date": datetime.datetime.now().isoformat(),
        "vst3_path": VST3_PATH,
        "binary_analysis": analyzer.results,
        "daw_compatibility": checklist,
        "known_issues": KNOWN_ISSUES,
        "summary": {
            "total_daws": len(DAW_LIST),
            "total_checks_per_daw": len(CHECK_ITEMS) - 4,  # 减去二进制检查
            "daws_tested": 0,
            "checks_total": 0,
            "checks_passed": 0,
            "checks_failed": 0,
            "notes": "请在实际 DAW 环境中完成测试并填写状态"
        }
    }

    # 保存报告
    os.makedirs(os.path.dirname(REPORT_FILE), exist_ok=True)
    with open(REPORT_FILE, 'w', encoding='utf-8') as f:
        json.dump(all_results, f, ensure_ascii=False, indent=2)

    print(f"\n{'=' * 60}")
    print(f"  报告已保存: {REPORT_FILE}")
    print(f"  目标 DAW: {len(DAW_LIST)} 个")
    print(f"  每 DAW 检查项: {len(CHECK_ITEMS) - 4} 项")
    print(f"  请在各 DAW 中手动测试并更新报告")
    print(f"{'=' * 60}")

    # 4. 输出快速测试步骤
    print("\n[3] 快速手动测试步骤")
    print("-" * 40)
    print("""
  对于每个 DAW，请按以下步骤操作：

  1. 启动 DAW，等待插件扫描完成
  2. 创建乐器轨道，加载 LianCore V3
  3. 验证 GUI 正常显示（无崩溃、无黑屏）
  4. 用 MIDI 键盘弹奏，确认有音频输出
  5. 旋转 Cutoff 旋钮，确认参数变化响应
  6. 切换 3 个不同预设，确认加载正常
  7. 保存工程，关闭 DAW，重新打开，确认工程恢复
  8. 移除插件，确认无崩溃

  请在测试完成后更新报告中的状态。
  """)


if __name__ == "__main__":
    main()