#!/usr/bin/env python3
# =============================================================================
# LianCore V3 - 出厂预设精选与质量提升 (P8-1)
# 从百万级预设库中精选500个高质量出厂预设
# 评分算法: 参数多样性 + 音色独特性 + 类别均衡
# 每个预设附带AI生成的中文描述（基于规则模板引擎）
# =============================================================================
import sqlite3
import json
import os
import sys
import time
import random
import hashlib
from collections import Counter, defaultdict
from datetime import datetime

# ---------------------------------------------------------------------------
# 配置
# ---------------------------------------------------------------------------
DB_PATH = "data/preset_library.db"
OUTPUT_DB = "data/factory_presets.db"
REPORT_FILE = "data/factory_preset_report.json"
MAX_PRESETS = 500
BATCH_SIZE = 10000  # 分批处理大小
QUALITY_LEVELS = ["premium", "high", "standard"]

# ---------------------------------------------------------------------------
# 精细化分类定义（10风格 x 8乐器 x 10情绪 = 800种组合）
# ---------------------------------------------------------------------------
CATEGORIES = {
    "风格": ["Bass", "Lead", "Pad", "Pluck", "Keys", "FX", "Synth", "Arp", "Chord", "Drums"],
    "乐器": ["Analog", "FM", "Wavetable", "Granular", "Additive", "Subtractive", "Physical", "Hybrid"],
    "情绪": ["Bright", "Dark", "Warm", "Cold", "Aggressive", "Soft", "Evolving", "Static", "Ethereal", "Punchy"]
}

# ---------------------------------------------------------------------------
# 中文描述模板引擎（纯本地，不依赖云端AI）
# ---------------------------------------------------------------------------
STYLE_DESCRIPTIONS = {
    "Bass": {
        "intro": ["厚重的低频音色", "深沉有力的贝斯声", "饱满的低音线条", "震撼的低频冲击"],
        "color": ["温暖浑厚", "紧实有力", "深沉饱满", "弹性十足"],
        "use": ["电子音乐的低音基础", "Hip-Hop 808风格", "舞曲的低频驱动", "影视配乐的低音铺垫"]
    },
    "Lead": {
        "intro": ["明亮的主旋律音色", "穿透力极强的领奏声", "富有表现力的独奏音色", "清晰锐利的主音"],
        "color": ["明亮通透", "锐利激昂", "富有穿透力", "华丽炫技"],
        "use": ["电子音乐的主旋律", "摇滚吉他独奏风格", "EDM Drop段落", "流行音乐的前奏"]
    },
    "Pad": {
        "intro": ["大气磅礴的铺底音色", "梦幻飘渺的背景氛围", "温暖包裹的填充音色", "广阔深邃的空间感"],
        "color": ["温暖柔和", "空灵飘渺", "大气磅礴", "梦幻深邃"],
        "use": ["氛围音乐的空间营造", "影视配乐的情感铺垫", "Chillwave风格", "冥想音乐背景"]
    },
    "Pluck": {
        "intro": ["清脆弹拨音色", "短促有力的拨弦声", "富有节奏感的弹奏音", "清晰利落的拨音"],
        "color": ["清脆明亮", "短促有力", "弹性十足", "干净利落"],
        "use": ["电子音乐的琶音段落", "Future Bass风格", "流行音乐的节奏装饰", "游戏配乐"]
    },
    "Keys": {
        "intro": ["经典键盘音色", "复古电钢琴声", "温暖的风琴音色", "优雅的钢琴质感"],
        "color": ["温暖经典", "复古优雅", "明亮清晰", "细腻丰富"],
        "use": ["流行音乐的键盘伴奏", "爵士乐风格", "R&B和Soul", "Lo-fi Hip-Hop"]
    },
    "FX": {
        "intro": ["酷炫的特效音色", "科幻感十足的音效", "戏剧性的声音效果", "前卫的声音设计"],
        "color": ["科幻前卫", "戏剧夸张", "神秘莫测", "冲击力强"],
        "use": ["电子音乐的过渡效果", "科幻影视配乐", "游戏音效设计", "实验音乐"]
    },
    "Synth": {
        "intro": ["经典的合成器音色", "复古模拟合成器声", "现代数字合成音色", "标志性的电子音色"],
        "color": ["经典复古", "现代前卫", "丰富多变", "标志性"],
        "use": ["Synthwave风格", "电子流行音乐", "复古迪斯科", "现代电子音乐"]
    },
    "Arp": {
        "intro": ["动感的琶音音色", "流畅的旋律序列", "节奏感十足的琶音", "迷人的音序模式"],
        "color": ["流畅动感", "节奏明快", "旋律优美", "律动十足"],
        "use": ["Trance音乐", "Techno风格", "电子音乐的旋律驱动", "游戏配乐BGM"]
    },
    "Chord": {
        "intro": ["丰富的和弦音色", "饱满的和声质感", "华丽的和弦进行", "立体的和声层次"],
        "color": ["丰富饱满", "华丽大气", "和谐温暖", "立体感强"],
        "use": ["House音乐的和弦进行", "Progressive风格", "流行音乐的伴奏", "EDM的情感铺垫"]
    },
    "Drums": {
        "intro": ["有力的打击乐音色", "震撼的鼓组声", "节奏感强烈的打击声", "动感十足的节奏"],
        "color": ["强劲有力", "节奏鲜明", "震撼冲击", "律动感强"],
        "use": ["电子音乐的节奏基础", "Trap风格", "Drum & Bass", "Hip-Hop节拍"]
    }
}

