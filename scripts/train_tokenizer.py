# =============================================================================
# LianCore Gamma - BPE Tokenizer 训练
# 从预设库提取文本，训练 SentencePiece BPE 模型 (词表 4096)
# 输出: models/tokenizer/tokenizer.model + tokenizer.vocab
# =============================================================================
import sentencepiece as spm
import sqlite3
import os
import json
import sys
import tempfile

def extract_texts_from_db(db_path: str, max_samples: int = 150000) -> list:
    """从预设数据库中提取所有文本描述"""
    texts = []
    if not os.path.exists(db_path):
        print(f"[WARN] Database not found: {db_path}")
        return texts

    print(f"[INFO] Reading from: {db_path}")
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()

    # 探测表结构
    cursor.execute("SELECT name FROM sqlite_master WHERE type='table'")
    tables = [r[0] for r in cursor.fetchall()]
    print(f"  Tables: {tables}")

    for table in tables:
        try:
            cursor.execute(f"SELECT * FROM {table} LIMIT 1")
            cols = [d[0] for d in cursor.description]

            # 收集中文文本列
            text_cols = []
            for col in cols:
                col_lower = col.lower()
                if col_lower in ('description', 'ai_prompt', 'name'):
                    text_cols.append(col)

            if text_cols:
                print(f"  Extracting from {table}: {text_cols}")
                col_str = ', '.join(text_cols)
                cursor.execute(f"SELECT {col_str} FROM {table} LIMIT {max_samples}")

                for row in cursor.fetchall():
                    for j, col in enumerate(text_cols):
                        if row[j]:
                            t = str(row[j]).strip().replace('\n', ' ').replace('\r', '')
                            if len(t) > 2:
                                texts.append(t)

                print(f"  Extracted {len(texts)} total texts")
                break
        except Exception as e:
            print(f"  Table '{table}' error: {e}")
            continue

    conn.close()

    # 如果没有从数据库读取到，使用合成数据
    if not texts:
        print("[WARN] No texts from database, using synthetic data...")
        texts = generate_synthetic_texts()

    return texts


def generate_synthetic_texts() -> list:
    """生成合成训练文本用于关键词规则"""
    keywords = [
        "温暖", "明亮", "暗", "柔和", "厚重", "轻盈", "尖锐",
        "紧张", "放松", "梦幻", "空灵", "复古", "现代", "电子",
        "经典", "管弦", "贝斯", "主音", "铺底", "快速", "慢速",
        "长音", "短促", "打击", "弹拨", "大厅", "房间", "环境",
        "低音", "高音", "噪声", "纯净", "失真", "合唱", "延迟",
        "混响", "压缩", "滤波器", "合成器", "钢琴", "吉他", "弦乐",
        "铜管", "木管", "人声", "琶音", "序列", "效果", "调制",
        "FM", "减法", "加法", "波表", "粒子", "物理建模",
    ]
    modifiers = ["的", "非常", "有点", "超级", "稍微", "极其", "略微"]
    types = ["音色", "音效", "声音", "合成", "乐器", "音源"]

    import random
    random.seed(42)
    texts = []
    for _ in range(20000):
        n = random.randint(1, 6)
        selected = random.sample(keywords, min(n, len(keywords)))
        text = "".join(selected)
        if random.random() < 0.3:
            text = modifiers[random.randint(0, len(modifiers)-1)] + text
        if random.random() < 0.3:
            text = text + types[random.randint(0, len(types)-1)]
        texts.append(text)
    return texts


def train_tokenizer(texts: list, output_dir: str):
    """训练 BPE tokenizer（使用临时目录避免中文路径编码问题）"""
    os.makedirs(output_dir, exist_ok=True)

    # 使用临时目录进行所有 I/O 操作，避免中文路径编码问题
    tmp_dir = tempfile.mkdtemp(prefix="liancore_tokenizer_")
    print(f"[INFO] Temp dir: {tmp_dir}")

    # 写入临时语料文件
    corpus_path = os.path.join(tmp_dir, "corpus.txt")
    with open(corpus_path, "w", encoding="utf-8") as f:
        for t in texts:
            f.write(t + "\n")

    print(f"[INFO] Corpus: {len(texts)} lines → {corpus_path}")

    # 模型输出到临时目录
    tmp_model_prefix = os.path.join(tmp_dir, "tokenizer")
    print(f"[INFO] Training...")

    spm.SentencePieceTrainer.Train(
        input=corpus_path,
        model_prefix=tmp_model_prefix,
        vocab_size=2500,
        character_coverage=0.9995,
        model_type="bpe",
        max_sentence_length=256,
        pad_id=0,
        unk_id=1,
        bos_id=2,
        eos_id=3,
        pad_piece="[PAD]",
        unk_piece="[UNK]",
        bos_piece="[BOS]",
        eos_piece="[EOS]",
        num_threads=4,
        input_sentence_size=100000,
        shuffle_input_sentence=True,
    )

    # 验证
    sp = spm.SentencePieceProcessor()
    tmp_model_path = tmp_model_prefix + ".model"
    sp.load(tmp_model_path)

    test_texts = [
        "温暖的合成器贝斯音色",
        "明亮的复古主音",
        "梦幻的电子铺底",
        "尖锐的失真音色",
        "厚重的打击乐器",
        "空灵的环境音效",
        "快速的弹拨合成器",
        "带一点点混响的现代贝斯",
    ]

    print(f"\n[INFO] Tokenizer verification:")
    print(f"  Vocab size: {sp.get_piece_size()}")
    print(f"  BOS ID: {sp.bos_id()}, EOS ID: {sp.eos_id()}, PAD ID: {sp.pad_id()}")
    print(f"\n  Sample encodings:")
    for tt in test_texts:
        ids = sp.encode(tt)
        tokens = sp.encode(tt, out_type=str)
        print(f"  '{tt}'")
        print(f"    IDs: {ids[:10]}... (len={len(ids)})")
        print(f"    Tokens: {tokens[:8]}...")

    # 复制模型文件到最终输出目录
    import shutil
    final_model = os.path.join(output_dir, "tokenizer.model")
    final_vocab = os.path.join(output_dir, "tokenizer.vocab")
    shutil.copy2(tmp_model_path, final_model)
    shutil.copy2(tmp_model_prefix + ".vocab", final_vocab)

    # 清理临时目录
    shutil.rmtree(tmp_dir, ignore_errors=True)

    print(f"\n[INFO] Done! Model saved to: {final_model}")
    print(f"[INFO] Vocab saved to: {final_vocab}")


if __name__ == "__main__":
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    db_path = os.path.join(project_root, "data", "preset_library.db")
    output_dir = os.path.join(project_root, "models", "tokenizer")

    texts = extract_texts_from_db(db_path)
    print(f"[INFO] Total texts: {len(texts)}")

    train_tokenizer(texts, output_dir)