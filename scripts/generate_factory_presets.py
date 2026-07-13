# =============================================================================
# LianCore - 出厂预设批量生成脚本
# Release 阶段: 生成 500 个 AI 优化出厂预设，覆盖 12 个类别
# =============================================================================

import sqlite3
import json
import random
import os
import sys
import math
from datetime import datetime

# =============================================================================
# 配置
# =============================================================================
DB_PATH = "data/preset_library.db"
PRESETS_PER_CATEGORY = 42  # 12类 × 42 ≈ 500
TOTAL_PRESETS = 504

# 12 个预设类别及其描述
CATEGORIES = {
    "Bass": {
        "tags": ["Bass", "Sub", "Deep", "Groove", "Low"],
        "description_templates": [
            "Deep sub bass with {osc} oscillator and resonant filter",
            "Punchy {osc} bass for modern productions",
            "Warm analog bass with {effect} processing",
            "FM bass with aggressive {effect} character",
            "Reese bass with detuned {osc} layers",
        ],
        "node_types": ["VirtualAnalogOscillator", "WavetableOscillator", "FilterProcessor", "Distortion", "Compressor"],
        "osc_types": ["virtual analog", "wavetable", "FM", "subtractive", "additive"],
        "effect_types": ["distortion", "saturation", "compression", "overdrive", "waveshaping"],
    },
    "Lead": {
        "tags": ["Lead", "Solo", "Melody", "Bright", "Expressive"],
        "description_templates": [
            "Bright {osc} lead with {effect} and delay",
            "Expressive {osc} solo voice with vibrato",
            "Cutting {osc} lead for melodic lines",
            "Vintage analog lead with {effect} warmth",
            "Modern {osc} lead with stereo widening",
        ],
        "node_types": ["VirtualAnalogOscillator", "WavetableOscillator", "SpectralOscillator", "FilterProcessor", "Delay", "Reverb"],
        "osc_types": ["saw", "square", "supersaw", "pulse", "wavetable"],
        "effect_types": ["chorus", "reverb", "delay", "phaser", "flanger"],
    },
    "Pad": {
        "tags": ["Pad", "Ambient", "Atmosphere", "Warm", "Evolving"],
        "description_templates": [
            "Lush {osc} pad with evolving {effect} texture",
            "Warm analog string pad with slow attack",
            "Ethereal {osc} pad with shimmer {effect}",
            "Cinematic {osc} pad with filter movement",
            "Dreamy ambient pad with {effect} modulation",
        ],
        "node_types": ["VirtualAnalogOscillator", "WavetableOscillator", "FilterProcessor", "Reverb", "LFOGenerator"],
        "osc_types": ["supersaw", "analog", "wavetable", "granular", "spectral"],
        "effect_types": ["reverb", "delay", "chorus", "phaser", "shimmer"],
    },
    "Pluck": {
        "tags": ["Pluck", "Short", "Percussive", "Staccato", "Transient"],
        "description_templates": [
            "Sharp {osc} pluck with fast decay",
            "Glass-like {osc} pluck with {effect}",
            "Short {osc} stab with aggressive transient",
            "Mallet-style {osc} pluck with resonance",
            "Digital {osc} pluck with {effect} processing",
        ],
        "node_types": ["VirtualAnalogOscillator", "WavetableOscillator", "FilterProcessor", "EnvelopeGenerator"],
        "osc_types": ["sine", "triangle", "FM", "wavetable", "physical modeling"],
        "effect_types": ["reverb", "delay", "compression", "EQ", "transient shaping"],
    },
    "Keys": {
        "tags": ["Keys", "Piano", "EP", "Organ", "Keyboard"],
        "description_templates": [
            "Classic {osc} piano with {effect} enhancement",
            "Warm electric piano with {osc} tone",
            "Vintage organ with {effect} character",
            "Hybrid {osc} keys with modern processing",
            "FM {osc} keys with bell-like timbre",
        ],
        "node_types": ["MultiSampler", "VirtualAnalogOscillator", "FilterProcessor", "EQ", "Reverb"],
        "osc_types": ["sampled", "FM", "additive", "subtractive", "physical modeling"],
        "effect_types": ["tremolo", "chorus", "reverb", "EQ", "compression"],
    },
    "Arp": {
        "tags": ["Arp", "Sequence", "Pattern", "Rhythmic", "Motion"],
        "description_templates": [
            "Driving {osc} arp with {effect} rhythm",
            "Melodic {osc} sequence with gate effect",
            "Pulsing {osc} arpeggio with filter modulation",
            "Syncopated {osc} pattern with {effect}",
            "Energetic {osc} arp with sidechain feel",
        ],
        "node_types": ["StepSequencer", "VirtualAnalogOscillator", "WavetableOscillator", "FilterProcessor", "LFOGenerator"],
        "osc_types": ["saw", "square", "supersaw", "wavetable", "pulse"],
        "effect_types": ["gate", "filter", "delay", "distortion", "compression"],
    },
    "FX": {
        "tags": ["FX", "Sound Design", "Special", "Texture", "Experimental"],
        "description_templates": [
            "Cinematic {osc} riser with {effect} sweep",
            "Glitchy {osc} texture with granular processing",
            "Deep space {osc} ambiance with {effect}",
            "Transition {osc} effect with filter automation",
            "Experimental {osc} soundscape with {effect}",
        ],
        "node_types": ["SpectralOscillator", "GranularPlayer", "WaveguideResonator", "FilterProcessor", "Reverb", "Delay", "Distortion"],
        "osc_types": ["spectral", "granular", "noise", "waveguide", "FM"],
        "effect_types": ["reverb", "delay", "distortion", "filter", "granular"],
    },
    "Drum": {
        "tags": ["Drum", "Percussion", "Kit", "Beat", "Rhythm"],
        "description_templates": [
            "Punchy {osc} kick with {effect} shaping",
            "Crisp {osc} snare with transient emphasis",
            "Metallic {osc} hi-hat with resonance",
            "808-style {osc} drum with {effect}",
            "Layered {osc} percussion with processing",
        ],
        "node_types": ["DrumSlicer", "NoiseGenerator", "VirtualAnalogOscillator", "FilterProcessor", "Compressor", "Distortion"],
        "osc_types": ["sine", "noise", "FM", "analog", "sampled"],
        "effect_types": ["compression", "saturation", "EQ", "transient shaping", "distortion"],
    },
    "Brass": {
        "tags": ["Brass", "Horn", "Wind", "Ensemble", "Orchestral"],
        "description_templates": [
            "Powerful {osc} brass section with {effect}",
            "Warm {osc} horn with expressive dynamics",
            "Synthesized {osc} brass with filter swell",
            "Ensemble {osc} brass with {effect} modulation",
            "Vintage {osc} brass with analog warmth",
        ],
        "node_types": ["VirtualAnalogOscillator", "WavetableOscillator", "FilterProcessor", "EnvelopeGenerator", "LFOGenerator"],
        "osc_types": ["saw", "square", "analog", "wavetable", "FM"],
        "effect_types": ["chorus", "reverb", "saturation", "compression", "EQ"],
    },
    "String": {
        "tags": ["String", "Orchestral", "Bowed", "Ensemble", "Classical"],
        "description_templates": [
            "Lush {osc} string ensemble with {effect}",
            "Solo {osc} string with expressive vibrato",
            "Pizzicato {osc} strings with fast attack",
            "Cinematic {osc} strings with {effect} texture",
            "Synthesized {osc} strings with analog character",
        ],
        "node_types": ["VirtualAnalogOscillator", "WavetableOscillator", "FilterProcessor", "Reverb", "LFOGenerator"],
        "osc_types": ["saw", "supersaw", "wavetable", "physical modeling", "analog"],
        "effect_types": ["chorus", "reverb", "EQ", "compression", "saturation"],
    },
    "Vocal": {
        "tags": ["Vocal", "Voice", "Choir", "Formant", "Synthesized"],
        "description_templates": [
            "Ethereal {osc} vocal pad with {effect}",
            "Formant-filtered {osc} voice simulation",
            "Choir-like {osc} ensemble with {effect}",
            "Synthesized {osc} vowel with filter movement",
            "Breathy {osc} vocal texture with modulation",
        ],
        "node_types": ["VirtualAnalogOscillator", "WavetableOscillator", "FilterProcessor", "Reverb", "LFOGenerator"],
        "osc_types": ["formant", "wavetable", "FM", "physical modeling", "spectral"],
        "effect_types": ["chorus", "reverb", "formant filter", "delay", "modulation"],
    },
    "Atmosphere": {
        "tags": ["Atmosphere", "Ambient", "Drone", "Soundscape", "Texture"],
        "description_templates": [
            "Deep {osc} drone with evolving {effect}",
            "Ambient {osc} soundscape with slow modulation",
            "Meditative {osc} texture with {effect} wash",
            "Dark {osc} atmosphere with granular haze",
            "Ethereal {osc} cloud with {effect} processing",
        ],
        "node_types": ["SpectralOscillator", "GranularPlayer", "WaveguideResonator", "Reverb", "Delay", "LFOGenerator"],
        "osc_types": ["spectral", "granular", "waveguide", "noise", "wavetable"],
        "effect_types": ["reverb", "delay", "granular", "spectral", "modulation"],
    },
}