INSTRUMENT_DESCRIPTIONS = {
    "Analog": "采用模拟合成技术，",
    "FM": "采用FM频率调制合成，",
    "Wavetable": "采用波表合成技术，",
    "Granular": "采用粒子合成技术，",
    "Additive": "采用加法合成技术，",
    "Subtractive": "采用减法合成技术，",
    "Physical": "采用物理建模合成，",
    "Hybrid": "采用混合合成技术，"
}

MOOD_DESCRIPTIONS = {
    "Bright": {
        "feel": "明亮愉悦",
        "scene": "适合阳光明媚的场景"
    },
    "Dark": {
        "feel": "深邃神秘",
        "scene": "适合昏暗氛围的场景"
    },
    "Warm": {
        "feel": "温暖舒适",
        "scene": "适合温馨放松的场景"
    },
    "Cold": {
        "feel": "冷峻清冽",
        "scene": "适合冰冷科技感的场景"
    },
    "Aggressive": {
        "feel": "激进有力",
        "scene": "适合高能量爆发的场景"
    },
    "Soft": {
        "feel": "柔和细腻",
        "scene": "适合安静内省的场景"
    },
    "Evolving": {
        "feel": "不断变化",
        "scene": "适合需要动态发展的场景"
    },
    "Static": {
        "feel": "稳定持续",
        "scene": "适合需要稳定背景的场景"
    },
    "Ethereal": {
        "feel": "空灵梦幻",
        "scene": "适合梦幻仙境的场景"
    },
    "Punchy": {
        "feel": "冲击力强",
        "scene": "适合需要节奏冲击的场景"
    }
}


def generate_chinese_description(preset):
    """基于规则模板引擎生成中文描述（纯本地，不依赖云端AI）"""
    style = preset.get('style', 'Synth')
    instrument = preset.get('instrument', 'Subtractive')
    mood = preset.get('mood', 'Soft')
    name = preset.get('name', '')
    tags = preset.get('tags', '')

    # 获取风格描述
    style_info = STYLE_DESCRIPTIONS.get(style, STYLE_DESCRIPTIONS["Synth"])
    intro = random.choice(style_info["intro"])
    color = random.choice(style_info["color"])
    use = random.choice(style_info["use"])

    # 获取乐器描述
    inst_desc = INSTRUMENT_DESCRIPTIONS.get(instrument, "采用合成技术，")

    # 获取情绪描述
    mood_info = MOOD_DESCRIPTIONS.get(mood, {"feel": "独特", "scene": "适合多种场景"})
    feel = mood_info["feel"]
    scene = mood_info["scene"]

    # 构建完整描述
    template = random.choice([
        f"「{name}」是一组{intro}，{inst_desc}音色{color}。整体听感{feel}，{scene}。{use}。",
        f"「{name}」{inst_desc}带来{color}的{style}音色。听感{feel}，{scene}。{use}。",
        f"「{name}」——{intro}。{inst_desc}音色特质{color}，呈现{feel}的听感体验。{scene}。{use}。",
    ])

    return template


