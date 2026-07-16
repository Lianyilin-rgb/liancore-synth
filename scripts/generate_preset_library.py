#!/usr/bin/env python3
# =============================================================================
# LianCore - 合成数据驱动预制库填充脚本 (P7-1: 100万+扩展)
# 生成 1,000,000+ 条合成器音色预设数据，写入 SQLite
# =============================================================================
"""
用法:
    python generate_preset_library.py [--count 1000000] [--db preset_library_1M.db]
                                      [--batch-size 10000] [--seed 42]

输出:
    - SQLite 数据库文件 (预设表 + 版本历史表)
    - 统计报告 JSON (类别分布、参数分布、质量指标)
"""

import sqlite3
import json
import random
import time
import os
import sys
import argparse
from datetime import datetime, timedelta
from collections import Counter

# =============================================================================
# 配置常量
# =============================================================================

# 节点类型及其参数定义
NODE_PARAM_DEFS = {
    "WavetableOscillator": {
        "params": ["freq", "volume", "unison", "detune", "framePosition", "fmDepth", "phaseOffset", "warpAmount"],
        "ranges": {
            "freq": (0.0, 1.0), "volume": (0.0, 1.0), "unison": (0.0, 1.0),
            "detune": (0.0, 1.0), "framePosition": (0.0, 1.0), "fmDepth": (0.0, 1.0),
            "phaseOffset": (0.0, 1.0), "warpAmount": (0.0, 1.0),
        },
        "display_name": "波表振荡器",
    },
    "VirtualAnalogOscillator": {
        "params": ["freq", "volume", "waveform", "pulseWidth", "unison", "detune", "hardSync"],
        "ranges": {
            "freq": (0.0, 1.0), "volume": (0.0, 1.0), "waveform": (0.0, 1.0),
            "pulseWidth": (0.1, 0.9), "unison": (0.0, 1.0), "detune": (0.0, 1.0),
            "hardSync": (0.0, 1.0),
        },
        "display_name": "虚拟模拟振荡器",
    },
    "NoiseGenerator": {
        "params": ["volume", "noiseType", "color", "stereoWidth"],
        "ranges": {
            "volume": (0.0, 1.0), "noiseType": (0.0, 1.0),
            "color": (0.0, 1.0), "stereoWidth": (0.0, 1.0),
        },
        "display_name": "噪声发生器",
    },
    "SpectralOscillator": {
        "params": ["freq", "volume", "partials", "spectralSpread", "formantShift", "inharmonicity"],
        "ranges": {
            "freq": (0.0, 1.0), "volume": (0.0, 1.0), "partials": (0.0, 1.0),
            "spectralSpread": (0.0, 1.0), "formantShift": (0.0, 1.0), "inharmonicity": (0.0, 1.0),
        },
        "display_name": "频谱振荡器",
    },
    "FilterProcessor": {
        "params": ["filterMode", "cutoff", "resonance", "drive", "mix"],
        "ranges": {
            "filterMode": (0.0, 1.0), "cutoff": (0.0, 1.0), "resonance": (0.0, 1.0),
            "drive": (0.0, 1.0), "mix": (0.0, 1.0),
        },
        "display_name": "滤波器",
    },
    "Distortion": {
        "params": ["drive", "type", "output", "mix", "bias", "tone"],
        "ranges": {
            "drive": (0.0, 1.0), "type": (0.0, 1.0), "output": (0.0, 1.0),
            "mix": (0.0, 1.0), "bias": (0.0, 1.0), "tone": (0.0, 1.0),
        },
        "display_name": "失真",
    },
    "Delay": {
        "params": ["time", "feedback", "mix", "pingPong", "lowPassCutoff", "highPassCutoff"],
        "ranges": {
            "time": (0.0, 1.0), "feedback": (0.0, 0.95), "mix": (0.0, 1.0),
            "pingPong": (0.0, 1.0), "lowPassCutoff": (0.0, 1.0), "highPassCutoff": (0.0, 1.0),
        },
        "display_name": "延迟",
    },
    "Reverb": {
        "params": ["roomSize", "decay", "damping", "mix", "predelay", "width", "earlyReflections"],
        "ranges": {
            "roomSize": (0.0, 1.0), "decay": (0.0, 1.0), "damping": (0.0, 1.0),
            "mix": (0.0, 1.0), "predelay": (0.0, 1.0), "width": (0.0, 1.0),
            "earlyReflections": (0.0, 1.0),
        },
        "display_name": "混响",
    },
    "Compressor": {
        "params": ["threshold", "ratio", "attack", "release", "knee", "makeupGain", "mix"],
        "ranges": {
            "threshold": (0.0, 1.0), "ratio": (0.0, 1.0), "attack": (0.0, 1.0),
            "release": (0.0, 1.0), "knee": (0.0, 1.0), "makeupGain": (0.0, 1.0),
            "mix": (0.0, 1.0),
        },
        "display_name": "压缩器",
    },
    "EQ": {
        "params": [
            "band1Freq", "band1Gain", "band1Q", "band1Type",
            "band2Freq", "band2Gain", "band2Q", "band2Type",
            "band3Freq", "band3Gain", "band3Q", "band3Type",
            "band4Freq", "band4Gain", "band4Q", "band4Type",
            "band5Freq", "band5Gain", "band5Q", "band5Type",
            "band6Freq", "band6Gain", "band6Q", "band6Type",
            "band7Freq", "band7Gain", "band7Q", "band7Type",
            "band8Freq", "band8Gain", "band8Q", "band8Type",
        ],
        "ranges": {p: (0.0, 1.0) for p in [
            "band1Freq", "band1Gain", "band1Q", "band1Type",
            "band2Freq", "band2Gain", "band2Q", "band2Type",
            "band3Freq", "band3Gain", "band3Q", "band3Type",
            "band4Freq", "band4Gain", "band4Q", "band4Type",
            "band5Freq", "band5Gain", "band5Q", "band5Type",
            "band6Freq", "band6Gain", "band6Q", "band6Type",
            "band7Freq", "band7Gain", "band7Q", "band7Type",
            "band8Freq", "band8Gain", "band8Q", "band8Type",
        ]},
        "display_name": "均衡器",
    },
    "LFOGenerator": {
        "params": ["rate", "depth", "waveform", "phase", "delay", "fadeIn"],
        "ranges": {
            "rate": (0.0, 1.0), "depth": (0.0, 1.0), "waveform": (0.0, 1.0),
            "phase": (0.0, 1.0), "delay": (0.0, 1.0), "fadeIn": (0.0, 1.0),
        },
        "display_name": "LFO",
    },
    "EnvelopeGenerator": {
        "params": ["attack", "decay", "sustain", "release", "velocityAmount", "curve"],
        "ranges": {
            "attack": (0.0, 1.0), "decay": (0.0, 1.0), "sustain": (0.0, 1.0),
            "release": (0.0, 1.0), "velocityAmount": (0.0, 1.0), "curve": (0.0, 1.0),
        },
        "display_name": "包络",
    },
    "StepSequencer": {
        "params": ["rate", "steps", "gate", "smooth", "direction", "swing"],
        "ranges": {
            "rate": (0.0, 1.0), "steps": (0.0, 1.0), "gate": (0.0, 1.0),
            "smooth": (0.0, 1.0), "direction": (0.0, 1.0), "swing": (0.0, 1.0),
        },
        "display_name": "步进音序器",
    },
}

