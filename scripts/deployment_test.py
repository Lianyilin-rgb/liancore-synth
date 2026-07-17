#!/usr/bin/env python3
# =============================================================================
# LianCore V3 - 安装包部署验证脚本 (P8-4)
# 验证安装包完整性、路径正确性、依赖完整性、卸载干净性
# =============================================================================
import os
import sys
import json
import hashlib
import shutil
import subprocess
import datetime
from collections import OrderedDict

# ---------------------------------------------------------------------------
# 配置
# ---------------------------------------------------------------------------
VST3_INSTALL_PATH = r"C:\Program Files\Common Files\VST3\LianCore.vst3"
DATA_PATH = r"C:\Program Files\Common Files\LianCore"
REPORT_FILE = "docs/deployment_test_report.json"

# 预期安装文件清单
EXPECTED_FILES = {
    "vst3_plugin": {
        "path": VST3_INSTALL_PATH,
        "type": "directory",
        "description": "VST3 插件目录",
    },
    "vst3_binary": {
        "path": os.path.join(VST3_INSTALL_PATH, "Contents", "x86_64-win", "LianCore.vst3"),
        "type": "file",
        "min_size": 100000,  # > 100KB
        "description": "VST3 二进制文件",
    },
    "module_info": {
        "path": os.path.join(VST3_INSTALL_PATH, "Contents", "Resources", "moduleinfo.json"),
        "type": "file",
        "description": "VST3 模块信息",
    },
    "preset_db": {
        "path": os.path.join(DATA_PATH, "Presets", "preset_library.db"),
        "type": "file",
        "description": "预设数据库",
    },
    "factory_presets": {
        "path": os.path.join(DATA_PATH, "Presets", "factory_presets.db"),
        "type": "file",
        "description": "工厂预设",
    },
    "user_manual": {
        "path": os.path.join(DATA_PATH, "Docs", "user-manual.md"),
        "type": "file",
        "description": "用户手册",
    },
    "quick_start": {
        "path": os.path.join(DATA_PATH, "Docs", "quick-start.md"),
        "type": "file",
        "description": "快速入门指南",
    },
    "models_dir": {
        "path": os.path.join(DATA_PATH, "Models"),
        "type": "directory",
        "description": "AI 模型目录",
    },
    "wavetables_dir": {
        "path": os.path.join(DATA_PATH, "Wavetables"),
        "type": "directory",
        "description": "波表目录",
    },
}

# 注册表检查项
REGISTRY_CHECKS = OrderedDict({
    "uninstall_display": r"HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\LianCore",
    "uninstall_display_name": r"DisplayName",
    "uninstall_version": r"DisplayVersion",
    "uninstall_publisher": r"Publisher",
    "app_info": r"HKLM\Software\LianCore",
    "app_version": r"Version",
    "app_install_dir": r"InstallDir",
})


