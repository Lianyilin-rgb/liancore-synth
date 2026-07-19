#!/usr/bin/env python3
"""
macOS 插件包 Info.plist 和 PkgInfo 生成脚本
用于 CI 工作流中为 VST3 和 AU 插件包创建必需的元数据文件.

用法:
    python3 scripts/create_macos_plist.py <vst3_dir> <au_dir>

示例:
    python3 scripts/create_macos_plist.py \
        build/LianCore_artefacts/Release/VST3/LianCore.vst3/Contents \
        build/LianCore_artefacts/Release/AU/LianCore.component/Contents
"""
import plistlib
import os
import sys


def create_vst3_plist(output_dir: str) -> None:
    """为 VST3 插件包创建 Info.plist 和 PkgInfo."""
    plist = {
        "CFBundleDevelopmentRegion": "English",
        "CFBundleExecutable": "LianCore",
        "CFBundleIdentifier": "com.lianyilin.liancore.vst3",
        "CFBundleInfoDictionaryVersion": "6.0",
        "CFBundleName": "LianCore",
        "CFBundlePackageType": "BNDL",
        "CFBundleShortVersionString": "3.0.0",
        "CFBundleVersion": "3.0.0",
        "NSHighResolutionCapable": True,
    }

    info_path = os.path.join(output_dir, "Info.plist")
    with open(info_path, "wb") as f:
        plistlib.dump(plist, f)
    print(f"  [OK] VST3 Info.plist -> {info_path}")

    pkg_path = os.path.join(output_dir, "PkgInfo")
    with open(pkg_path, "w") as f:
        f.write("BNDL????")
    print(f"  [OK] VST3 PkgInfo -> {pkg_path}")


def create_au_plist(output_dir: str) -> None:
    """为 AU 插件包创建 Info.plist 和 PkgInfo."""
    plist = {
        "CFBundleDevelopmentRegion": "English",
        "CFBundleExecutable": "LianCore",
        "CFBundleIdentifier": "com.lianyilin.liancore.au",
        "CFBundleInfoDictionaryVersion": "6.0",
        "CFBundleName": "LianCore",
        "CFBundlePackageType": "BNDL",
        "CFBundleShortVersionString": "3.0.0",
        "CFBundleVersion": "3.0.0",
        "NSHighResolutionCapable": True,
        "AudioComponents": [
            {
                "description": "LianCore V3",
                "factoryFunction": "LianCoreAUFactory",
                "manufacturer": "LnCr",
                "name": "LianCore V3",
                "subtype": "LnCr",
                "type": "aumu",
                "version": 196608,
            }
        ],
    }

    info_path = os.path.join(output_dir, "Info.plist")
    with open(info_path, "wb") as f:
        plistlib.dump(plist, f)
    print(f"  [OK] AU Info.plist -> {info_path}")

    pkg_path = os.path.join(output_dir, "PkgInfo")
    with open(pkg_path, "w") as f:
        f.write("BNDL????")
    print(f"  [OK] AU PkgInfo -> {pkg_path}")


def main() -> None:
    if len(sys.argv) != 3:
        print(f"用法: {sys.argv[0]} <vst3_contents_dir> <au_contents_dir>")
        sys.exit(1)

    vst3_dir = sys.argv[1]
    au_dir = sys.argv[2]

    print(f"=== 创建 macOS 插件包元数据 ===")
    print(f"  VST3 Contents: {vst3_dir}")
    print(f"  AU Contents:   {au_dir}")

    os.makedirs(vst3_dir, exist_ok=True)
    os.makedirs(au_dir, exist_ok=True)

    create_vst3_plist(vst3_dir)
    create_au_plist(au_dir)

    print(f"=== 完成 ===")


if __name__ == "__main__":
    main()