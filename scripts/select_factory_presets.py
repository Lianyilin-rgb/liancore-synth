#!/usr/bin/env python3
# LianCore V3 - 出厂预设精选与分类优化
# 从10万预设中精选500个高质量出厂预设，按风格/乐器/情绪精细化分类
import sqlite3
import json
import os
import random
from collections import Counter

DB_PATH = "data/preset_library.db"
OUTPUT_DB = "data/factory_presets.db"
REPORT_FILE = "data/factory_preset_report.json"
MAX_PRESETS = 500

# 精细化分类定义
CATEGORIES = {
    "风格": ["Bass", "Lead", "Pad", "Pluck", "Keys", "FX", "Synth", "Arp", "Chord", "Drums"],
    "乐器": ["Analog", "FM", "Wavetable", "Granular", "Additive", "Subtractive", "Physical", "Hybrid"],
    "情绪": ["Bright", "Dark", "Warm", "Cold", "Aggressive", "Soft", "Evolving", "Static", "Ethereal", "Punchy"]
}

def score_preset(preset):
    """评分算法: 基于参数多样性和预设完整度"""
    score = 0.0
    name = preset.get('name', '')
    params = preset.get('params', '')
    tags = preset.get('tags', '')
    category = preset.get('category', '')
    
    # 名称长度 (2-32字符最优)
    name_len = len(name)
    if 2 <= name_len <= 32:
        score += 10.0
    elif 1 <= name_len <= 64:
        score += 5.0
    
    # 参数复杂度
    if params and len(params) > 10:
        score += 15.0
    
    # 标签多样性
    if tags:
        tag_list = tags.split(',')
        score += min(len(tag_list) * 3, 20.0)
    
    # 分类完善度
    if category and category in CATEGORIES["风格"]:
        score += 10.0
    
    # 随机扰动 (避免重复)
    score += random.uniform(0, 5.0)
    return score


def auto_classify(preset):
    """自动分类：根据名称和标签推断风格/乐器/情绪"""
    name = preset.get('name', '').lower()
    tags = preset.get('tags', '').lower()
    combined = name + " " + tags
    
    styles = []
    instruments = []
    moods = []
    
    # 风格关键词
    style_keywords = {
        "Bass": ["bass", "sub", "low", "808", "deep bass"],
        "Lead": ["lead", "solo", "melody", "mono", "lead synth"],
        "Pad": ["pad", "atmosphere", "ambient", "drone", "string"],
        "Pluck": ["pluck", "pizz", "harp", "guitar", "picked"],
        "Keys": ["key", "piano", "organ", "ep", "rhodes", "clav"],
        "FX": ["fx", "sfx", "effect", "noise", "sweep", "riser", "down"],
        "Synth": ["synth", "saw", "square", "pulse", "brass", "synth"],
        "Arp": ["arp", "sequence", "pattern", "arpeggio"],
        "Chord": ["chord", "stab", "hit", "ensemble"],
        "Drums": ["drum", "kick", "snare", "hat", "perc", "beat"]
    }
    
    for style, keywords in style_keywords.items():
        for kw in keywords:
            if kw in combined:
                styles.append(style)
                break
    
    # 乐器关键词
    inst_keywords = {
        "Analog": ["analog", "vintage", "classic", "moog", "roland", "warm"],
        "FM": ["fm", "dx", "frequency modulation", "digital", "operator"],
        "Wavetable": ["wavetable", "wave", "table", "scan", "wav"],
        "Granular": ["granular", "grain", "particle", "texture", "cloud"],
        "Additive": ["additive", "harmonic", "partial", "sine", "overtone"],
        "Subtractive": ["subtractive", "filter", "lpf", "resonance", "cutoff"],
        "Physical": ["physical", "model", "string", "reed", "bow", "resonator"],
        "Hybrid": ["hybrid", "layer", "combo", "mix", "blend"]
    }
    
    for inst, keywords in inst_keywords.items():
        for kw in keywords:
            if kw in combined:
                instruments.append(inst)
                break
    
    # 情绪关键词
    mood_keywords = {
        "Bright": ["bright", "shiny", "sparkle", "crystal", "clear", "luminous"],
        "Dark": ["dark", "deep", "mysterious", "shadow", "night", "brooding"],
        "Warm": ["warm", "rich", "full", "smooth", "analog", "cozy"],
        "Cold": ["cold", "icy", "metallic", "digital", "sterile", "frozen"],
        "Aggressive": ["aggressive", "hard", "distorted", "gritty", "harsh", "fierce"],
        "Soft": ["soft", "gentle", "mellow", "smooth", "calm", "delicate"],
        "Evolving": ["evolving", "motion", "moving", "sweeping", "dynamic", "morph"],
        "Static": ["static", "steady", "constant", "fixed", "hold", "stable"],
        "Ethereal": ["ethereal", "dreamy", "floating", "airy", "heavenly", "celestial"],
        "Punchy": ["punchy", "hit", "impact", "attack", "transient", "percussive"]
    }
    
    for mood, keywords in mood_keywords.items():
        for kw in keywords:
            if kw in combined:
                moods.append(mood)
                break
    
    # 默认分类
    if not styles:
        styles.append("Synth")
    if not instruments:
        instruments.append("Subtractive")
    if not moods:
        moods.append("Soft")
    
    return styles[0], instruments[0], moods[0]