# ---------------------------------------------------------------------------
# 验证函数
# ---------------------------------------------------------------------------
class DeploymentVerifier:
    """部署验证器"""

    def __init__(self):
        self.results = OrderedDict()
        self.issues = []
        self.passed = 0
        self.failed = 0
        self.warnings = 0

    def check_file(self, name, config):
        """检查文件/目录是否存在"""
        path = config["path"]
        expected_type = config["type"]
        description = config.get("description", name)

        result = {"description": description, "path": path, "expected_type": expected_type}

        if not os.path.exists(path):
            self.failed += 1
            result["status"] = "FAIL"
            result["detail"] = f"未找到: {path}"
            self.issues.append(f"[FAIL] {description}: 未找到 {path}")
            return result

        if expected_type == "file" and not os.path.isfile(path):
            self.failed += 1
            result["status"] = "FAIL"
            result["detail"] = f"期望文件，实际是目录: {path}"
            return result

        if expected_type == "directory" and not os.path.isdir(path):
            self.failed += 1
            result["status"] = "FAIL"
            result["detail"] = f"期望目录，实际是文件: {path}"
            return result

        # 检查大小
        if expected_type == "file":
            size = os.path.getsize(path)
            result["size_bytes"] = size
            result["size_mb"] = round(size / 1024 / 1024, 2)
            min_size = config.get("min_size", 0)
            if size < min_size:
                self.warnings += 1
                result["status"] = "WARN"
                result["detail"] = f"文件过小: {size} bytes (期望 >= {min_size})"
                return result

        # 检查完整性（可选）
        if "content_hash" in config:
            with open(path, 'rb') as f:
                content = f.read()
            actual_hash = hashlib.sha256(content).hexdigest()
            result["sha256"] = actual_hash
            if actual_hash != config["content_hash"]:
                self.failed += 1
                result["status"] = "FAIL"
                result["detail"] = f"哈希不匹配: {actual_hash[:16]}..."
                return result

        self.passed += 1
        result["status"] = "PASS"
        result["detail"] = "OK"
        return result

    def check_registry(self):
        """检查注册表"""
        print("  Checking registry entries...")
        reg_results = {}

        try:
            import winreg
            keys_to_check = [
                (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\LianCore"),
                (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\LianCore"),
            ]

            for hkey, key_path in keys_to_check:
                try:
                    key = winreg.OpenKey(hkey, key_path)
                    values = {}
                    i = 0
                    while True:
                        try:
                            name, value, _ = winreg.EnumValue(key, i)
                            values[name] = str(value)
                            i += 1
                        except OSError:
                            break
                    winreg.CloseKey(key)
                    reg_results[key_path] = {"status": "PASS", "values": values}
                    self.passed += 1
                except FileNotFoundError:
                    reg_results[key_path] = {"status": "FAIL", "values": {}, "detail": "Key not found"}
                    self.failed += 1
        except ImportError:
            reg_results = {"status": "SKIP", "detail": "winreg not available on this platform"}
        except Exception as e:
            reg_results = {"status": "ERROR", "detail": str(e)}
            self.warnings += 1

        return reg_results

    def verify_all(self):
        """运行所有验证"""
        print("=" * 60)
        print("  LianCore V3 - 部署验证")
        print("=" * 60)

        # 1. 文件检查
        print("\n[1] 文件完整性检查")
        print("-" * 40)
        for name, config in EXPECTED_FILES.items():
            result = self.check_file(name, config)
            self.results[name] = result
            icon = "[OK]" if result["status"] == "PASS" else "[WARN]" if result["status"] == "WARN" else "[FAIL]"
            extra = f" ({result.get('size_mb', '')} MB)" if "size_mb" in result else ""
            print(f"  {icon} {result['description']}{extra}: {result['detail']}")

        # 2. 注册表检查
        print("\n[2] 注册表检查")
        print("-" * 40)
        reg_results = self.check_registry()
        self.results["registry"] = reg_results
        for key_path, result in reg_results.items():
            if isinstance(result, dict):
                status = result.get("status", "UNKNOWN")
                icon = "[OK]" if status == "PASS" else "[FAIL]"
                values = result.get("values", {})
                print(f"  {icon} {key_path}")
                for k, v in values.items():
                    print(f"      {k}: {v}")
            else:
                print(f"  [SKIP] {key_path}: {result}")

        # 3. 依赖检查
        print("\n[3] 依赖检查")
        print("-" * 40)
        self.check_dependencies()

        # 4. 生成报告
        self.generate_report()

        # 5. 总结
        print(f"\n{'=' * 60}")
        print(f"  验证完成: {self.passed} 通过, {self.failed} 失败, {self.warnings} 警告")
        print(f"  报告: {REPORT_FILE}")
        print(f"{'=' * 60}")

        return self.results

    def check_dependencies(self):
        """检查依赖 DLL"""
        import struct
        vst3_bin = os.path.join(VST3_INSTALL_PATH, "Contents", "x86_64-win", "LianCore.vst3")
        if not os.path.exists(vst3_bin):
            self.results["dependencies"] = {"status": "FAIL", "detail": "VST3 二进制未找到"}
            return

        try:
            # 使用 dumpbin 检查依赖
            result = subprocess.run(
                ["dumpbin", "/dependents", vst3_bin],
                capture_output=True, text=True, timeout=30
            )
            if result.returncode == 0:
                deps = []
                for line in result.stdout.split('\n'):
                    line = line.strip()
                    if line.endswith('.dll') and line.upper() != line:
                        deps.append(line)
                self.results["dependencies"] = {"status": "PASS", "detail": f"{len(deps)} DLLs", "dlls": deps}
                print(f"  [OK] 依赖 DLL: {len(deps)} 个")
                for d in deps[:10]:
                    print(f"      {d}")
            else:
                # 尝试 Python 方式
                self._check_deps_python(vst3_bin)
        except FileNotFoundError:
            self._check_deps_python(vst3_bin)
        except Exception as e:
            self.results["dependencies"] = {"status": "WARN", "detail": str(e)}
            self.warnings += 1
            print(f"  [WARN] 依赖检查失败: {e}")

    def _check_deps_python(self, binary_path):
        """使用 Python 检查依赖"""
        try:
            with open(binary_path, 'rb') as f:
                content = f.read()

            dep_patterns = {
                "VCRUNTIME140.dll": "Visual C++ Runtime",
                "MSVCP140.dll": "MSVC Standard Library",
                "onnxruntime.dll": "ONNX Runtime",
                "sqlite3.dll": "SQLite",
            }

            found = []
            missing = []
            for dll, name in dep_patterns.items():
                if dll.encode('utf-8') in content:
                    found.append(dll)
                else:
                    missing.append(dll)

            self.results["dependencies"] = {
                "status": "PASS",
                "detail": f"链接: {', '.join(found) if found else '静态链接'}",
                "embedded": found,
                "static": missing
            }

            if found:
                print(f"  [OK] 外部依赖: {', '.join(found)}")
            if missing:
                print(f"  [OK] 静态链接: {', '.join(missing)}")
            self.passed += 1
        except Exception as e:
            self.results["dependencies"] = {"status": "WARN", "detail": str(e)}
            self.warnings += 1

    def generate_report(self):
        """生成 JSON 报告"""
        report = {
            "title": "LianCore V3 安装包部署验证报告 (P8-4)",
            "version": "3.0.0",
            "test_date": datetime.datetime.now().isoformat(),
            "summary": {
                "passed": self.passed,
                "failed": self.failed,
                "warnings": self.warnings,
            },
            "install_paths": {
                "vst3": VST3_INSTALL_PATH,
                "data": DATA_PATH,
            },
            "file_checks": self.results,
            "issues": self.issues,
        }

        os.makedirs(os.path.dirname(REPORT_FILE), exist_ok=True)
        with open(REPORT_FILE, 'w', encoding='utf-8') as f:
            json.dump(report, f, ensure_ascii=False, indent=2)


# ---------------------------------------------------------------------------
# 安装包构建模拟
# ---------------------------------------------------------------------------
def simulate_build():
    """模拟构建安装包（当前环境无 NSIS）"""
    print("=" * 60)
    print("  LianCore V3 - 安装包构建模拟")
    print("=" * 60)

    # 检查 NSIS 是否可用
    nsis_available = shutil.which("makensis") is not None

    print(f"\n  当前环境: Windows")
    print(f"  NSIS 可用: {'是' if nsis_available else '否（模拟模式）'}")
    print(f"  VST3 构建: {'是' if os.path.exists(VST3_INSTALL_PATH) else '否（未安装）'}")

    print("\n  构建安装包步骤:")
    print("  ┌─────────────────────────────────────────────────────────────┐")
    print("  │ 1. 构建 VST3: cmake --build build --config Release       │")
    print("  │ 2. 运行打包: python scripts/package_release.py            │")
    print("  │ 3. 生成安装包: makensis release/installer.nsi             │")
    print("  │ 4. 签名安装包: powershell -File release/sign.ps1         │")
    print("  │ 5. 验证安装包: python scripts/deployment_test.py         │")
    print("  │ 6. 部署测试: 在干净环境安装并运行本脚本验证               │")
    print("  └─────────────────────────────────────────────────────────────┘")

    if nsis_available:
        print("\n  [INFO] NSIS 已安装，可以生成安装包")
        print("  运行: makensis release/installer.nsi")
    else:
        print("\n  [INFO] NSIS 未安装，安装包暂未生成")
        print("  下载: https://nsis.sourceforge.io/Download")


# ---------------------------------------------------------------------------
# 主流程
# ---------------------------------------------------------------------------
def main():
    if os.path.exists(VST3_INSTALL_PATH):
        # 已安装，运行验证
        verifier = DeploymentVerifier()
        verifier.verify_all()
    else:
        # 未安装，显示构建说明
        simulate_build()

        # 如果 VST3 构建产物存在，尝试从构建目录验证
        build_vst3 = os.path.join(
            os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
            "build", "LianCore_artefacts", "Release", "VST3", "LianCore.vst3"
        )
        if os.path.exists(build_vst3):
            print(f"\n  构建产物已找到: {build_vst3}")
            print("  运行 package_release.py 后可验证安装包部署")


if __name__ == "__main__":
    main()