# ---------------------------------------------------------------------------
# 增强评分算法 (P8-1)
# ---------------------------------------------------------------------------
def parse_params(params_str):
    """解析JSON参数，提取参数键和值"""
    if not params_str or params_str == '{}':
        return {}, 0
    try:
        params = json.loads(params_str)
        if isinstance(params, dict):
            return params, len(params)
        return {}, 0
    except (json.JSONDecodeError, TypeError):
        return {}, 0


def calc_param_diversity(params_dict):
    """计算参数多样性（参数键数量 + 值范围 + 嵌套深度）"""
    if not params_dict:
        return 0.0

    score = 0.0
    # 参数键数量
    key_count = len(params_dict)
    score += min(key_count * 2.0, 20.0)

    # 参数值多样性
    value_types = set()
    for key, value in params_dict.items():
        value_types.add(type(value).__name__)
        if isinstance(value, (int, float)):
            score += 0.5  # 数值参数加分
        elif isinstance(value, dict):
            score += 1.0  # 嵌套参数加分
        elif isinstance(value, list):
            score += 0.8  # 数组参数加分

    # 类型多样性
    score += len(value_types) * 1.5

    return score


def calc_timbre_uniqueness(preset, all_tags_counter):
    """计算音色独特性（基于标签稀有度和名称特征）"""
    score = 0.0
    name = preset.get('name', '')
    tags = preset.get('tags', '')

    # 名称长度合理性（2-24字符最优）
    name_len = len(name)
    if 4 <= name_len <= 24:
        score += 8.0
    elif 2 <= name_len <= 32:
        score += 5.0
    elif name_len > 0:
        score += 2.0

    # 名称单词数（2-5个单词最丰富）
    word_count = len(name.split())
    if 2 <= word_count <= 5:
        score += 6.0
    elif word_count > 0:
        score += 3.0

    # 标签稀有度（不常见的标签更有价值）
    if tags:
        tag_list = [t.strip().lower() for t in tags.replace(',', ' ').split()]
        for tag in tag_list:
            if tag and tag in all_tags_counter:
                freq = all_tags_counter[tag]
                if freq < 100:
                    score += 5.0  # 稀有标签
                elif freq < 1000:
                    score += 3.0  # 较少见标签
                elif freq < 10000:
                    score += 1.5  # 常见标签
                else:
                    score += 0.5  # 高频标签
        score += min(len(tag_list) * 1.5, 12.0)

    # 名称中包含特殊字符（数字、连字符等加分）
    if any(c.isdigit() for c in name):
        score += 2.0
    if '-' in name or '_' in name:
        score += 1.0

    return score


def calc_completeness(preset):
    """计算预设完整度"""
    score = 0.0
    fields = ['name', 'params', 'tags', 'category', 'original_description']
    filled = sum(1 for f in fields if preset.get(f, ''))
    score += filled * 3.0

    # 参数JSON非空
    params = preset.get('params', '')
    if params and params != '{}' and len(params) > 20:
        score += 10.0

    # 标签数量
    tags = preset.get('tags', '')
    if tags:
        tag_count = len(tags.split(','))
        score += min(tag_count * 2.0, 10.0)

    # 评分
    rating = preset.get('rating', 0)
    if isinstance(rating, (int, float)) and rating > 0:
        score += min(rating * 2.0, 10.0)

    # 有AI提示词加分
    if preset.get('ai_prompt', ''):
        score += 5.0

    # 有原始描述加分
    if preset.get('original_description', ''):
        score += 5.0

    return score