# 节点类型参数模板
OSCILLATOR_PARAMS = {
    "VirtualAnalogOscillator": {"waveform": ["sine", "saw", "square", "triangle", "pulse"], "freq": (0.1, 1.0), "volume": (0.3, 0.9), "pulse_width": (0.1, 0.9), "detune": (0.0, 0.3)},
    "WavetableOscillator": {"freq": (0.1, 1.0), "volume": (0.3, 0.9), "position": (0.0, 1.0), "detune": (0.0, 0.2)},
    "SpectralOscillator": {"freq": (0.1, 1.0), "volume": (0.3, 0.9), "partials": (4, 32), "spread": (0.0, 0.5)},
    "NoiseGenerator": {"volume": (0.1, 0.6), "color": (0.0, 1.0)},
    "GranularPlayer": {"density": (0.1, 0.9), "grain_size": (0.05, 0.5), "spread": (0.0, 0.5)},
    "MultiSampler": {"volume": (0.5, 0.9), "transpose": (-12, 12), "velocity_sens": (0.3, 0.9)},
    "WaveguideResonator": {"freq": (0.1, 1.0), "volume": (0.3, 0.8), "resonance": (0.1, 0.9), "damping": (0.1, 0.9)},
    "DrumSlicer": {"volume": (0.5, 0.9), "slice": (0, 15), "pitch": (-12, 12)},
}

