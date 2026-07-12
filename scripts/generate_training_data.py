# =============================================================================
# LianCore - 自监督训练数据生成器
# Gamma Week 3-4: 音频参考音色复刻
# 从预设库读取参数 → 合成音频 → 存储为训练数据
# =============================================================================

import sqlite3
import json
import numpy as np
import os
import sys
import random
from typing import Dict, List, Optional, Tuple

# 配置
TARGET_SR = 44100
TARGET_SAMPLES = 16384
DB_PATH = "data/preset_library.db"
OUTPUT_DIR = "data/training/audio_timbre"
NUM_AUGMENTATIONS = 2  # 每条预设的增强变体数
VALIDATION_SPLIT = 0.1
MAX_PRESETS = 1000  # 限制使用的预设数量 (快速生成)

# 振荡器类型映射
OSC_TYPE_MAP = {
    "WavetableOscillator": 0.0,
    "VirtualAnalogOscillator": 0.33,
    "SpectralOscillator": 0.66,
    "MultiSampler": 1.0,
    "NoiseGenerator": 0.5,
    "GranularPlayer": 0.75,
}


def load_presets(db_path: str, max_presets: int = MAX_PRESETS) -> List[Dict]:
    """从预设库加载参数和描述"""
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    cursor.execute(
        "SELECT name, json_data FROM presets ORDER BY RANDOM() LIMIT ?",
        (max_presets,),
    )
    presets = []
    for row in cursor.fetchall():
        name = row[0] or "unknown"
        json_data = row[1] or "{}"
        try:
            params = json.loads(json_data)
            presets.append({"name": name, "params": params})
        except json.JSONDecodeError:
            continue
    conn.close()
    return presets


def extract_param_vector(graph: Dict) -> np.ndarray:
    """
    从合成图提取 11 维参数向量

    0: oscillator_type       [0=波表, 0.33=虚拟模拟, 0.66=频谱, 1=采样]
    1: filter_cutoff         [0.0-1.0]
    2: filter_resonance      [0.0-1.0]
    3: filter_type           [0=LPF, 0.33=HPF, 0.66=BPF, 1=Notch]
    4: amp_attack            [0.0-1.0]
    5: amp_decay             [0.0-1.0]
    6: amp_sustain           [0.0-1.0]
    7: amp_release           [0.0-1.0]
    8: unison_voices         [0.0-1.0] → [1, 16]
    9: unison_detune         [0.0-1.0] → [0, 0.5]
    10: fx_mix               [0.0-1.0]
    """
    vec = np.zeros(11, dtype=np.float32)

    # 默认值
    vec[0] = 0.0  # 默认波表
    vec[1] = 0.5  # 默认 cutoff 中间
    vec[2] = 0.0  # 默认无共振
    vec[3] = 0.0  # 默认 LPF
    vec[4] = 0.01  # 快速起音
    vec[5] = 0.3  # 中等衰减
    vec[6] = 0.7  # 高保持
    vec[7] = 0.3  # 中等释放
    vec[8] = 0.0  # 单音
    vec[9] = 0.0  # 无失谐
    vec[10] = 0.0  # 无效果

    nodes = graph.get("nodes", [])

    for ni in nodes:
        ntype = ni.get("type", "")
        params = ni.get("params", {})

        # 振荡器类型
        if ntype in OSC_TYPE_MAP:
            vec[0] = OSC_TYPE_MAP[ntype]
            # 同音/失谐
            if "unison" in params:
                vec[8] = params["unison"]
            if "detune" in params:
                vec[9] = params["detune"]

        # 滤波器
        if ntype == "FilterProcessor":
            if "cutoff" in params:
                vec[1] = params["cutoff"]
            if "resonance" in params:
                vec[2] = params["resonance"]
            if "filterMode" in params:
                vec[3] = params["filterMode"]

        # 包络
        if ntype == "EnvelopeGenerator":
            if "attack" in params:
                vec[4] = params["attack"]
            if "decay" in params:
                vec[5] = params["decay"]
            if "sustain" in params:
                vec[6] = params["sustain"]
            if "release" in params:
                vec[7] = params["release"]

        # 效果器混合量
        if ntype in ("Reverb", "Delay", "Distortion", "Compressor"):
            if "mix" in params:
                vec[10] = max(vec[10], params["mix"])
            elif "amount" in params:
                vec[10] = max(vec[10], params["amount"])

    return np.clip(vec, 0.0, 1.0)


def augment_params(params: np.ndarray, noise_scale: float = 0.05) -> np.ndarray:
    """参数增强：添加均匀噪声，模拟不同变体"""
    noise = np.random.uniform(-noise_scale, noise_scale, size=params.shape)
    return np.clip(params + noise, 0.0, 1.0)