# 音色类别体系
CATEGORIES = {
    "Bass": {
        "name_cn": "贝斯",
        "subcategories": ["Sub", "Reese", "Pluck", "808", "FM", "Acid", "Wobble", "Neuro"],
        "osc_types": ["WavetableOscillator", "VirtualAnalogOscillator", "SpectralOscillator"],
        "filter_types": ["FilterProcessor", "EQ"],
        "fx_types": ["Distortion", "Compressor"],
    },
    "Lead": {
        "name_cn": "主音",
        "subcategories": ["Saw", "Square", "Supersaw", "Pluck", "Sync", "FM", "Vocal", "Arp"],
        "osc_types": ["WavetableOscillator", "VirtualAnalogOscillator", "SpectralOscillator", "NoiseGenerator"],
        "filter_types": ["FilterProcessor", "EQ"],
        "fx_types": ["Delay", "Reverb", "Distortion"],
    },
    "Pad": {
        "name_cn": "铺底",
        "subcategories": ["Warm", "Bright", "Evolving", "String", "Choir", "Atmosphere", "Dark", "Shimmer"],
        "osc_types": ["WavetableOscillator", "SpectralOscillator", "GranularPlayer", "VirtualAnalogOscillator"],
        "filter_types": ["FilterProcessor", "EQ"],
        "fx_types": ["Reverb", "Delay", "Compressor"],
    },
    "Pluck": {
        "name_cn": "拨弦",
        "subcategories": ["Guitar", "Harp", "Bell", "Mallet", "Koto", "Pizzicato", "Kalimba", "MusicBox"],
        "osc_types": ["WavetableOscillator", "WaveguideResonator", "MultiSampler"],
        "filter_types": ["FilterProcessor", "EQ"],
        "fx_types": ["Reverb", "Delay", "Compressor"],
    },
    "Keys": {
        "name_cn": "键盘",
        "subcategories": ["Piano", "ElectricPiano", "Organ", "Clav", "Harpsichord", "FM Piano", "Wurlitzer", "Rhodes"],
        "osc_types": ["WavetableOscillator", "MultiSampler", "VirtualAnalogOscillator", "SpectralOscillator"],
        "filter_types": ["FilterProcessor", "EQ"],
        "fx_types": ["Reverb", "Compressor", "EQ"],
    },
    "Arp": {
        "name_cn": "琶音",
        "subcategories": ["Up", "Down", "Random", "Chord", "Sequence", "Gate", "Trance", "Poly"],
        "osc_types": ["WavetableOscillator", "VirtualAnalogOscillator", "SpectralOscillator"],
        "filter_types": ["FilterProcessor"],
        "fx_types": ["Delay", "Reverb", "Distortion"],
    },
    "FX": {
        "name_cn": "音效",
        "subcategories": ["Riser", "Downer", "Impact", "Sweep", "Glitch", "Texture", "Drone", "Ambient"],
        "osc_types": ["NoiseGenerator", "GranularPlayer", "SpectralOscillator", "WavetableOscillator"],
        "filter_types": ["FilterProcessor", "EQ"],
        "fx_types": ["Reverb", "Delay", "Distortion"],
    },
    "Drum": {
        "name_cn": "鼓组",
        "subcategories": ["Kick", "Snare", "Hihat", "Clap", "Percussion", "Tom", "Cymbal", "808Kit"],
        "osc_types": ["DrumSlicer", "NoiseGenerator", "VirtualAnalogOscillator"],
        "filter_types": ["FilterProcessor", "EQ"],
        "fx_types": ["Compressor", "Distortion", "Reverb"],
    },
    "Vocal": {
        "name_cn": "人声",
        "subcategories": ["Choir", "VoxLead", "Talkbox", "Vocoder", "Whisper", "Formant", "Chant", "Breath"],
        "osc_types": ["SpectralOscillator", "WavetableOscillator", "GranularPlayer", "WaveguideResonator"],
        "filter_types": ["FilterProcessor", "EQ"],
        "fx_types": ["Reverb", "Delay", "Compressor"],
    },
    "Orchestral": {
        "name_cn": "管弦",
        "subcategories": ["Strings", "Brass", "Woodwind", "Ensemble", "Solo", "Section", "Cinematic", "Hybrid"],
        "osc_types": ["MultiSampler", "WaveguideResonator", "WavetableOscillator", "SpectralOscillator"],
        "filter_types": ["FilterProcessor", "EQ"],
        "fx_types": ["Reverb", "Compressor", "EQ"],
    },
}