def main():
    print("LianCore V3 - 出厂预设精选与分类优化")
    print("=" * 50)
    
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    
    # 获取所有预设
    cursor.execute("SELECT name, params, tags, category FROM presets")
    rows = cursor.fetchall()
    print(f"总预设数: {len(rows)}")
    
    if len(rows) == 0:
        print("警告: 预设库为空，使用模拟数据")
        rows = []
        for i in range(100000):
            rows.append((f"Preset_{i:05d}", "{}", f"tag_{i%20}", "Synth"))
        print(f"使用模拟数据: {len(rows)} 预设")
    
    # 评分和排序
    presets = []
    for row in rows:
        preset = {
            'name': row[0] if row[0] else f"Preset_{len(presets)}",
            'params': row[1] if len(row) > 1 else '{}',
            'tags': row[2] if len(row) > 2 else '',
            'category': row[3] if len(row) > 3 else ''
        }
        score = score_preset(preset)
        preset['score'] = score
        presets.append(preset)
    
    presets.sort(key=lambda x: x['score'], reverse=True)
    
    # 精选前500个，确保分类多样性
    selected = []
    category_counts = Counter()
    
    for preset in presets:
        if len(selected) >= MAX_PRESETS:
            break
        style, instrument, mood = auto_classify(preset)
        cat_key = f"{style}|{instrument}|{mood}"
        
        # 每个分类最多15个，确保多样性
        if category_counts[cat_key] < 15:
            preset['style'] = style
            preset['instrument'] = instrument
            preset['mood'] = mood
            selected.append(preset)
            category_counts[cat_key] += 1
    
    # 如果不足500，追加高分预设
    if len(selected) < MAX_PRESETS:
        for preset in presets:
            if len(selected) >= MAX_PRESETS:
                break
            if preset not in selected:
                style, instrument, mood = auto_classify(preset)
                preset['style'] = style
                preset['instrument'] = instrument
                preset['mood'] = mood
                selected.append(preset)
    
    print(f"精选预设数: {len(selected)}")
    
    # 统计分布
    style_counts = Counter(p['style'] for p in selected)
    inst_counts = Counter(p['instrument'] for p in selected)
    mood_counts = Counter(p['mood'] for p in selected)
    
    print("\n=== 风格分布 ===")
    for style, count in style_counts.most_common():
        print(f"  {style}: {count}")
    
    print("\n=== 乐器分布 ===")
    for inst, count in inst_counts.most_common():
        print(f"  {inst}: {count}")
    
    print("\n=== 情绪分布 ===")
    for mood, count in mood_counts.most_common():
        print(f"  {mood}: {count}")
    
    # 保存到新数据库
    out_conn = sqlite3.connect(OUTPUT_DB)
    out_cursor = out_conn.cursor()
    out_cursor.execute('''
        CREATE TABLE IF NOT EXISTS factory_presets (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            params TEXT,
            tags TEXT,
            style TEXT,
            instrument TEXT,
            mood TEXT,
            score REAL,
            quality TEXT DEFAULT 'high'
        )
    ''')
    out_cursor.execute("DELETE FROM factory_presets")
    
    for i, preset in enumerate(selected):
        out_cursor.execute(
            "INSERT INTO factory_presets (name, params, tags, style, instrument, mood, score) VALUES (?, ?, ?, ?, ?, ?, ?)",
            (preset['name'], preset['params'], preset['tags'],
             preset['style'], preset['instrument'], preset['mood'], preset['score'])
        )
    
    out_conn.commit()
    out_conn.close()
    
    # 生成JSON报告
    report = {
        "title": "LianCore V3 出厂预设精选报告",
        "total_presets": len(selected),
        "selection_date": __import__('datetime').datetime.now().isoformat(),
        "style_distribution": dict(style_counts.most_common()),
        "instrument_distribution": dict(inst_counts.most_common()),
        "mood_distribution": dict(mood_counts.most_common()),
        "quality_metrics": {
            "avg_score": sum(p['score'] for p in selected) / len(selected) if selected else 0,
            "max_score": max(p['score'] for p in selected) if selected else 0,
            "min_score": min(p['score'] for p in selected) if selected else 0
        }
    }
    
    with open(REPORT_FILE, 'w', encoding='utf-8') as f:
        json.dump(report, f, ensure_ascii=False, indent=2)
    
    print(f"\n报告已保存: {REPORT_FILE}")
    print(f"精选数据库: {OUTPUT_DB}")
    conn.close()


if __name__ == "__main__":
    main()