def render_audio(params: np.ndarray, duration: float = 0.37, sr: int = TARGET_SR) -> np.ndarray:
    """
    基于参数向量合成音频
    使用多个振荡器混合 + 包络 + 滤波器模拟
    """
    t = np.linspace(0, duration, int(sr * duration), endpoint=False)
    n_samples = len(t)

    # === 振荡器 ===
    osc_type = params[0]
    osc_type_idx = int(osc_type * 3.999)  # 0=sine/like, 1=VA, 2=spectral, 3=sample

    base_freq = 220.0  # A3
    audio = np.zeros(n_samples, dtype=np.float64)

    if osc_type_idx == 0:  # 波表风格 (丰富谐波)
        for h in range(1, 12):
            amp = 1.0 / h
            audio += amp * np.sin(2 * np.pi * base_freq * h * t)
    elif osc_type_idx == 1:  # 虚拟模拟 (锯齿)
        for h in range(1, 16):
            amp = 1.0 / h
            audio += amp * np.sin(2 * np.pi * base_freq * h * t)
    elif osc_type_idx == 2:  # 频谱 (奇数谐波)
        for h in range(1, 16, 2):
            amp = 1.0 / (h * h)
            audio += amp * np.sin(2 * np.pi * base_freq * h * t)
    else:  # 采样/噪声混合
        audio = np.sin(2 * np.pi * base_freq * t)
        audio += 0.3 * np.sin(2 * np.pi * base_freq * 2 * t)
        audio += 0.1 * np.random.randn(n_samples)

    audio = audio / (np.max(np.abs(audio)) + 1e-8)

    # === 同音失谐 ===
    unison = params[8]
    detune = params[9]
    if unison > 0.01:
        num_voices = max(1, int(unison * 15.999) + 1)
        for v in range(1, num_voices):
            detune_factor = 1.0 + (detune * 0.5 * (v - num_voices / 2) / num_voices)
            audio += 0.3 * np.sin(2 * np.pi * base_freq * detune_factor * t)
    audio = audio / (np.max(np.abs(audio)) + 1e-8)

    # === 振幅包络 ===
    attack_s = int(params[4] * sr * 1.0)
    decay_s = int(params[5] * sr * 0.5)
    sustain = params[6]
    release_s = int(params[7] * sr * 1.0)

    total = attack_s + decay_s + release_s
    if total > n_samples:
        ratio = n_samples / total
        attack_s = int(attack_s * ratio)
        decay_s = int(decay_s * ratio)
        release_s = n_samples - attack_s - decay_s

    env = np.ones(n_samples, dtype=np.float64) * sustain
    if attack_s > 0:
        env[:attack_s] = np.linspace(0, 1, attack_s)
    if decay_s > 0:
        env[attack_s : attack_s + decay_s] = np.linspace(1, sustain, decay_s)
    if release_s > 0:
        start_release = attack_s + decay_s
        env[start_release : start_release + release_s] = np.linspace(sustain, 0, release_s)

    audio = audio * env

    # === 滤波器 ===
    cutoff = params[1] * 0.45 + 0.01
    resonance = params[2]
    filter_mode = int(params[3] * 3.999)

    # 应用滤波器 (FFT 频域滤波，高效)
    if cutoff < 0.49:
        # 频域滤波
        fft_audio = np.fft.rfft(audio)
        freqs = np.fft.rfftfreq(n_samples, 1.0 / sr)
        cutoff_hz = cutoff * sr / 2.0
        
        if filter_mode == 0:  # LPF
            mask = np.ones_like(fft_audio)
            mask[freqs > cutoff_hz] = np.exp(-(freqs[freqs > cutoff_hz] - cutoff_hz) / (cutoff_hz * 0.2 + 100))
        elif filter_mode == 1:  # HPF
            mask = np.ones_like(fft_audio)
            mask[freqs < cutoff_hz] = np.exp(-(cutoff_hz - freqs[freqs < cutoff_hz]) / (cutoff_hz * 0.2 + 100))
        elif filter_mode == 2:  # BPF
            bw = cutoff_hz * 0.3
            mask = np.exp(-((freqs - cutoff_hz) ** 2) / (2 * bw ** 2))
        else:  # Notch
            bw = cutoff_hz * 0.1
            mask = 1.0 - np.exp(-((freqs - cutoff_hz) ** 2) / (2 * bw ** 2))
        
        # 共振增强
        if resonance > 0.01:
            boost = 1.0 + resonance * 3.0 * np.exp(-((freqs - cutoff_hz) ** 2) / (2 * (cutoff_hz * 0.05 + 50) ** 2))
            mask = mask * boost
            mask = mask / np.max(mask)
        
        audio = np.fft.irfft(fft_audio * mask, n=n_samples)

    audio = audio / (np.max(np.abs(audio)) + 1e-8)

    # === 效果器混合 ===
    fx_mix = params[10]
    if fx_mix > 0.01:
        # 简单混响模拟 (梳状滤波)
        delay_len = int(sr * 0.05)
        dry = audio * (1.0 - fx_mix)
        wet = np.zeros(n_samples, dtype=np.float64)
        for i in range(delay_len, n_samples):
            wet[i] = audio[i] + 0.6 * wet[i - delay_len]
        wet = wet / (np.max(np.abs(wet)) + 1e-8)
        audio = dry + fx_mix * wet

    audio = audio / (np.max(np.abs(audio)) + 1e-8)

    # 截取/填充至目标长度
    if len(audio) > TARGET_SAMPLES:
        audio = audio[:TARGET_SAMPLES]
    else:
        audio = np.pad(audio, (0, TARGET_SAMPLES - len(audio)))

    return audio.astype(np.float32)