# 音色名称模板
PRESET_NAME_TEMPLATES = {
    "Bass": [
        "{adj} {sub} Bass", "Deep {adj} {sub}", "{sub} {adj} Low",
        "Massive {adj} {sub}", "{adj} {sub} Rumble", "Fat {adj} {sub}",
        "Subterranean {adj} {sub}", "Punchy {adj} {sub}",
    ],
    "Lead": [
        "{adj} {sub} Lead", "Screaming {adj} {sub}", "{adj} {sub} Melody",
        "Bright {adj} {sub}", "{adj} {sub} Solo", "Cutting {adj} {sub}",
        "Aggressive {adj} {sub}", "Vintage {adj} {sub}",
    ],
    "Pad": [
        "{adj} {sub} Pad", "Lush {adj} {sub}", "Ethereal {adj} {sub}",
        "{adj} {sub} Atmosphere", "Dreamy {adj} {sub}", "Cinematic {adj} {sub}",
        "Warm {adj} {sub}", "Expansive {adj} {sub}",
    ],
    "Pluck": [
        "{adj} {sub} Pluck", "Crisp {adj} {sub}", "{adj} {sub} Pick",
        "Delicate {adj} {sub}", "Sharp {adj} {sub}", "Resonant {adj} {sub}",
        "Wooden {adj} {sub}", "Metallic {adj} {sub}",
    ],
    "Keys": [
        "{adj} {sub} Keys", "Classic {adj} {sub}", "{adj} {sub} Piano",
        "Vintage {adj} {sub}", "Modern {adj} {sub}", "Warm {adj} {sub}",
        "Bright {adj} {sub}", "Mellow {adj} {sub}",
    ],
    "Arp": [
        "{adj} {sub} Arp", "Rhythmic {adj} {sub}", "{adj} {sub} Sequence",
        "Pulsing {adj} {sub}", "Hypnotic {adj} {sub}", "Driving {adj} {sub}",
        "Melodic {adj} {sub}", "Complex {adj} {sub}",
    ],
    "FX": [
        "{adj} {sub} FX", "Rising {adj} {sub}", "{adj} {sub} Sweep",
        "Cinematic {adj} {sub}", "Glitchy {adj} {sub}", "Dark {adj} {sub}",
        "Evolving {adj} {sub}", "Atmospheric {adj} {sub}",
    ],
    "Drum": [
        "{adj} {sub} Drum", "Hard {adj} {sub}", "{adj} {sub} Hit",
        "Tight {adj} {sub}", "Heavy {adj} {sub}", "Crisp {adj} {sub}",
        "Punchy {adj} {sub}", "Deep {adj} {sub}",
    ],
    "Vocal": [
        "{adj} {sub} Vocal", "Heavenly {adj} {sub}", "{adj} {sub} Choir",
        "Synthetic {adj} {sub}", "Robotic {adj} {sub}", "Ethereal {adj} {sub}",
        "Mystical {adj} {sub}", "Angelic {adj} {sub}",
    ],
    "Orchestral": [
        "{adj} {sub} Orchestra", "Grand {adj} {sub}", "{adj} {sub} Ensemble",
        "Epic {adj} {sub}", "Cinematic {adj} {sub}", "Majestic {adj} {sub}",
        "Sweeping {adj} {sub}", "Dramatic {adj} {sub}",
    ],
}

