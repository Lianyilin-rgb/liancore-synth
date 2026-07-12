# =============================================================================
# LianCore Gamma - 2层 Transformer 文本编码器训练
# 架构: d_model=128, n_head=4, d_ff=256, 2 layers, ~1M params
# 输入: BPE tokenized text (max_len=64)
# 输出: 128-dim embedding → 接入现有 MLP (128→64→32→11)
# 导出: models/transformer_encoder.onnx
# =============================================================================
import os
import sys
import sqlite3
import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
import sentencepiece as spm
import tempfile
import json

# =============================================================================
# 配置
# =============================================================================
VOCAB_SIZE = 2500
D_MODEL = 128
N_HEAD = 4
D_FF = 256
N_LAYERS = 2
MAX_LEN = 64
DROPOUT = 0.1
BATCH_SIZE = 64
EPOCHS = 10
LEARNING_RATE = 1e-3

# =============================================================================
# 位置编码
# =============================================================================
class PositionalEncoding(nn.Module):
    def __init__(self, d_model, max_len=128, dropout=0.1):
        super().__init__()
        self.dropout = nn.Dropout(p=dropout)
        pe = torch.zeros(max_len, d_model)
        position = torch.arange(0, max_len, dtype=torch.float).unsqueeze(1)
        div_term = torch.exp(torch.arange(0, d_model, 2).float() * (-np.log(10000.0) / d_model))
        pe[:, 0::2] = torch.sin(position * div_term)
        pe[:, 1::2] = torch.cos(position * div_term)
        self.register_buffer('pe', pe.unsqueeze(0))

    def forward(self, x):
        # x: (batch, seq_len, d_model)
        x = x + self.pe[:, :x.size(1), :]
        return self.dropout(x)


# =============================================================================
# 2层 Transformer 编码器
# =============================================================================
class LianCoreTransformerEncoder(nn.Module):
    def __init__(self, vocab_size=VOCAB_SIZE, d_model=D_MODEL, n_head=N_HEAD,
                 d_ff=D_FF, n_layers=N_LAYERS, max_len=MAX_LEN, dropout=DROPOUT):
        super().__init__()
        self.d_model = d_model
        self.max_len = max_len

        self.token_embedding = nn.Embedding(vocab_size, d_model, padding_idx=0)
        self.pos_encoding = PositionalEncoding(d_model, max_len, dropout)

        encoder_layer = nn.TransformerEncoderLayer(
            d_model=d_model, nhead=n_head, dim_feedforward=d_ff,
            dropout=dropout, activation='gelu', batch_first=True
        )
        self.transformer = nn.TransformerEncoder(encoder_layer, num_layers=n_layers)

        # 输出投影: 平均池化 → 128-dim embedding
        self.output_proj = nn.Linear(d_model, d_model)

    def forward(self, input_ids, attention_mask=None):
        # input_ids: (batch, seq_len)
        x = self.token_embedding(input_ids) * np.sqrt(self.d_model)
        x = self.pos_encoding(x)

        # 创建 attention mask (True = 忽略)
        if attention_mask is None:
            attention_mask = (input_ids == 0)  # padding_idx=0

        x = self.transformer(x, src_key_padding_mask=attention_mask)

        # 平均池化 (忽略 padding)
        if attention_mask is not None:
            mask_expanded = (~attention_mask).float().unsqueeze(-1)  # (B, L, 1)
            x = x * mask_expanded
            x = x.sum(dim=1) / mask_expanded.sum(dim=1).clamp(min=1e-9)
        else:
            x = x.mean(dim=1)

        x = self.output_proj(x)
        return x  # (batch, 128)


# =============================================================================
# 完整模型: Transformer + MLP
# =============================================================================
class LianCoreFullModel(nn.Module):
    def __init__(self):
        super().__init__()
        self.encoder = LianCoreTransformerEncoder()

        # MLP head: 128 → 64 → 32 → 11
        self.mlp = nn.Sequential(
            nn.Linear(128, 64),
            nn.GELU(),
            nn.Dropout(0.1),
            nn.Linear(64, 32),
            nn.GELU(),
            nn.Dropout(0.1),
            nn.Linear(32, 11),
            nn.Sigmoid()
        )

    def forward(self, input_ids, attention_mask=None):
        embedding = self.encoder(input_ids, attention_mask)
        params = self.mlp(embedding)
        return params, embedding