def preprocess_audio(audio: np.ndarray) -> np.ndarray:
    """预处理：预加重 + 归一化"""
    # 预加重
    audio = np.append(audio[0], audio[1:] - 0.97 * audio[:-1])
    # 归一化
    audio = audio / (np.max(np.abs(audio)) + 1e-8)
    return audio.astype(np.float32)


def compute_mel_spec(
    audio: np.ndarray,
    sr: int = TARGET_SR,
    n_mels: int = 64,
    n_fft: int = 1024,
    hop_length: int = 256,
) -> np.ndarray:
    """计算 Mel 频谱图 [n_mels, n_frames] - 向量化版本"""
    n_frames = 1 + (len(audio) - n_fft) // hop_length
    
    # 构建帧矩阵 (向量化)
    window = np.hanning(n_fft).astype(np.float32)
    frames = np.zeros((n_frames, n_fft), dtype=np.float32)
    for i in range(n_frames):
        start = i * hop_length
        frames[i] = audio[start:start + n_fft] * window
    
    # FFT (向量化)
    spec = np.abs(np.fft.rfft(frames, n=n_fft))[:, :n_fft // 2 + 1]
    
    # Mel 滤波器组 (向量化)
    mel_freqs = 20.0 * np.power(1000.0, np.arange(n_mels + 2) / n_mels)
    mel_bins = np.floor((n_fft + 1) * mel_freqs / sr).astype(np.int32)
    
    mel_weights = np.zeros((n_mels, n_fft // 2 + 1), dtype=np.float32)
    for m in range(n_mels):
        low, center, high = mel_bins[m], mel_bins[m + 1], mel_bins[m + 2]
        for k in range(low, min(center, n_fft // 2 + 1)):
            mel_weights[m, k] = (k - low) / max(center - low, 1)
        for k in range(center, min(high, n_fft // 2 + 1)):
            mel_weights[m, k] = (high - k) / max(high - center, 1)
    
    # Mel 滤波 (矩阵乘法)
    mel_spec = np.log(np.dot(spec, mel_weights.T) + 1e-8).astype(np.float32)
    
    # 标准化
    mean = mel_spec.mean()
    std = mel_spec.std() + 1e-8
    mel_spec = (mel_spec - mean) / std
    
    # 确保 64 帧 (填充或截取)
    if mel_spec.shape[0] < 64:
        pad = np.zeros((64 - mel_spec.shape[0], n_mels), dtype=np.float32)
        mel_spec = np.vstack([mel_spec, pad])
    elif mel_spec.shape[0] > 64:
        mel_spec = mel_spec[:64]
    
    return mel_spec  # [64, n_mels]


def generate_dataset():
    """主函数：生成训练数据集"""
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    print(f"Loading presets from {DB_PATH}...", flush=True)
    presets = load_presets(DB_PATH)
    print(f"Loaded {len(presets)} presets")

    all_params = []
    all_audio = []
    all_mel = []

    for i, preset in enumerate(presets):
        if i % 500 == 0:
            print(f"  Processing {i}/{len(presets)}...", flush=True)

        base_params = extract_param_vector(preset["params"])

        # 原始 + 增强变体
        for aug_idx in range(1 + NUM_AUGMENTATIONS):
            if aug_idx == 0:
                params = base_params
            else:
                params = augment_params(base_params)

            audio = render_audio(params)
            audio = preprocess_audio(audio)
            mel = compute_mel_spec(audio)

            all_params.append(params)
            all_audio.append(audio)
            all_mel.append(mel)

    # 转换为 NumPy 数组
    params_array = np.array(all_params, dtype=np.float32)
    audio_array = np.array(all_audio, dtype=np.float32)
    mel_array = np.array(all_mel, dtype=np.float32)

    # 随机打乱
    indices = np.random.permutation(len(params_array))
    params_array = params_array[indices]
    audio_array = audio_array[indices]
    mel_array = mel_array[indices]

    # 训练/验证分割
    split = int(len(params_array) * (1 - VALIDATION_SPLIT))

    train_data = {
        "params": params_array[:split],
        "audio": audio_array[:split],
        "mel": mel_array[:split],
    }
    val_data = {
        "params": params_array[split:],
        "audio": audio_array[split:],
        "mel": mel_array[split:],
    }

    train_path = os.path.join(OUTPUT_DIR, "train.npz")
    val_path = os.path.join(OUTPUT_DIR, "val.npz")
    np.savez_compressed(train_path, **train_data)
    np.savez_compressed(val_path, **val_data)

    print(f"\nTraining data: {len(train_data['params'])} samples → {train_path}", flush=True)
    print(f"Validation data: {len(val_data['params'])} samples → {val_path}", flush=True)
    print(f"Audio shape: {audio_array.shape}, Mel shape: {mel_array.shape}", flush=True)
    print(f"Params range: [{params_array.min():.3f}, {params_array.max():.3f}]", flush=True)


if __name__ == "__main__":
    generate_dataset()