ADJECTIVES = [
    "Warm", "Bright", "Dark", "Soft", "Hard", "Smooth", "Rough", "Rich",
    "Thin", "Thick", "Light", "Heavy", "Clean", "Dirty", "Crisp", "Muddy",
    "Airy", "Dense", "Sharp", "Round", "Wide", "Narrow", "Deep", "Shallow",
    "Fast", "Slow", "Tight", "Loose", "Dry", "Wet", "Cool", "Hot",
    "Silky", "Gritty", "Punchy", "Mellow", "Aggressive", "Gentle", "Bold", "Subtle",
    "Vintage", "Modern", "Analog", "Digital", "Organic", "Synthetic", "Natural", "Processed",
]

AI_PROMPT_TEMPLATES = [
    "生成一个{adj}的{category_cn}音色，适合{genre}风格",
    "创建一个{category_cn}预设，带{adj}质感和{adj2}特性",
    "设计一个{category_cn}音色，需要{adj}的音头和{adj2}的尾音",
    "制作一个{genre}风格的{category_cn}音色，强调{adj}表现力",
    "合成一个{adj}的{category_cn}，带有{adj2}的调制效果",
]

GENRES = [
    "电子", "流行", "嘻哈", "摇滚", "爵士", "古典", "氛围", "舞曲",
    "Techno", "House", "Trance", "Dubstep", "Drum&Bass", "Lo-Fi",
    "Synthwave", "Trap", "Future Bass", "Ambient", "Cinematic", "EDM",
]

AUTHORS = [
    "LianCore AI", "Synthetic Engine", "PresetGen v1", "AI Sound Designer",
    "NeuralSynth", "DeepSound AI", "AutoPreset Bot", "WaveGen AI",
    "SonicAI", "TimbreForge", "SoundCraft AI", "SynthMaster AI",
]

# =============================================================================
# 数据生成函数
# =============================================================================

