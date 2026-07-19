#!/usr/bin/env python3
# =============================================================================
# LianCore V3 - GitHub Release 资源上传脚本
# 将预设库、波表库、AI 模型上传到 GitHub Release 作为附加资产
#
# 用法:
#   python scripts/upload_release_assets.py --token <GITHUB_TOKEN> [--tag v3.0.0]
#
# 由于这些文件体积巨大（预设库 3.67 GB），无法包含在 Git 仓库中，
# 因此需要作为 Release 附加资产单独上传。macOS CI 和 ResourceDownloader
# 将从 Release 下载这些资源。
# =============================================================================
import os
import sys
import argparse
import json
import urllib.request
import urllib.error
import time

REPO = "Lianyilin-rgb/liancore-synth"
API_BASE = f"https://api.github.com/repos/{REPO}"
UPLOAD_BASE = f"https://uploads.github.com/repos/{REPO}"

# 需要上传的资源文件列表
# (本地路径, 资源名称, 描述)
ASSETS = [
    ("data/preset_library_1M.db", "preset_library_1M.db", "百万级预设库主文件 (3.67 GB, 1,402,008 条预设) — 注意: 超过 2GB 需分片上传"),
    ("data/factory_presets.db", "factory_presets.db", "工厂预设库 (378 MB, 100,000 条预设)"),
    ("data/factory_wavetables.zip", "factory_wavetables.zip", "工厂波表库 (100 个 .wav 波表文件)"),
    ("models/liancore_ai_model.onnx", "liancore_ai_model.onnx", "LianCore AI 推理模型"),
    ("models/audio_encoder.onnx", "audio_encoder.onnx", "音频编码器 ONNX 模型"),
    ("models/param_regressor.onnx", "param_regressor.onnx", "参数回归器 ONNX 模型"),
    ("models/wavetable_vae_decoder.onnx", "wavetable_vae_decoder.onnx", "波表 VAE 解码器 ONNX 模型"),
    ("models/transformer_encoder.onnx", "transformer_encoder.onnx", "Transformer 编码器 ONNX 模型"),
]


def make_request(url, token, method="GET", data=None, headers_extra=None):
    """发送 GitHub API 请求"""
    headers = {
        "Authorization": f"Bearer {token}",
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "LianCore-Uploader/1.0",
    }
    if headers_extra:
        headers.update(headers_extra)

    req = urllib.request.Request(url, data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req) as resp:
            return json.loads(resp.read().decode())
    except urllib.error.HTTPError as e:
        body = e.read().decode()
        print(f"  HTTP {e.code}: {body}")
        return None


def get_release(tag, token):
    """获取指定 tag 的 Release"""
    url = f"{API_BASE}/releases/tags/{tag}"
    print(f"获取 Release: {tag}")
    return make_request(url, token)


def upload_asset(upload_url, filepath, asset_name, content_type, token):
    """上传单个资源文件到 Release"""
    # 检查文件是否存在
    if not os.path.exists(filepath):
        print(f"  [跳过] 文件不存在: {filepath}")
        return False

    file_size = os.path.getsize(filepath)
    file_size_mb = file_size / (1024 * 1024)
    file_size_gb = file_size / (1024 * 1024 * 1024)

    if file_size_gb >= 1.0:
        size_str = f"{file_size_gb:.2f} GB"
    else:
        size_str = f"{file_size_mb:.2f} MB"

    print(f"上传: {asset_name} ({size_str})")

    # 构建上传 URL（替换模板参数）
    asset_url = upload_url.replace("{?name,label}", f"?name={urllib.parse.quote(asset_name)}")

    headers = {
        "Content-Type": content_type,
        "Content-Length": str(file_size),
    }

    start_time = time.time()
    with open(filepath, "rb") as f:
        data = f.read()

    result = make_request(asset_url, token, method="POST", data=data, headers_extra=headers)
    elapsed = time.time() - start_time

    if result:
        print(f"  [成功] {asset_name} 上传完成 ({elapsed:.1f}s)")
        return True
    else:
        print(f"  [失败] {asset_name} 上传失败")
        return False