# =============================================================================
# 数据加载
# =============================================================================
def load_tokenizer():
    """加载 BPE tokenizer（复制到临时目录避免中文路径问题）"""
    import shutil
    script_dir = os.path.dirname(os.path.abspath(__file__))
    model_path = os.path.normpath(os.path.join(script_dir, "..", "models", "tokenizer", "tokenizer.model"))

    # 复制到临时目录
    tmp_dir = tempfile.mkdtemp(prefix="liancore_sp_")
    tmp_model = os.path.join(tmp_dir, "tokenizer.model")
    shutil.copy2(model_path, tmp_model)

    sp = spm.SentencePieceProcessor()
    sp.load(tmp_model)

    # 注意: 不清理临时目录，因为 SentencePiece 可能延迟加载模型
    return sp


def load_training_data(db_path, sp, max_samples=50000):
    """从数据库加载训练数据: (文本, 11参数)"""
    if not os.path.exists(db_path):
        print(f"[WARN] Database not found: {db_path}, using synthetic data")
        return generate_synthetic_data(sp, max_samples)

    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()

    # 检查 json_data 列
    cursor.execute("SELECT * FROM presets LIMIT 1")
    cols = [d[0] for d in cursor.description]
    print(f"  Columns: {cols}")

    has_json = 'json_data' in cols
    has_name = 'name' in cols
    has_desc = 'description' in cols

    data = []
    cursor.execute(f"SELECT * FROM presets LIMIT {max_samples}")
    for row in cursor.fetchall():
        row_dict = {cols[i]: row[i] for i in range(len(cols))}

        # 文本
        text = row_dict.get('ai_prompt', '') or row_dict.get('description', '') or row_dict.get('name', '')
        if not text or len(str(text).strip()) < 3:
            continue

        # 参数 (11个目标值)
        if has_json and row_dict.get('json_data'):
            try:
                jd = json.loads(row_dict['json_data'])
                params = [
                    jd.get('brightness', 0.5),
                    jd.get('warmth', 0.5),
                    jd.get('tension', 0.5),
                    jd.get('density', 0.5),
                    jd.get('attack', 0.5),
                    jd.get('decay', 0.5),
                    jd.get('sustain', 0.7),
                    jd.get('release', 0.5),
                    jd.get('filter_cutoff', 0.5),
                    jd.get('resonance', 0.3),
                    jd.get('mod_depth', 0.3),
                ]
            except:
                params = [0.5] * 11
        else:
            params = [0.5] * 11

        data.append((str(text).strip(), params))

    conn.close()

    if len(data) < 100:
        print(f"[WARN] Only {len(data)} samples, using synthetic data")
        return generate_synthetic_data(sp, max_samples)

    print(f"[INFO] Loaded {len(data)} training samples")
    return data