def generate_params(node_type, rng, character_bias=None):
    """为指定节点类型生成随机参数值"""
    if node_type not in NODE_PARAM_DEFS:
        return {}
    defs = NODE_PARAM_DEFS[node_type]
    params = {}
    for pname in defs["params"]:
        lo, hi = defs["ranges"][pname]
        if character_bias:
            # 根据音色倾向调整分布
            if character_bias == "bright" and pname in ("cutoff", "freq"):
                val = rng.betavariate(3, 1.5)  # 偏向高值
            elif character_bias == "dark" and pname in ("cutoff", "freq"):
                val = rng.betavariate(1.5, 3)  # 偏向低值
            elif character_bias == "punchy" and pname in ("attack", "drive"):
                val = rng.betavariate(4, 1.5)  # 快攻/高驱动
            elif character_bias == "soft" and pname in ("attack", "drive"):
                val = rng.betavariate(1.5, 4)  # 慢攻/低驱动
            else:
                val = rng.random()
        else:
            val = rng.random()
        params[pname] = round(lo + val * (hi - lo), 4)
    return params


def generate_node_graph(category, subcategory, rng):
    """生成完整的节点图配置"""
    cat_info = CATEGORIES[category]
    nodes = []
    connections = []
    node_id = 0

    # 确定音色倾向
    character_biases = ["bright", "dark", "punchy", "soft", "warm", "cold", None]
    character = rng.choice(character_biases)

    # 1. 选择振荡器 (1-2个)
    num_osc = rng.choices([1, 2, 3], weights=[0.5, 0.35, 0.15])[0]
    osc_pool = cat_info["osc_types"]
    for i in range(num_osc):
        osc_type = rng.choice(osc_pool)
        nid = f"n{node_id}"
        node_id += 1
        nodes.append({
            "id": nid,
            "type": osc_type,
            "name": f"OSC {i+1}" if num_osc > 1 else "OSC",
            "enabled": True,
            "params": generate_params(osc_type, rng, character),
        })

    # 2. 滤波器 (1个)
    if cat_info["filter_types"] and rng.random() < 0.9:
        filt_type = rng.choice(cat_info["filter_types"])
        nid = f"n{node_id}"
        node_id += 1
        nodes.append({
            "id": nid,
            "type": filt_type,
            "name": "Filter",
            "enabled": True,
            "params": generate_params(filt_type, rng, character),
        })
        # 连接最后一个振荡器到滤波器
        if nodes:
            connections.append({
                "src": nodes[-2]["id"], "dst": nid,
                "srcPort": 0, "dstPort": 0,
            })

    # 3. 效果器 (1-3个)
    num_fx = rng.choices([0, 1, 2, 3], weights=[0.1, 0.35, 0.35, 0.2])[0]
    fx_pool = cat_info["fx_types"]
    last_node_id = f"n{node_id - 1}" if nodes else None
    for i in range(num_fx):
        fx_type = rng.choice(fx_pool)
        nid = f"n{node_id}"
        node_id += 1
        nodes.append({
            "id": nid,
            "type": fx_type,
            "name": fx_type if fx_type != "Delay" else "Echo",
            "enabled": rng.random() < 0.85,
            "params": generate_params(fx_type, rng, character),
        })
        if last_node_id:
            connections.append({
                "src": last_node_id, "dst": nid,
                "srcPort": 0, "dstPort": 0,
            })
        last_node_id = nid

    # 4. 调制器 (0-2个)
    num_mod = rng.choices([0, 1, 2], weights=[0.3, 0.5, 0.2])[0]
    mod_types = ["LFOGenerator", "EnvelopeGenerator", "StepSequencer"]
    for i in range(num_mod):
        mod_type = rng.choice(mod_types)
        nid = f"n{node_id}"
        node_id += 1
        nodes.append({
            "id": nid,
            "type": mod_type,
            "name": f"{mod_type.replace('Generator', '')} {i+1}",
            "enabled": True,
            "params": generate_params(mod_type, rng),
        })
        # 调制连接到第一个节点
        if nodes:
            connections.append({
                "src": nid, "dst": nodes[0]["id"],
                "srcPort": 0, "dstPort": 0,
            })

    # 5. 输出节点
    nid = f"n{node_id}"
    nodes.append({
        "id": nid,
        "type": "AudioOutput",
        "name": "Output",
        "enabled": True,
        "params": {},
    })
    if last_node_id:
        connections.append({
            "src": last_node_id, "dst": nid,
            "srcPort": 0, "dstPort": 0,
        })

    return {
        "version": "3.0.0",
        "format": "LianCorePreset",
        "nodes": nodes,
        "connections": connections,
        "metadata": {
            "character": character or "neutral",
            "nodeCount": len(nodes),
            "connectionCount": len(connections),
        },
    }