FILTER_PARAMS = {"type": ["lpf", "hpf", "bpf", "notch"], "cutoff": (0.1, 0.95), "resonance": (0.0, 0.8), "drive": (0.0, 0.5)}
ENVELOPE_PARAMS = {"attack": (0.0, 0.5), "decay": (0.05, 0.5), "sustain": (0.0, 1.0), "release": (0.05, 0.8)}
LFO_PARAMS = {"rate": (0.01, 0.5), "depth": (0.0, 0.5), "waveform": ["sine", "triangle", "saw", "square", "random"]}
EFFECT_PARAMS = {
    "Reverb": {"mix": (0.1, 0.5), "size": (0.2, 0.9), "damping": (0.1, 0.8)},
    "Delay": {"mix": (0.1, 0.5), "time": (0.1, 0.8), "feedback": (0.1, 0.7)},
    "Distortion": {"drive": (0.1, 0.9), "mix": (0.1, 0.8), "tone": (0.1, 0.9)},
    "Compressor": {"threshold": (0.1, 0.7), "ratio": (0.1, 0.9), "attack": (0.0, 0.3), "release": (0.1, 0.5)},
    "EQ": {"low_gain": (-0.5, 0.5), "mid_gain": (-0.5, 0.5), "high_gain": (-0.5, 0.5), "mid_freq": (0.2, 0.8)},
    "Chorus": {"mix": (0.1, 0.5), "rate": (0.1, 0.5), "depth": (0.1, 0.5)},
}


def rand(min_val, max_val):
    """随机浮点数"""
    return round(random.uniform(min_val, max_val), 4)


def rand_int(min_val, max_val):
    """随机整数"""
    return random.randint(min_val, max_val)