def generate_synthetic_data(sp, num_samples):
    """生成合成训练数据"""
    keywords = [
        ("温暖", [0.3, 0.8, 0.2, 0.5, 0.4, 0.5, 0.7, 0.5, 0.3, 0.2, 0.3]),
        ("明亮", [0.8, 0.3, 0.3, 0.4, 0.6, 0.4, 0.7, 0.5, 0.7, 0.4, 0.4]),
        ("暗", [0.2, 0.4, 0.5, 0.6, 0.3, 0.6, 0.5, 0.6, 0.2, 0.3, 0.2]),
        ("柔和", [0.5, 0.7, 0.2, 0.3, 0.5, 0.5, 0.7, 0.6, 0.4, 0.2, 0.2]),
        ("厚重", [0.4, 0.6, 0.4, 0.8, 0.3, 0.5, 0.8, 0.6, 0.3, 0.5, 0.4]),
        ("尖锐", [0.7, 0.2, 0.7, 0.3, 0.8, 0.3, 0.4, 0.3, 0.8, 0.7, 0.5]),
        ("梦幻", [0.5, 0.5, 0.1, 0.3, 0.6, 0.7, 0.8, 0.8, 0.5, 0.4, 0.6]),
        ("空灵", [0.6, 0.4, 0.1, 0.2, 0.7, 0.8, 0.8, 0.9, 0.6, 0.3, 0.7]),
        ("复古", [0.4, 0.7, 0.3, 0.5, 0.4, 0.5, 0.6, 0.5, 0.3, 0.3, 0.3]),
        ("现代", [0.7, 0.4, 0.4, 0.5, 0.6, 0.4, 0.7, 0.4, 0.7, 0.5, 0.5]),
        ("贝斯", [0.3, 0.5, 0.4, 0.7, 0.3, 0.4, 0.8, 0.5, 0.2, 0.3, 0.3]),
        ("主音", [0.7, 0.5, 0.5, 0.5, 0.7, 0.4, 0.7, 0.4, 0.7, 0.5, 0.5]),
        ("铺底", [0.4, 0.6, 0.2, 0.6, 0.5, 0.7, 0.8, 0.7, 0.4, 0.3, 0.4]),
        ("打击", [0.6, 0.3, 0.6, 0.4, 0.9, 0.2, 0.3, 0.2, 0.5, 0.4, 0.3]),
        ("弹拨", [0.6, 0.5, 0.4, 0.3, 0.9, 0.3, 0.3, 0.3, 0.6, 0.4, 0.3]),
        ("失真", [0.5, 0.3, 0.8, 0.7, 0.7, 0.3, 0.5, 0.3, 0.6, 0.8, 0.6]),
        ("合唱", [0.5, 0.6, 0.2, 0.6, 0.5, 0.5, 0.7, 0.6, 0.5, 0.3, 0.7]),
        ("延迟", [0.5, 0.5, 0.3, 0.5, 0.5, 0.6, 0.6, 0.7, 0.5, 0.3, 0.6]),
        ("混响", [0.5, 0.5, 0.2, 0.4, 0.5, 0.7, 0.7, 0.8, 0.5, 0.3, 0.5]),
        ("钢琴", [0.6, 0.5, 0.3, 0.4, 0.7, 0.5, 0.5, 0.6, 0.6, 0.2, 0.3]),
        ("弦乐", [0.5, 0.7, 0.3, 0.6, 0.4, 0.6, 0.8, 0.6, 0.5, 0.3, 0.4]),
        ("铜管", [0.7, 0.6, 0.5, 0.7, 0.6, 0.4, 0.7, 0.4, 0.6, 0.4, 0.4]),
        ("合成器", [0.6, 0.4, 0.4, 0.5, 0.5, 0.5, 0.6, 0.5, 0.6, 0.5, 0.5]),
        ("快速", [0.6, 0.4, 0.5, 0.4, 0.9, 0.2, 0.5, 0.2, 0.6, 0.4, 0.4]),
        ("慢速", [0.4, 0.6, 0.2, 0.5, 0.2, 0.8, 0.8, 0.8, 0.3, 0.2, 0.3]),
    ]

    modifiers = ["的", "非常", "有点", "超级", "稍微", "极其", "略微"]
    types = ["音色", "音效", "声音", "合成", "乐器", "音源"]

    import random
    random.seed(42)
    data = []
    for i in range(num_samples):
        n = random.randint(1, 4)
        selected = random.sample(keywords, min(n, len(keywords)))
        kw_names = [s[0] for s in selected]
        params = [0.0] * 11
        for s in selected:
            for j in range(11):
                params[j] += s[1][j]
        params = [min(1.0, max(0.0, p / n + random.uniform(-0.05, 0.05))) for p in params]

        text = "".join(kw_names)
        if random.random() < 0.3:
            text = modifiers[random.randint(0, len(modifiers)-1)] + text
        if random.random() < 0.3:
            text = text + types[random.randint(0, len(types)-1)]
        data.append((text, params))

    return data