def generate_preset_name(category, subcategory, rng):
    """生成音色名称"""
    template = rng.choice(PRESET_NAME_TEMPLATES.get(category, ["{adj} {sub} Sound"]))
    adj = rng.choice(ADJECTIVES)
    return template.format(adj=adj, sub=subcategory)


def generate_tags(category, subcategory, node_graph, rng):
    """生成标签列表"""
    tags = [category, subcategory]
    # 添加节点类型标签
    for node in node_graph["nodes"]:
        if node["type"] != "AudioOutput":
            tags.append(node["type"])
    # 添加音色特征标签
    feature_tags = ["Analog", "Digital", "Stereo", "Mono", "Complex", "Simple",
                    "Modulated", "Static", "Layered", "Clean"]
    tags.extend(rng.sample(feature_tags, k=rng.randint(2, 4)))
    # 去重
    return list(dict.fromkeys(tags))


def generate_description(category, subcategory, node_graph, rng):
    """生成音色描述"""
    cat_info = CATEGORIES[category]
    node_types = [n["type"] for n in node_graph["nodes"] if n["type"] != "AudioOutput"]
    osc_count = sum(1 for t in node_types if "Oscillator" in t or "Generator" in t or "Player" in t or "Sampler" in t or "Slicer" in t)
    fx_count = sum(1 for t in node_types if t in ("Distortion", "Delay", "Reverb", "Compressor", "EQ"))

    templates = [
        f"一个{cat_info['name_cn']}类别的{subcategory}风格音色，使用{osc_count}个振荡器{'和' + str(fx_count) + '个效果器' if fx_count > 0 else ''}。{rng.choice(ADJECTIVES)}的音色质感。",
        f"{cat_info['name_cn']}预设 - {subcategory}。包含{', '.join(node_types[:3])}等{'个' if len(node_types) > 3 else ''}模块。适合{rng.choice(GENRES)}风格。",
        f"专业级{cat_info['name_cn']}音色，{subcategory}类型。信号链: {' -> '.join(node_types[:4])}。{rng.choice(['温暖', '明亮', '浑厚', '清晰', '饱满'])}的声音表现。",
    ]
    return rng.choice(templates)


def generate_ai_prompt(category, subcategory, rng):
    """生成AI提示词"""
    cat_info = CATEGORIES[category]
    template = rng.choice(AI_PROMPT_TEMPLATES)
    adj = rng.choice(ADJECTIVES)
    adj2 = rng.choice(ADJECTIVES)
    genre = rng.choice(GENRES)
    return template.format(
        adj=adj, adj2=adj2, category_cn=cat_info["name_cn"],
        sub=subcategory, genre=genre,
    )


# =============================================================================
# 数据库操作
# =============================================================================

def create_tables(conn):
    """创建数据库表"""
    conn.execute("""
        CREATE TABLE IF NOT EXISTS presets (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            category TEXT,
            tags TEXT,
            description TEXT,
            author TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            json_data TEXT NOT NULL,
            ai_prompt TEXT,
            ai_confidence REAL DEFAULT 0,
            rating INTEGER DEFAULT 0,
            usage_count INTEGER DEFAULT 0
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS preset_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            preset_id INTEGER,
            version INTEGER DEFAULT 1,
            json_data TEXT NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (preset_id) REFERENCES presets(id)
        )
    """)
    # 索引
    conn.execute("CREATE INDEX IF NOT EXISTS idx_presets_category ON presets(category)")
    conn.execute("CREATE INDEX IF NOT EXISTS idx_presets_name ON presets(name)")
    conn.execute("CREATE INDEX IF NOT EXISTS idx_presets_rating ON presets(rating)")
    conn.execute("CREATE INDEX IF NOT EXISTS idx_presets_updated ON presets(updated_at)")
    conn.commit()