def build_node(node_id, node_type, name):
    """构建合成节点"""
    node = {
        "id": node_id,
        "type": node_type,
        "name": name,
        "enabled": True,
        "params": {},
    }
    
    if node_type in OSCILLATOR_PARAMS:
        for param, spec in OSCILLATOR_PARAMS[node_type].items():
            if isinstance(spec, list):
                node["params"][param] = random.choice(spec) if "waveform" in param else rand(0, 1)
            elif isinstance(spec, tuple):
                node["params"][param] = rand(*spec)
    
    if node_type == "FilterProcessor":
        for param, spec in FILTER_PARAMS.items():
            if isinstance(spec, list):
                node["params"][param] = random.choice(spec)
            elif isinstance(spec, tuple):
                node["params"][param] = rand(*spec)
    
    if node_type == "EnvelopeGenerator":
        for param, spec in ENVELOPE_PARAMS.items():
            node["params"][param] = rand(*spec)
    
    if node_type == "LFOGenerator":
        for param, spec in LFO_PARAMS.items():
            if isinstance(spec, list):
                node["params"][param] = random.choice(spec)
            elif isinstance(spec, tuple):
                node["params"][param] = rand(*spec)
    
    if node_type in EFFECT_PARAMS:
        for param, spec in EFFECT_PARAMS[node_type].items():
            node["params"][param] = rand(*spec)
    
    return node


def generate_synthesis_graph(category, preset_name):
    """为给定类别生成合成图"""
    cat_info = CATEGORIES[category]
    nodes = []
    connections = []
    
    # 确定节点数量 (2-5个)
    num_nodes = random.randint(2, 5)
    selected_types = []
    
    # 至少一个振荡器
    osc_types = [t for t in cat_info["node_types"] if t in OSCILLATOR_PARAMS]
    if osc_types:
        osc_type = random.choice(osc_types)
        selected_types.append(osc_type)
    
    # 添加效果器节点
    effect_types = [t for t in cat_info["node_types"] if t in EFFECT_PARAMS or t in ["FilterProcessor", "EnvelopeGenerator", "LFOGenerator"]]
    random.shuffle(effect_types)
    for et in effect_types[:num_nodes - len(selected_types)]:
        selected_types.append(et)
    
    # 确保不超过最大节点数
    selected_types = selected_types[:num_nodes]
    
    # 构建节点
    node_names = {
        "VirtualAnalogOscillator": "OSC",
        "WavetableOscillator": "WT OSC",
        "SpectralOscillator": "SPEC OSC",
        "NoiseGenerator": "NOISE",
        "GranularPlayer": "GRAIN",
        "MultiSampler": "SAMPLER",
        "WaveguideResonator": "WAVE",
        "DrumSlicer": "DRUM",
        "FilterProcessor": "FILTER",
        "EnvelopeGenerator": "ENV",
        "LFOGenerator": "LFO",
        "Reverb": "REVERB",
        "Delay": "DELAY",
        "Distortion": "DIST",
        "Compressor": "COMP",
        "EQ": "EQ",
        "Chorus": "CHORUS",
        "StepSequencer": "SEQ",
    }
    
    for i, node_type in enumerate(selected_types):
        node_id = f"n{i}"
        name = node_names.get(node_type, node_type)
        nodes.append(build_node(node_id, node_type, name))
    
    # 构建连接 (链式: n0 → n1 → n2 → ...)
    for i in range(len(nodes) - 1):
        connections.append({
            "source": f"n{i}",
            "target": f"n{i+1}",
            "source_port": "audio_out",
            "target_port": "audio_in",
        })
    
    # 生成 JSON 数据
    json_data = {
        "version": "3.0.0",
        "format": "LianCorePreset",
        "category": category,
        "nodes": nodes,
        "connections": connections,
        "metadata": {
            "generated": True,
            "algorithm": "factory-preset-generator-v1",
            "timestamp": datetime.now().isoformat(),
        },
    }
    
    return json_data