# =============================================================================
# 训练循环
# =============================================================================
def train():
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"[INFO] Device: {device}")

    # 加载Tokenizer
    sp = load_tokenizer()
    print(f"[INFO] Tokenizer loaded, vocab_size={sp.get_piece_size()}")

    # 加载数据
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.normpath(os.path.join(script_dir, ".."))
    db_path = os.path.join(project_root, "data", "preset_library.db")
    data = load_training_data(db_path, sp, max_samples=50000)
    print(f"[INFO] Training samples: {len(data)}")

    # 创建模型
    model = LianCoreFullModel().to(device)
    total_params = sum(p.numel() for p in model.parameters())
    print(f"[INFO] Model parameters: {total_params:,}")

    optimizer = torch.optim.AdamW(model.parameters(), lr=LEARNING_RATE, weight_decay=1e-4)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=EPOCHS)
    criterion = nn.MSELoss()

    # 训练循环
    model.train()
    for epoch in range(EPOCHS):
        total_loss = 0.0
        np.random.shuffle(data)

        for i in range(0, len(data), BATCH_SIZE):
            batch = data[i:i+BATCH_SIZE]
            texts = [item[0] for item in batch]
            targets = torch.tensor([item[1] for item in batch], dtype=torch.float32).to(device)

            # Tokenize
            encoded = [sp.encode(t, out_type=int) for t in texts]
            # Pad to max_len
            max_batch_len = min(max(len(e) for e in encoded), MAX_LEN)
            input_ids = np.zeros((len(encoded), max_batch_len), dtype=np.int64)
            for j, e in enumerate(encoded):
                length = min(len(e), max_batch_len)
                input_ids[j, :length] = e[:length]

            input_ids = torch.from_numpy(input_ids).to(device)

            # Forward
            params_pred, _ = model(input_ids)
            loss = criterion(params_pred, targets)

            # Backward
            optimizer.zero_grad()
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            optimizer.step()

            total_loss += loss.item()

        avg_loss = total_loss / (len(data) // BATCH_SIZE + 1)
        scheduler.step()
        print(f"  Epoch {epoch+1}/{EPOCHS}, Loss: {avg_loss:.6f}")

    # =========================================================================
    # 导出 ONNX (仅编码器部分)
    # =========================================================================
    print(f"\n[INFO] Exporting Transformer encoder to ONNX...")
    model.eval()
    encoder = model.encoder

    # 导出编码器
    dummy_input = torch.zeros((1, MAX_LEN), dtype=torch.long).to(device)
    output_dir = os.path.join(project_root, "models")
    os.makedirs(output_dir, exist_ok=True)
    onnx_path = os.path.join(output_dir, "transformer_encoder.onnx")

    torch.onnx.export(
        encoder,
        dummy_input,
        onnx_path,
        input_names=["input_ids"],
        output_names=["embedding"],
        dynamic_axes={
            "input_ids": {0: "batch", 1: "sequence"},
            "embedding": {0: "batch"}
        },
        opset_version=14,
        do_constant_folding=True,
    )
    print(f"[INFO] Encoder exported to: {onnx_path}")

    # 验证 ONNX
    import onnx
    onnx_model = onnx.load(onnx_path)
    onnx.checker.check_model(onnx_model)
    print(f"[INFO] ONNX model verified OK")

    # 验证推理
    test_texts = [
        "温暖的合成器贝斯音色",
        "明亮的复古主音",
        "梦幻的电子铺底",
        "尖锐的失真音色",
    ]
    print(f"\n[INFO] Inference verification:")
    with torch.no_grad():
        for text in test_texts:
            ids = sp.encode(text, out_type=int)[:MAX_LEN]
            input_tensor = torch.zeros((1, MAX_LEN), dtype=torch.long)
            input_tensor[0, :len(ids)] = torch.tensor(ids)
            input_tensor = input_tensor.to(device)
            params, emb = model(input_tensor)
            params = params.cpu().numpy()[0]
            print(f"  '{text}'")
            print(f"    params: [{', '.join(f'{p:.3f}' for p in params)}]")
            print(f"    embedding norm: {emb.cpu().numpy()[0].std():.4f}")

    # 保存模型权重
    weights_path = os.path.join(output_dir, "transformer_encoder.pt")
    torch.save(encoder.state_dict(), weights_path)
    print(f"\n[INFO] Weights saved to: {weights_path}")

    # 模型信息
    print(f"\n[INFO] ========================================")
    print(f"[INFO] Training complete!")
    print(f"[INFO]   Encoder ONNX: {onnx_path}")
    print(f"[INFO]   Encoder weights: {weights_path}")
    print(f"[INFO]   Total params: {total_params:,}")
    print(f"[INFO]   d_model: {D_MODEL}, n_head: {N_HEAD}")
    print(f"[INFO]   d_ff: {D_FF}, n_layers: {N_LAYERS}")
    print(f"[INFO] ========================================")


if __name__ == "__main__":
    train()