def score_preset(preset, all_tags_counter):
    """综合评分：参数多样性(30%) + 音色独特性(35%) + 完整度(25%) + 随机扰动(10%)"""
    params_dict, _ = parse_params(preset.get('params', ''))

    diversity = calc_param_diversity(params_dict)
    uniqueness = calc_timbre_uniqueness(preset, all_tags_counter)
    completeness = calc_completeness(preset)

    # 加权综合
    score = (
        diversity * 0.30 +
        uniqueness * 0.35 +
        completeness * 0.25 +
        random.uniform(0, 8.0)  # 随机扰动，确保每次运行结果略有不同
    )

    return score, {
        'diversity': diversity,
        'uniqueness': uniqueness,
        'completeness': completeness
    }


# ---------------------------------------------------------------------------
# 自动分类
# ---------------------------------------------------------------------------
def auto_classify(preset):
    """自动分类：根据名称和标签推断风格/乐器/情绪"""
    name = preset.get('name', '').lower()
    tags = preset.get('tags', '').lower()
    category = preset.get('category', '').lower()
    combined = name + " " + tags + " " + category

    styles = []
    instruments = []
    moods = []

    # 风格关键词
    style_keywords = {
        "Bass": ["bass", "sub", "low", "808", "deep bass", "低音", "贝斯"],
        "Lead": ["lead", "solo", "melody", "mono", "lead synth", "主音", "领奏"],
        "Pad": ["pad", "atmosphere", "ambient", "drone", "string", "铺底", "氛围"],
        "Pluck": ["pluck", "pizz", "harp", "guitar", "picked", "拨弦", "弹拨"],
        "Keys": ["key", "piano", "organ", "ep", "rhodes", "clav", "钢琴", "风琴"],
        "FX": ["fx", "sfx", "effect", "noise", "sweep", "riser", "特效", "音效"],
        "Synth": ["synth", "saw", "square", "pulse", "brass", "合成器"],
        "Arp": ["arp", "sequence", "pattern", "arpeggio", "琶音", "音序"],
        "Chord": ["chord", "stab", "hit", "ensemble", "和弦", "和声"],
        "Drums": ["drum", "kick", "snare", "hat", "perc", "beat", "鼓", "打击"]
    }

    for style, keywords in style_keywords.items():
        for kw in keywords:
            if kw in combined:
                styles.append(style)
                break

    # 乐器关键词
    inst_keywords = {
        "Analog": ["analog", "vintage", "classic", "moog", "roland", "模拟"],
        "FM": ["fm", "dx", "frequency modulation", "operator", "调频"],
        "Wavetable": ["wavetable", "wave", "table", "scan", "波表"],
        "Granular": ["granular", "grain", "particle", "texture", "cloud", "粒子"],
        "Additive": ["additive", "harmonic", "partial", "sine", "overtone", "加法"],
        "Subtractive": ["subtractive", "filter", "lpf", "resonance", "cutoff", "减法"],
        "Physical": ["physical", "model", "string", "reed", "bow", "resonator", "物理"],
        "Hybrid": ["hybrid", "layer", "combo", "mix", "blend", "混合"]
    }

    for inst, keywords in inst_keywords.items():
        for kw in keywords:
            if kw in combined:
                instruments.append(inst)
                break

    # 情绪关键词
    mood_keywords = {
        "Bright": ["bright", "shiny", "sparkle", "crystal", "clear", "luminous", "明亮"],
        "Dark": ["dark", "deep", "mysterious", "shadow", "night", "brooding", "黑暗"],
        "Warm": ["warm", "rich", "full", "smooth", "analog", "cozy", "温暖"],
        "Cold": ["cold", "icy", "metallic", "digital", "sterile", "frozen", "冰冷"],
        "Aggressive": ["aggressive", "hard", "distorted", "gritty", "harsh", "fierce", "激进"],
        "Soft": ["soft", "gentle", "mellow", "smooth", "calm", "delicate", "柔和"],
        "Evolving": ["evolving", "motion", "moving", "sweeping", "dynamic", "morph", "演变"],
        "Static": ["static", "steady", "constant", "fixed", "hold", "stable", "静态"],
        "Ethereal": ["ethereal", "dreamy", "floating", "airy", "heavenly", "celestial", "空灵"],
        "Punchy": ["punchy", "hit", "impact", "attack", "transient", "percussive", "冲击"]
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


# ---------------------------------------------------------------------------
# 主流程
# ---------------------------------------------------------------------------
def main():
    print("=" * 60)
    print("  LianCore V3 - 出厂预设精选与质量提升 (P8-1)")
    print("=" * 60)
    start_time = time.time()

    # 1. 连接源数据库
    if not os.path.exists(DB_PATH):
        print(f"\n[WARN] 预设库 {DB_PATH} 不存在，使用模拟数据生成")
        _generate_sample_data()
        return

    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    cursor = conn.cursor()

    # 2. 获取总预设数
    cursor.execute("SELECT COUNT(*) as cnt FROM presets")
    total_count = cursor.fetchone()['cnt']
    print(f"\n[INFO] 预设库总量: {total_count:,}")

    if total_count == 0:
        print("[WARN] 预设库为空，使用模拟数据")
        conn.close()
        _generate_sample_data()
        return

    # 3. 构建标签频率统计（用于音色独特性评分）
    print("[INFO] 构建标签频率统计...")
    all_tags_counter = Counter()
    cursor.execute("SELECT tags, category FROM presets")
    batch_count = 0
    for row in cursor:
        tags = row['tags'] or ''
        for tag in tags.replace(',', ' ').split():
            tag = tag.strip().lower()
            if tag:
                all_tags_counter[tag] += 1
        batch_count += 1
        if batch_count % 500000 == 0:
            print(f"  处理中... {batch_count:,} / {total_count:,}")
    print(f"  [OK] 唯一标签数: {len(all_tags_counter):,}")

    # 4. 分批评分和排序
    print("[INFO] 分批评分...")
    scored_presets = []
    cursor.execute(
        "SELECT name, json_data, tags, category, description, author, rating, ai_prompt "
        "FROM presets"
    )
    batch_count = 0

    for row in cursor:
        preset = {
            'name': row['name'] or f"Preset_{batch_count}",
            'params': row['json_data'] or '{}',
            'tags': row['tags'] or '',
            'category': row['category'] or '',
            'original_description': row['description'] or '',
            'author': row['author'] or '',
            'rating': row['rating'] or 0,
            'ai_prompt': row['ai_prompt'] or ''
        }
        score, details = score_preset(preset, all_tags_counter)
        preset['score'] = score
        preset['_details'] = details
        scored_presets.append(preset)

        batch_count += 1
        if batch_count % 100000 == 0:
            print(f"  评分进度: {batch_count:,} / {total_count:,} "
                  f"({batch_count*100//total_count}%)")

    print(f"  [OK] 评分完成: {batch_count:,} 预设")

    # 5. 按分数排序
    print("[INFO] 排序...")
    scored_presets.sort(key=lambda x: x['score'], reverse=True)

    # 6. 精选500个，确保分类多样性
    print("[INFO] 精选Top 500，确保分类多样性...")
    selected = []
    category_counts = Counter()
    # 目标：每个分类组合最多5个（10x8x10=800种组合，800x5=4000 > 500，所以每类最多1-2个）
    max_per_category = 2

    for preset in scored_presets:
        if len(selected) >= MAX_PRESETS:
            break
        style, instrument, mood = auto_classify(preset)
        cat_key = f"{style}|{instrument}|{mood}"

        if category_counts[cat_key] < max_per_category:
            preset['style'] = style
            preset['instrument'] = instrument
            preset['mood'] = mood
            preset['description'] = generate_chinese_description(preset)
            preset['quality'] = 'premium' if preset['score'] >= 50 else 'high'
            selected.append(preset)
            category_counts[cat_key] += 1

    # 如果不足500，放宽分类限制
    if len(selected) < MAX_PRESETS:
        max_per_category = 5
        for preset in scored_presets:
            if len(selected) >= MAX_PRESETS:
                break
            if preset in selected:
                continue
            style, instrument, mood = auto_classify(preset)
            cat_key = f"{style}|{instrument}|{mood}"
            if category_counts[cat_key] < max_per_category:
                preset['style'] = style
                preset['instrument'] = instrument
                preset['mood'] = mood
                preset['description'] = generate_chinese_description(preset)
                preset['quality'] = 'high'
                selected.append(preset)
                category_counts[cat_key] += 1

    # 最终补充
    if len(selected) < MAX_PRESETS:
        for preset in scored_presets:
            if len(selected) >= MAX_PRESETS:
                break
            if preset not in selected:
                style, instrument, mood = auto_classify(preset)
                preset['style'] = style
                preset['instrument'] = instrument
                preset['mood'] = mood
                preset['description'] = generate_chinese_description(preset)
                preset['quality'] = 'standard'
                selected.append(preset)

    print(f"  [OK] 精选预设数: {len(selected)}")

    # 7. 统计分析
    print("\n" + "=" * 40)
    print("  分类分布统计")
    print("=" * 40)

    style_counts = Counter(p['style'] for p in selected)
    inst_counts = Counter(p['instrument'] for p in selected)
    mood_counts = Counter(p['mood'] for p in selected)
    quality_counts = Counter(p.get('quality', 'high') for p in selected)

    print("\n--- 风格分布 ---")
    for style, count in style_counts.most_common():
        bar = "█" * (count // 2)
        print(f"  {style:<10} {count:>3} {bar}")

    print("\n--- 乐器分布 ---")
    for inst, count in inst_counts.most_common():
        bar = "█" * (count // 2)
        print(f"  {inst:<12} {count:>3} {bar}")

    print("\n--- 情绪分布 ---")
    for mood, count in mood_counts.most_common():
        bar = "█" * (count // 2)
        print(f"  {mood:<12} {count:>3} {bar}")

    print("\n--- 质量等级 ---")
    for q, count in quality_counts.most_common():
        print(f"  {q}: {count}")

    # 8. 保存到输出数据库
    print(f"\n[INFO] 保存到 {OUTPUT_DB}...")
    out_conn = sqlite3.connect(OUTPUT_DB)
    out_cursor = out_conn.cursor()

    out_cursor.execute('''
        CREATE TABLE IF NOT EXISTS factory_presets (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            json_data TEXT,
            tags TEXT,
            style TEXT NOT NULL,
            instrument TEXT NOT NULL,
            mood TEXT NOT NULL,
            score REAL DEFAULT 0.0,
            quality TEXT DEFAULT 'high',
            description TEXT,
            original_description TEXT,
            author TEXT,
            rating INTEGER DEFAULT 0,
            ai_prompt TEXT,
            selection_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    ''')
    # 创建索引加速查询
    out_cursor.execute('''
        CREATE INDEX IF NOT EXISTS idx_style ON factory_presets(style)
    ''')
    out_cursor.execute('''
        CREATE INDEX IF NOT EXISTS idx_mood ON factory_presets(mood)
    ''')
    out_cursor.execute('''
        CREATE INDEX IF NOT EXISTS idx_quality ON factory_presets(quality)
    ''')

    out_cursor.execute("DELETE FROM factory_presets")

    for i, preset in enumerate(selected):
        out_cursor.execute(
            """INSERT INTO factory_presets
               (name, json_data, tags, style, instrument, mood, score, quality,
                description, original_description, author, rating, ai_prompt)
               VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
            (preset['name'], preset.get('params', '{}'), preset.get('tags', ''),
             preset['style'], preset['instrument'], preset['mood'],
             preset['score'], preset.get('quality', 'high'),
             preset.get('description', ''),
             preset.get('original_description', ''),
             preset.get('author', ''),
             preset.get('rating', 0),
             preset.get('ai_prompt', ''))
        )

    out_conn.commit()
    out_conn.close()

    # 9. 生成JSON报告
    scores = [p['score'] for p in selected]
    report = {
        "title": "LianCore V3 出厂预设精选报告 (P8-1)",
        "version": "3.0.0",
        "total_presets": len(selected),
        "source_total": total_count,
        "selection_date": datetime.now().isoformat(),
        "style_distribution": {k: v for k, v in style_counts.most_common()},
        "instrument_distribution": {k: v for k, v in inst_counts.most_common()},
        "mood_distribution": {k: v for k, v in mood_counts.most_common()},
        "quality_distribution": {k: v for k, v in quality_counts.most_common()},
        "quality_metrics": {
            "avg_score": round(sum(scores) / len(scores), 2) if scores else 0,
            "max_score": round(max(scores), 2) if scores else 0,
            "min_score": round(min(scores), 2) if scores else 0,
            "median_score": round(sorted(scores)[len(scores)//2], 2) if scores else 0
        },
        "category_coverage": {
            "total_combinations": len(category_counts),
            "max_possible_combinations": (
                len(CATEGORIES["风格"]) *
                len(CATEGORIES["乐器"]) *
                len(CATEGORIES["情绪"])
            )
        },
        "execution_time_seconds": round(time.time() - start_time, 2)
    }

    with open(REPORT_FILE, 'w', encoding='utf-8') as f:
        json.dump(report, f, ensure_ascii=False, indent=2)

    # 10. 输出示例描述
    print("\n" + "=" * 40)
    print("  预设中文描述示例（前5个）")
    print("=" * 40)
    for i, preset in enumerate(selected[:5]):
        print(f"\n  [{i+1}] {preset['name']}")
        print(f"      风格: {preset['style']} | 乐器: {preset['instrument']} | 情绪: {preset['mood']}")
        print(f"      评分: {preset['score']:.1f} | 质量: {preset.get('quality', 'high')}")
        print(f"      描述: {preset.get('description', '')}")

    elapsed = time.time() - start_time
    print(f"\n{'=' * 60}")
    print(f"  完成! 耗时: {elapsed:.1f}秒")
    print(f"  精选: {len(selected)} 预设")
    print(f"  报告: {REPORT_FILE}")
    print(f"  数据库: {OUTPUT_DB}")
    print(f"{'=' * 60}")

    conn.close()


def _generate_sample_data():
    """生成模拟数据用于测试（当预设库不存在时）"""
    print("[INFO] 生成10万条模拟预设数据用于测试...")
    random.seed(42)

    style_names = {
        "Bass": ["Deep Sub", "Warm Bass", "808 Boom", "Growl Bass", "Reese Bass",
                 "FM Bass", "Wobble Bass", "Neuro Bass", "Acid Bass", "Pluck Bass"],
        "Lead": ["Saw Lead", "Square Lead", "Sync Lead", "Vocal Lead", "Trance Lead",
                 "Hard Lead", "Soft Lead", "Mono Lead", "Poly Lead", "Dirty Lead"],
        "Pad": ["Warm Pad", "Bright Pad", "Dark Pad", "Evolving Pad", "String Pad",
                "Vocal Pad", "Analog Pad", "Digital Pad", "Cinematic Pad", "Lush Pad"],
        "Pluck": ["Soft Pluck", "Hard Pluck", "FM Pluck", "Wood Pluck", "Metal Pluck",
                  "Glass Pluck", "Water Pluck", "Electric Pluck", "Deep Pluck", "Bright Pluck"],
        "Keys": ["Electric Piano", "Rhodes", "Wurlitzer", "Clavinet", "Organ",
                 "Harpsichord", "Celesta", "Toy Piano", "Digital Piano", "Honky Tonk"],
        "FX": ["Riser", "Downlifter", "Impact", "Sweep", "Noise Burst",
               "Laser", "Zap", "Whoosh", "Glitch", "Stutter"],
        "Synth": ["Classic Synth", "Retro Synth", "Modern Synth", "Poly Synth", "Mono Synth",
                  "Juno Style", "Prophet Style", "DX Style", "CS Style", "OB Style"],
        "Arp": ["Simple Arp", "Complex Arp", "Up Arp", "Down Arp", "Random Arp",
                "Octave Arp", "Chord Arp", "Melodic Arp", "Rhythmic Arp", "Gate Arp"],
        "Chord": ["Major Chord", "Minor Chord", "Jazz Chord", "Sus Chord", "Dim Chord",
                  "Aug Chord", "Seventh Chord", "Ninth Chord", "Power Chord", "Cluster"],
        "Drums": ["Kick", "Snare", "Hi-Hat", "Tom", "Cymbal",
                  "Clap", "Rim", "Perc", "808 Kit", "Acoustic Kit"]
    }

    instruments = ["Analog", "FM", "Wavetable", "Granular", "Additive",
                   "Subtractive", "Physical", "Hybrid"]
    moods = ["Bright", "Dark", "Warm", "Cold", "Aggressive",
             "Soft", "Evolving", "Static", "Ethereal", "Punchy"]

    # 创建模拟数据库
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS presets (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT,
            params TEXT,
            tags TEXT,
            category TEXT
        )
    ''')
    cursor.execute("DELETE FROM presets")

    for i in range(100000):
        style = random.choice(list(style_names.keys()))
        name = random.choice(style_names[style]) + f" {random.randint(1, 999)}"
        inst = random.choice(instruments)
        mood = random.choice(moods)

        # 生成模拟参数JSON
        params = {
            "osc_type": random.choice(["saw", "square", "sine", "triangle", "noise"]),
            "filter_cutoff": random.randint(100, 8000),
            "filter_resonance": round(random.uniform(0, 0.95), 2),
            "amp_env_attack": round(random.uniform(0, 2.0), 2),
            "amp_env_decay": round(random.uniform(0.1, 5.0), 2),
            "amp_env_sustain": round(random.uniform(0, 1.0), 2),
            "amp_env_release": round(random.uniform(0.1, 5.0), 2),
            "filter_env_amount": round(random.uniform(-1.0, 1.0), 2),
            "lfo_rate": round(random.uniform(0.1, 20.0), 2),
            "lfo_depth": round(random.uniform(0, 1.0), 2),
            "unison_voices": random.choice([1, 2, 3, 4, 5, 7, 8]),
            "unison_detune": round(random.uniform(0, 0.5), 2),
            "fx_reverb": round(random.uniform(0, 1.0), 2),
            "fx_delay": round(random.uniform(0, 1.0), 2),
            "fx_chorus": round(random.uniform(0, 1.0), 2),
        }
        params_json = json.dumps(params)

        # 标签
        tag_list = [style.lower(), inst.lower(), mood.lower()]
        if random.random() > 0.5:
            tag_list.append(random.choice(["vintage", "modern", "retro", "warm", "bright"]))
        if random.random() > 0.7:
            tag_list.append(random.choice(["analog", "digital", "hybrid", "experimental"]))
        tags_str = ",".join(tag_list)

        cursor.execute(
            "INSERT INTO presets (name, params, tags, category) VALUES (?, ?, ?, ?)",
            (name, params_json, tags_str, style)
        )

        if (i + 1) % 10000 == 0:
            conn.commit()
            print(f"  生成进度: {i+1:,} / 100,000")

    conn.commit()
    conn.close()
    print(f"  [OK] 模拟数据已生成: {DB_PATH}")

    # 重新运行主流程
    main()


if __name__ == "__main__":
    main()