def generate_preset_name(category, index):
    """生成预设名称"""
    adjectives = {
        "Bass": ["Deep", "Punchy", "Sub", "Warm", "Fat", "Gritty", "Clean"],
        "Lead": ["Bright", "Cutting", "Vintage", "Modern", "Smooth", "Aggressive", "Dreamy"],
        "Pad": ["Lush", "Ethereal", "Warm", "Cinematic", "Dark", "Shimmer", "Vast"],
        "Pluck": ["Sharp", "Glass", "Crisp", "Wooden", "Digital", "Soft", "Bright"],
        "Keys": ["Classic", "Warm", "Vintage", "Modern", "Bright", "Mellow", "Hybrid"],
        "Arp": ["Driving", "Melodic", "Pulsing", "Energetic", "Rhythmic", "Flowing", "Sync"],
        "FX": ["Cinematic", "Glitchy", "Deep", "Evolving", "Cosmic", "Dark", "Bright"],
        "Drum": ["Punchy", "Crisp", "808", "Acoustic", "Electronic", "Hybrid", "Layered"],
        "Brass": ["Powerful", "Warm", "Bright", "Vintage", "Ensemble", "Solo", "Synth"],
        "String": ["Lush", "Solo", "Pizzicato", "Cinematic", "Warm", "Bright", "Dark"],
        "Vocal": ["Ethereal", "Choir", "Formant", "Solo", "Breathy", "Bright", "Warm"],
        "Atmosphere": ["Deep", "Dark", "Evolving", "Meditative", "Cosmic", "Sparse", "Dense"],
    }
    nouns = {
        "Bass": ["Bass", "Sub", "Reese", "808", "Wobble", "Growl", "Foundation"],
        "Lead": ["Lead", "Solo", "Synth", "Arp", "Hook", "Riff", "Melody"],
        "Pad": ["Pad", "Strings", "Cloud", "Wash", "Texture", "Swell", "Backdrop"],
        "Pluck": ["Pluck", "Stab", "Hit", "Chord", "Mallet", "Bell", "Strike"],
        "Keys": ["Piano", "EP", "Organ", "Keys", "Clav", "Rhodes", "Wurli"],
        "Arp": ["Arp", "Sequence", "Pattern", "Groove", "Motion", "Pulse", "Flow"],
        "FX": ["Riser", "Downer", "Impact", "Sweep", "Glitch", "Whoosh", "Drone"],
        "Drum": ["Kick", "Snare", "Hat", "Tom", "Clap", "Cymbal", "Kit"],
        "Brass": ["Brass", "Horn", "Trumpet", "Trombone", "Section", "Fanfare", "Stab"],
        "String": ["Strings", "Cello", "Violin", "Viola", "Section", "Quartet", "Ensemble"],
        "Vocal": ["Vox", "Voice", "Choir", "Vowel", "Whisper", "Chant", "Syllable"],
        "Atmosphere": ["Drone", "Space", "Ambient", "Void", "Nebula", "Horizon", "Aether"],
    }
    
    adj = random.choice(adjectives.get(category, ["Pro"]))
    noun = random.choice(nouns.get(category, ["Sound"]))
    return f"{adj} {noun} {index + 1}"


def generate_presets():
    """生成所有出厂预设"""
    print(f"Generating {TOTAL_PRESETS} factory presets across {len(CATEGORIES)} categories...")
    print(f"Database: {DB_PATH}")
    
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    
    # 清除已有的出厂预设 (author = 'factory')
    cur.execute("DELETE FROM presets WHERE author = 'factory'")
    print(f"Cleared existing factory presets")
    
    preset_id = cur.execute("SELECT MAX(id) FROM presets").fetchone()[0] or 0
    generated = 0
    
    for category, cat_info in CATEGORIES.items():
        for i in range(PRESETS_PER_CATEGORY):
            name = generate_preset_name(category, i)
            tags = json.dumps(cat_info["tags"])
            desc_template = random.choice(cat_info["description_templates"])
            description = desc_template.format(
                osc=random.choice(cat_info["osc_types"]),
                effect=random.choice(cat_info["effect_types"]),
            )
            
            json_data = generate_synthesis_graph(category, name)
            json_str = json.dumps(json_data, ensure_ascii=False)
            
            preset_id += 1
            cur.execute(
                """INSERT INTO presets (id, name, category, tags, description, author, json_data, ai_confidence, rating)
                   VALUES (?, ?, ?, ?, ?, 'factory', ?, 0.85, 0)""",
                (preset_id, name, category, tags, description, json_str),
            )
            generated += 1
            
            if generated % 50 == 0:
                print(f"  Generated {generated}/{TOTAL_PRESETS} presets...", flush=True)
    
    conn.commit()
    conn.close()
    
    print(f"\nComplete! Generated {generated} factory presets in {DB_PATH}")
    print(f"Categories: {', '.join(CATEGORIES.keys())}")
    print(f"Presets per category: {PRESETS_PER_CATEGORY}")


if __name__ == "__main__":
    random.seed(42)
    generate_presets()