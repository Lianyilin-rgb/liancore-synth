# =============================================================================
# LianCore - 出厂波表批量生成脚本
# Release 阶段: 使用 VAE 生成 100 个高质量出厂波表
# =============================================================================

import numpy as np
import os
import sys
import json
from time import time

# 配置
MODEL_PATH = "models/wavetable_vae_decoder.onnx"
OUTPUT_DIR = "data/factory_wavetables"
NUM_WAVETABLES = 100
LATENT_DIM = 64
TEXT_DIM = 128
WT_FRAMES = 256
WT_SIZE = 2048

# 波表类型描述 (用于文本嵌入)
WAVETABLE_TYPES = {
    "Analog_Saw": {"type_idx": 0, "desc": "Classic analog sawtooth wave"},
    "Analog_Square": {"type_idx": 1, "desc": "Vintage square wave with PWM"},
    "Analog_Triangle": {"type_idx": 2, "desc": "Warm triangle wave"},
    "FM_Bell": {"type_idx": 3, "desc": "FM synthesis bell-like timbre"},
    "FM_Brass": {"type_idx": 4, "desc": "FM brass with bright harmonics"},
    "FM_EPiano": {"type_idx": 5, "desc": "FM electric piano"},
    "Wavetable_Modern": {"type_idx": 6, "desc": "Modern wavetable with formants"},
    "Wavetable_Vintage": {"type_idx": 7, "desc": "Vintage PPG-style wavetable"},
    "Wavetable_Complex": {"type_idx": 8, "desc": "Complex evolving wavetable"},
    "Spectral_Pad": {"type_idx": 9, "desc": "Spectral pad texture"},
    "Spectral_Glass": {"type_idx": 10, "desc": "Glass-like spectral harmonic"},
    "Spectral_Organ": {"type_idx": 11, "desc": "Organ-like spectral profile"},
    "Hybrid_SawPad": {"type_idx": 12, "desc": "Hybrid saw-pad morphing"},
    "Hybrid_BassGrowl": {"type_idx": 13, "desc": "Aggressive bass growl"},
    "Hybrid_PluckBell": {"type_idx": 14, "desc": "Plucked bell hybrid"},
    "Formant_Vowel_A": {"type_idx": 15, "desc": "Formant vowel 'A' shape"},
    "Formant_Vowel_E": {"type_idx": 16, "desc": "Formant vowel 'E' shape"},
    "Formant_Vowel_I": {"type_idx": 17, "desc": "Formant vowel 'I' shape"},
    "Formant_Vowel_O": {"type_idx": 18, "desc": "Formant vowel 'O' shape"},
    "Formant_Vowel_U": {"type_idx": 19, "desc": "Formant vowel 'U' shape"},
    "Additive_Bright": {"type_idx": 20, "desc": "Bright additive synthesis"},
    "Additive_Dark": {"type_idx": 21, "desc": "Dark additive with few harmonics"},
    "Additive_Chiff": {"type_idx": 22, "desc": "Chiff/breath noise additive"},
    "Noise_Filtered": {"type_idx": 23, "desc": "Filtered noise for percussion"},
    "Noise_Metallic": {"type_idx": 24, "desc": "Metallic noise resonance"},
    "PWM_Slow": {"type_idx": 25, "desc": "Slow PWM sweep"},
    "PWM_Fast": {"type_idx": 26, "desc": "Fast PWM modulation"},
    "Sync_Hard": {"type_idx": 27, "desc": "Hard sync oscillator sweep"},
    "Sync_Soft": {"type_idx": 28, "desc": "Soft sync with harmonics"},
    "RingMod_Bell": {"type_idx": 29, "desc": "Ring modulation bell"},
    "RingMod_Alien": {"type_idx": 30, "desc": "Ring modulation alien texture"},
    "Filter_Sweep_LP": {"type_idx": 31, "desc": "Low-pass filter sweep"},
    "Filter_Sweep_BP": {"type_idx": 32, "desc": "Band-pass filter sweep"},
    "Filter_Sweep_HP": {"type_idx": 33, "desc": "High-pass filter sweep"},
    "Comb_Resonance": {"type_idx": 34, "desc": "Comb filter resonance"},
    "Comb_Pluck": {"type_idx": 35, "desc": "Comb filter pluck"},
    "Distortion_Soft": {"type_idx": 36, "desc": "Soft saturation distortion"},
    "Distortion_Hard": {"type_idx": 37, "desc": "Hard clipping distortion"},
    "Distortion_Fold": {"type_idx": 38, "desc": "Wave folding distortion"},
    "Phase_Distortion": {"type_idx": 39, "desc": "Phase distortion sweep"},
    "Supersaw_3x": {"type_idx": 40, "desc": "3-voice supersaw detune"},
    "Supersaw_7x": {"type_idx": 41, "desc": "7-voice supersaw detune"},
    "Unison_Lead": {"type_idx": 42, "desc": "Unison lead with detune"},
    "Unison_Choir": {"type_idx": 43, "desc": "Unison choir effect"},
    "Chorus_Ensemble": {"type_idx": 44, "desc": "Chorus ensemble texture"},
    "Flanger_Jet": {"type_idx": 45, "desc": "Jet flanger sweep"},
    "Phaser_Slow": {"type_idx": 46, "desc": "Slow phaser modulation"},
    "Phaser_Fast": {"type_idx": 47, "desc": "Fast phaser sweep"},
    "Vibrato_Sine": {"type_idx": 48, "desc": "Sine vibrato modulation"},
    "Vibrato_Random": {"type_idx": 49, "desc": "Random vibrato texture"},
    "Tremolo_Soft": {"type_idx": 50, "desc": "Soft tremolo amplitude"},
    "Tremolo_Hard": {"type_idx": 51, "desc": "Hard tremolo gate"},
    "Reverb_Ambient": {"type_idx": 52, "desc": "Ambient reverb-like texture"},
    "Delay_Echo": {"type_idx": 53, "desc": "Echo delay texture"},
    "Glitch_Stutter": {"type_idx": 54, "desc": "Glitch stutter effect"},
    "Glitch_Buffer": {"type_idx": 55, "desc": "Buffer glitch texture"},
    "Granular_Cloud": {"type_idx": 56, "desc": "Granular cloud texture"},
    "Granular_Stretch": {"type_idx": 57, "desc": "Time-stretched granular"},
    "Waveguide_String": {"type_idx": 58, "desc": "Waveguide string model"},
    "Waveguide_Wind": {"type_idx": 59, "desc": "Waveguide wind model"},
    "Physical_Resonance": {"type_idx": 60, "desc": "Physical resonance model"},
    "Physical_Impact": {"type_idx": 61, "desc": "Physical impact model"},
    "Vocal_Whisper": {"type_idx": 62, "desc": "Vocal whisper texture"},
    "Vocal_Chant": {"type_idx": 63, "desc": "Vocal chant overtone"},
    "Nature_Wind": {"type_idx": 64, "desc": "Wind sound texture"},
    "Nature_Water": {"type_idx": 65, "desc": "Water flow texture"},
    "Nature_Fire": {"type_idx": 66, "desc": "Fire crackle texture"},
    "Machine_Engine": {"type_idx": 67, "desc": "Engine drone texture"},
    "Machine_Motor": {"type_idx": 68, "desc": "Motor whine texture"},
    "Machine_Data": {"type_idx": 69, "desc": "Data stream texture"},
    "SciFi_Laser": {"type_idx": 70, "desc": "Sci-fi laser sweep"},
    "SciFi_Alien": {"type_idx": 71, "desc": "Alien communication texture"},
    "SciFi_Spaceship": {"type_idx": 72, "desc": "Spaceship drone"},
    "SciFi_Teleport": {"type_idx": 73, "desc": "Teleportation effect"},
    "SciFi_Shield": {"type_idx": 74, "desc": "Energy shield hum"},
    "Orchestral_Brass": {"type_idx": 75, "desc": "Orchestral brass section"},
    "Orchestral_String": {"type_idx": 76, "desc": "Orchestral string ensemble"},
    "Orchestral_Woodwind": {"type_idx": 77, "desc": "Woodwind ensemble"},
    "Orchestral_Choir": {"type_idx": 78, "desc": "Choir ensemble texture"},
    "Ethnic_Sitar": {"type_idx": 79, "desc": "Sitar-like resonance"},
    "Ethnic_Didgeridoo": {"type_idx": 80, "desc": "Didgeridoo drone"},
    "Ethnic_Kalimba": {"type_idx": 81, "desc": "Kalimba-like pluck"},
    "Ethnic_Shakuhachi": {"type_idx": 82, "desc": "Shakuhachi breath texture"},
    "Retro_8bit": {"type_idx": 83, "desc": "8-bit chip sound"},
    "Retro_16bit": {"type_idx": 84, "desc": "16-bit arcade sound"},
    "Retro_C64": {"type_idx": 85, "desc": "Commodore 64 SID chip"},
    "Retro_Gameboy": {"type_idx": 86, "desc": "Gameboy pulse wave"},
    "Retro_NES": {"type_idx": 87, "desc": "NES triangle wave"},
    "EDM_Pluck": {"type_idx": 88, "desc": "EDM pluck with reverb"},
    "EDM_Lead": {"type_idx": 89, "desc": "EDM festival lead"},
    "EDM_Bass": {"type_idx": 90, "desc": "EDM bass growl"},
    "EDM_Chord": {"type_idx": 91, "desc": "EDM chord stack"},
    "EDM_Riser": {"type_idx": 92, "desc": "EDM riser sweep"},
    "LoFi_Piano": {"type_idx": 93, "desc": "Lo-fi degraded piano"},
    "LoFi_Vinyl": {"type_idx": 94, "desc": "Vinyl crackle texture"},
    "LoFi_Tape": {"type_idx": 95, "desc": "Tape warble texture"},
    "LoFi_Radio": {"type_idx": 96, "desc": "Radio transmission texture"},
    "Ambient_Drone": {"type_idx": 97, "desc": "Deep ambient drone"},
    "Ambient_Shimmer": {"type_idx": 98, "desc": "Shimmer reverb texture"},
    "Ambient_SubBass": {"type_idx": 99, "desc": "Sub-bass ambient rumble"},
}