def create_wavetable_zip():
    """将 factory_wavetables/ 目录打包为 zip"""
    import zipfile

    zip_path = "data/factory_wavetables.zip"
    wav_dir = "data/factory_wavetables"

    if not os.path.isdir(wav_dir):
        print(f"  [跳过] 波表目录不存在: {wav_dir}")
        return False

    wav_files = [f for f in os.listdir(wav_dir) if f.endswith(".wav")]
    if not wav_files:
        print(f"  [跳过] 波表目录为空: {wav_dir}")
        return False

    print(f"打包波表库: {len(wav_files)} 个 .wav 文件 -> {zip_path}")
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for wav_file in wav_files:
            filepath = os.path.join(wav_dir, wav_file)
            zf.write(filepath, wav_file)

    zip_size = os.path.getsize(zip_path) / (1024 * 1024)
    print(f"  波表库 zip 大小: {zip_size:.2f} MB")
    return True


def main():
    parser = argparse.ArgumentParser(description="LianCore GitHub Release 资源上传工具")
    parser.add_argument("--token", required=True, help="GitHub Personal Access Token (需要 repo 权限)")
    parser.add_argument("--tag", default="v3.0.0", help="Release 标签 (默认: v3.0.0)")
    parser.add_argument("--skip-upload", action="store_true", help="仅检查文件，不实际上传")
    args = parser.parse_args()

    print("=" * 60)
    print("LianCore V3 - GitHub Release 资源上传")
    print(f"仓库: {REPO}")
    print(f"标签: {args.tag}")
    print("=" * 60)

    # 获取 Release
    release = get_release(args.tag, args.token)
    if not release:
        print(f"错误: 找不到 Release {args.tag}")
        print("请先创建 Release，或检查 tag 是否正确")
        sys.exit(1)

    upload_url = release.get("upload_url")
    if not upload_url:
        print("错误: Release 缺少 upload_url")
        sys.exit(1)

    print(f"Release ID: {release.get('id')}")
    print(f"已有资产: {len(release.get('assets', []))} 个")
    print()

    # 检查并打包波表库
    if not os.path.exists("data/factory_wavetables.zip"):
        create_wavetable_zip()

    if args.skip_upload:
        print("--skip-upload 已设置，仅检查文件:")
        for filepath, asset_name, description in ASSETS:
            if os.path.exists(filepath):
                size = os.path.getsize(filepath)
                if size >= 1e9:
                    print(f"  [OK] {asset_name} ({size/1e9:.2f} GB)")
                else:
                    print(f"  [OK] {asset_name} ({size/1e6:.2f} MB)")
            else:
                print(f"  [MISSING] {asset_name}")
        return

    # 上传所有资源
    success_count = 0
    fail_count = 0
    skip_count = 0

    for filepath, asset_name, description in ASSETS:
        if not os.path.exists(filepath):
            print(f"  [跳过] 文件不存在: {filepath}")
            skip_count += 1
            continue

        # 根据扩展名确定 Content-Type
        if asset_name.endswith(".zip"):
            content_type = "application/zip"
        elif asset_name.endswith(".onnx"):
            content_type = "application/octet-stream"
        elif asset_name.endswith(".db"):
            content_type = "application/octet-stream"
        else:
            content_type = "application/octet-stream"

        if upload_asset(upload_url, filepath, asset_name, content_type, args.token):
            success_count += 1
        else:
            fail_count += 1

    print()
    print("=" * 60)
    print(f"上传完成: 成功 {success_count}, 失败 {fail_count}, 跳过 {skip_count}")
    print("=" * 60)

    if fail_count > 0:
        sys.exit(1)


if __name__ == "__main__":
    main()