def insert_presets_batch(conn, presets_data):
    """批量插入预设数据"""
    conn.executemany("""
        INSERT INTO presets (name, category, tags, description, author, created_at, updated_at,
                            json_data, ai_prompt, ai_confidence, rating, usage_count)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    """, presets_data)
    conn.commit()


# =============================================================================
# 主流程
# =============================================================================

def main():
    parser = argparse.ArgumentParser(description="LianCore 合成数据驱动预制库填充脚本")
    parser.add_argument("--count", type=int, default=1000000, help="生成预设数量 (默认: 1000000)")
    parser.add_argument("--db", type=str, default="preset_library_1M.db", help="数据库文件路径")
    parser.add_argument("--batch-size", type=int, default=10000, help="批量插入大小 (默认: 10000)")
    parser.add_argument("--seed", type=int, default=42, help="随机种子 (默认: 42)")
    parser.add_argument("--output-dir", type=str, default=".", help="输出目录")
    args = parser.parse_args()

    # 初始化随机种子
    rng = random.Random(args.seed)
    random.seed(args.seed)

    db_path = os.path.join(args.output_dir, args.db)
    print(f"[LianCore] 预制库填充脚本启动")
    print(f"  目标数量: {args.count:,}")
    print(f"  数据库: {db_path}")
    print(f"  批量大小: {args.batch_size:,}")
    print(f"  随机种子: {args.seed}")

    # 删除旧数据库
    if os.path.exists(db_path):
        os.remove(db_path)
        print(f"  已删除旧数据库")

    # 创建数据库
    conn = sqlite3.connect(db_path)
    conn.execute("PRAGMA journal_mode=WAL")
    conn.execute("PRAGMA synchronous=OFF")
    conn.execute("PRAGMA cache_size=-256000")  # P7-1: 256MB缓存以支持100万级数据
    conn.execute("PRAGMA mmap_size=1073741824")  # P7-1: 1GB内存映射加速
    create_tables(conn)

    # 统计
    stats = Counter()
    category_dist = Counter()
    subcategory_dist = Counter()
    node_type_dist = Counter()

    start_time = time.time()
    batch = []
    total_generated = 0

    # 类别权重 (按比例分配)
    category_weights = {
        "Bass": 0.18, "Lead": 0.18, "Pad": 0.16, "Pluck": 0.10,
        "Keys": 0.12, "Arp": 0.08, "FX": 0.06, "Drum": 0.05,
        "Vocal": 0.03, "Orchestral": 0.04,
    }
    categories_list = list(category_weights.keys())
    category_probs = [category_weights[c] for c in categories_list]

    print(f"\n[LianCore] 开始生成 {args.count:,} 条预设...")

    for i in range(args.count):
        # 选择类别
        category = rng.choices(categories_list, weights=category_probs, k=1)[0]
        cat_info = CATEGORIES[category]
        subcategory = rng.choice(cat_info["subcategories"])

        # 生成节点图
        node_graph = generate_node_graph(category, subcategory, rng)
        json_data = json.dumps(node_graph, ensure_ascii=False)

        # 生成元数据
        name = generate_preset_name(category, subcategory, rng)
        tags = json.dumps(generate_tags(category, subcategory, node_graph, rng), ensure_ascii=False)
        description = generate_description(category, subcategory, node_graph, rng)
        ai_prompt = generate_ai_prompt(category, subcategory, rng)
        author = rng.choice(AUTHORS)
        ai_confidence = round(rng.uniform(0.55, 0.98), 3)
        rating = rng.choices([0, 1, 2, 3, 4, 5], weights=[5, 8, 12, 20, 30, 25])[0]
        usage_count = rng.randint(0, 5000)

        # 时间戳 (分布在过去 365 天内)
        days_ago = rng.randint(0, 365)
        created_at = (datetime.now() - timedelta(days=days_ago)).strftime("%Y-%m-%d %H:%M:%S")
        updated_days = rng.randint(0, days_ago)
        updated_at = (datetime.now() - timedelta(days=updated_days)).strftime("%Y-%m-%d %H:%M:%S")

        batch.append((name, category, tags, description, author, created_at, updated_at,
                      json_data, ai_prompt, ai_confidence, rating, usage_count))

        # 统计
        stats["total"] += 1
        category_dist[category] += 1
        subcategory_dist[f"{category}/{subcategory}"] += 1
        for node in node_graph["nodes"]:
            if node["type"] != "AudioOutput":
                node_type_dist[node["type"]] += 1

        # 批量插入
        if len(batch) >= args.batch_size:
            insert_presets_batch(conn, batch)
            total_generated += len(batch)
            elapsed = time.time() - start_time
            rate = total_generated / elapsed if elapsed > 0 else 0
            pct = total_generated / args.count * 100
            eta = (args.count - total_generated) / rate if rate > 0 else 0
            print(f"  [{total_generated:>8,}/{args.count:,}] {pct:5.1f}% | {rate:,.0f} 条/秒 | ETA {eta:.0f}s | 耗时 {elapsed:.0f}s")
            batch = []

            # P7-1: 每10万条做一次WAL检查点，控制WAL文件大小
            if total_generated % 100000 == 0:
                conn.execute("PRAGMA wal_checkpoint(TRUNCATE)")
                print(f"  [检查点] WAL已截断于 {total_generated:,} 条")

    # 插入剩余数据
    if batch:
        insert_presets_batch(conn, batch)
        total_generated += len(batch)

    # 生成部分版本历史 (约10%的预设)
    print(f"\n[LianCore] 生成版本历史...")
    version_count = 0
    version_batch = []
    for preset_id in range(1, total_generated + 1, 10):  # 每10个预设取1个
        num_versions = rng.randint(1, 5)
        for v in range(1, num_versions + 1):
            category = rng.choice(categories_list)
            subcategory = rng.choice(CATEGORIES[category]["subcategories"])
            node_graph = generate_node_graph(category, subcategory, rng)
            json_data = json.dumps(node_graph, ensure_ascii=False)
            version_batch.append((preset_id, v, json_data))
            version_count += 1
            if len(version_batch) >= args.batch_size:
                conn.executemany(
                    "INSERT INTO preset_history (preset_id, version, json_data) VALUES (?, ?, ?)",
                    version_batch
                )
                conn.commit()
                version_batch = []

    if version_batch:
        conn.executemany(
            "INSERT INTO preset_history (preset_id, version, json_data) VALUES (?, ?, ?)",
            version_batch
        )
        conn.commit()

    # 最终统计
    elapsed = time.time() - start_time
    conn.close()

    db_size = os.path.getsize(db_path)
    print(f"\n{'='*60}")
    print(f"[LianCore] 生成完成!")
    print(f"  预设总数: {total_generated:,}")
    print(f"  版本历史: {version_count:,}")
    print(f"  数据库大小: {db_size / (1024*1024):.1f} MB")
    print(f"  总耗时: {elapsed:.1f} 秒")
    print(f"  平均速率: {total_generated / elapsed:,.0f} 条/秒")

    # 类别分布
    print(f"\n[LianCore] 类别分布:")
    for cat, count in sorted(category_dist.items(), key=lambda x: -x[1]):
        pct = count / total_generated * 100
        bar = "█" * int(pct / 2)
        print(f"  {cat:12s} ({CATEGORIES[cat]['name_cn']:4s}): {count:>8,} ({pct:5.1f}%) {bar}")

    # 节点类型分布
    print(f"\n[LianCore] 节点类型分布 (Top 10):")
    for ntype, count in node_type_dist.most_common(10):
        pct = count / sum(node_type_dist.values()) * 100
        print(f"  {ntype:30s}: {count:>10,} ({pct:5.1f}%)")

    # 生成统计报告
    report = {
        "generated_at": datetime.now().isoformat(),
        "total_presets": total_generated,
        "total_versions": version_count,
        "database_size_bytes": db_size,
        "generation_time_seconds": round(elapsed, 2),
        "average_rate_per_second": round(total_generated / elapsed, 1),
        "seed": args.seed,
        "category_distribution": dict(category_dist),
        "node_type_distribution": dict(node_type_dist),
        "top_subcategories": dict(subcategory_dist.most_common(20)),
    }
    report_path = os.path.join(args.output_dir, "preset_generation_report.json")
    with open(report_path, "w", encoding="utf-8") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)
    print(f"\n[LianCore] 统计报告已保存: {report_path}")
    print(f"[LianCore] 数据库文件: {db_path}")


if __name__ == "__main__":
    main()