def generate_text_embedding(type_info, seed=42):
    """为波表类型生成文本嵌入 (128维)"""
    rng = np.random.RandomState(seed + type_info["type_idx"] * 100)
    emb = rng.randn(TEXT_DIM).astype(np.float32)
    
    # 强化类型特征
    type_idx = type_info["type_idx"] % TEXT_DIM
    emb[type_idx] += 2.0
    emb[(type_idx + 1) % TEXT_DIM] += 1.0
    
    # 归一化
    emb = emb / (np.linalg.norm(emb) + 1e-8)
    return emb


def generate_wavetables():
    """生成所有出厂波表"""
    print(f"Generating {NUM_WAVETABLES} factory wavetables using VAE decoder...")
    print(f"Model: {MODEL_PATH}")
    print(f"Output: {OUTPUT_DIR}/")
    
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    
    # 加载 ONNX 模型
    import onnxruntime as ort
    session = ort.InferenceSession(MODEL_PATH)
    print("Model loaded successfully")
    
    # 生成波表
    type_names = list(WAVETABLE_TYPES.keys())
    generated = 0
    
    manifest = []
    
    for i, type_name in enumerate(type_names):
        type_info = WAVETABLE_TYPES[type_name]
        
        # 生成文本嵌入
        text_emb = generate_text_embedding(type_info, seed=42)
        text_emb = text_emb.reshape(1, TEXT_DIM)
        
        # 生成潜在向量
        rng = np.random.RandomState(42 + i)
        latent_z = rng.randn(1, LATENT_DIM).astype(np.float32)
        
        # 运行推理
        inputs = {
            "latent_z": latent_z,
            "text_embedding": text_emb,
        }
        outputs = session.run(None, inputs)
        wavetable = outputs[0].astype(np.float32)  # (1, 256, 2048)
        
        # 验证
        assert wavetable.shape == (1, WT_FRAMES, WT_SIZE), \
            f"Unexpected shape: {wavetable.shape}"
        assert wavetable.min() >= -1.01 and wavetable.max() <= 1.01, \
            f"Value range: [{wavetable.min()}, {wavetable.max()}]"
        
        # 保存为 .npy 文件
        safe_name = type_name.replace(" ", "_").replace("/", "_")
        npy_path = os.path.join(OUTPUT_DIR, f"{safe_name}.npy")
        np.save(npy_path, wavetable[0])  # 只保存 (256, 2048) 部分
        
        # 保存为 .wav 格式 (可选，便于试听)
        wav_path = os.path.join(OUTPUT_DIR, f"{safe_name}.wav")
        # 将波表展开为音频：每帧重复并交叉淡入淡出
        audio = wavetable[0].flatten()  # 256*2048 = 524288 samples
        # 归一化
        audio = audio / (np.max(np.abs(audio)) + 1e-8)
        # 保存为 16-bit WAV
        import wave
        audio_16 = (audio * 32767).astype(np.int16)
        with wave.open(wav_path, 'w') as wf:
            wf.setnchannels(1)
            wf.setsampwidth(2)
            wf.setframerate(44100)
            wf.writeframes(audio_16.tobytes())
        
        # 记录元数据
        manifest.append({
            "name": type_name,
            "file": f"{safe_name}.npy",
            "wav": f"{safe_name}.wav",
            "description": type_info["desc"],
            "type_idx": type_info["type_idx"],
            "frames": WT_FRAMES,
            "table_size": WT_SIZE,
            "value_range": [float(wavetable.min()), float(wavetable.max())],
        })
        
        generated += 1
        if generated % 10 == 0:
            print(f"  Generated {generated}/{NUM_WAVETABLES} wavetables...", flush=True)
    
    # 保存 manifest
    manifest_path = os.path.join(OUTPUT_DIR, "manifest.json")
    with open(manifest_path, 'w') as f:
        json.dump(manifest, f, indent=2, ensure_ascii=False)
    
    print(f"\nComplete! Generated {generated} factory wavetables")
    print(f"Output directory: {OUTPUT_DIR}/")
    print(f"Manifest: {manifest_path}")
    
    # 统计
    total_npy_size = sum(
        os.path.getsize(os.path.join(OUTPUT_DIR, m["file"]))
        for m in manifest if os.path.exists(os.path.join(OUTPUT_DIR, m["file"]))
    )
    total_wav_size = sum(
        os.path.getsize(os.path.join(OUTPUT_DIR, m["wav"]))
        for m in manifest if os.path.exists(os.path.join(OUTPUT_DIR, m["wav"]))
    )
    print(f"Total .npy size: {total_npy_size / 1024 / 1024:.1f} MB")
    print(f"Total .wav size: {total_wav_size / 1024 / 1024:.1f} MB")


if __name__ == "__main__":
    generate_wavetables()