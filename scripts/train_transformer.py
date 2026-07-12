# =============================================================================
# LianCore Gamma - 2层 Transformer 文本编码器训练 (V2: 对比学习增强)
# 修复: 余弦相似度过高问题 (0.997 → 目标 <0.9)
# 新增: InfoNCE 对比损失 + 余弦相似度正则化
# 架构: d_model=128, n_head=4, d_ff=256, 2 layers, ~1M params
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
import shutil
import random

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
BATCH_SIZE = 128       # 增大 batch 以支持对比学习
EPOCHS = 20            # 增加训练轮数
LEARNING_RATE = 1e-3
TEMPERATURE = 0.1      # 对比学习温度
CONTRASTIVE_WEIGHT = 0.5  # 对比损失权重
COSINE_PENALTY_WEIGHT = 0.3  # 余弦相似度惩罚权重

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

        self.output_proj = nn.Linear(d_model, d_model)
        # L2 归一化层 (用于对比学习)
        self.embed_norm = nn.LayerNorm(d_model)

    def forward(self, input_ids, attention_mask=None, normalize=False):
        x = self.token_embedding(input_ids) * np.sqrt(self.d_model)
        x = self.pos_encoding(x)

        if attention_mask is None:
            attention_mask = (input_ids == 0)

        x = self.transformer(x, src_key_padding_mask=attention_mask)

        if attention_mask is not None:
            mask_expanded = (~attention_mask).float().unsqueeze(-1)
            x = x * mask_expanded
            x = x.sum(dim=1) / mask_expanded.sum(dim=1).clamp(min=1e-9)
        else:
            x = x.mean(dim=1)

        x = self.output_proj(x)

        if normalize:
            x = self.embed_norm(x)
            x = F.normalize(x, p=2, dim=-1)

        return x


# =============================================================================
# 完整模型: Transformer + MLP
# =============================================================================
class LianCoreFullModel(nn.Module):
    def __init__(self):
        super().__init__()
        self.encoder = LianCoreTransformerEncoder()

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

    def forward(self, input_ids, attention_mask=None, return_embedding=False):
        embedding = self.encoder(input_ids, attention_mask, normalize=False)
        params = self.mlp(embedding)
        if return_embedding:
            return params, embedding
        return params


# =============================================================================
# InfoNCE 对比损失
# =============================================================================
def info_nce_loss(embeddings, temperature=0.1):
    """
    embeddings: (batch_size, d_model) - L2 normalized
    每个样本的正样本是自身的数据增强，这里简化：正样本对=(i, i)，负样本对=(i, j!=i)
    """
    batch_size = embeddings.size(0)
    device = embeddings.device

    # L2 归一化
    embeddings = F.normalize(embeddings, p=2, dim=-1)

    # 计算相似度矩阵 [B, B]
    sim_matrix = torch.matmul(embeddings, embeddings.T) / temperature

    # 对角线是正样本对 (同一文本的自身)
    # 对于小 batch，使用 identity 作为正样本标签
    labels = torch.arange(batch_size, device=device)

    # 计算 InfoNCE
    loss = F.cross_entropy(sim_matrix, labels)

    return loss


# =============================================================================
# 余弦多样性损失 - 惩罚高相似度
# =============================================================================
def cosine_diversity_loss(embeddings):
    """
    惩罚 embedding 之间的高余弦相似度
    目标: 平均余弦相似度 < 0.9
    """
    batch_size = embeddings.size(0)
    if batch_size <= 1:
        return torch.tensor(0.0, device=embeddings.device)

    embeddings = F.normalize(embeddings, p=2, dim=-1)

    # 计算所有对之间的余弦相似度
    sim_matrix = torch.matmul(embeddings, embeddings.T)

    # 去除对角线 (自身)
    mask = 1.0 - torch.eye(batch_size, device=embeddings.device)
    sim_matrix = sim_matrix * mask

    # 惩罚接近 1.0 的相似度
    # 使用 ReLU 只惩罚 sim > 0.85 的
    penalty = F.relu(sim_matrix - 0.85)
    loss = penalty.mean()

    return loss


# =============================================================================
# 数据加载
# =============================================================================
def load_tokenizer():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    model_path = os.path.normpath(os.path.join(script_dir, "..", "models", "tokenizer", "tokenizer.model"))
    tmp_dir = tempfile.mkdtemp(prefix='sp_')
    tmp_model = os.path.join(tmp_dir, 'tokenizer.model')
    shutil.copy2(model_path, tmp_model)
    sp = spm.SentencePieceProcessor()
    sp.load(tmp_model)
    return sp


def generate_diverse_synthetic_data(sp, num_samples=50000):
    """生成具有区分度的合成训练数据"""
    # 定义 25 种音色方向，参数差异更大
    sound_profiles = {
        "温暖": [0.3, 0.8, 0.2, 0.5, 0.4, 0.5, 0.7, 0.5, 0.3, 0.2, 0.3],
        "明亮": [0.8, 0.2, 0.3, 0.3, 0.7, 0.3, 0.6, 0.4, 0.8, 0.5, 0.5],
        "暗": [0.1, 0.3, 0.5, 0.7, 0.2, 0.7, 0.4, 0.7, 0.1, 0.2, 0.1],
        "柔和": [0.4, 0.7, 0.1, 0.2, 0.5, 0.6, 0.7, 0.7, 0.3, 0.1, 0.2],
        "厚重": [0.3, 0.5, 0.4, 0.9, 0.2, 0.4, 0.8, 0.6, 0.2, 0.6, 0.4],
        "尖锐": [0.8, 0.1, 0.8, 0.2, 0.9, 0.2, 0.3, 0.2, 0.9, 0.8, 0.6],
        "梦幻": [0.5, 0.5, 0.1, 0.2, 0.6, 0.8, 0.8, 0.9, 0.5, 0.4, 0.7],
        "空灵": [0.6, 0.3, 0.1, 0.1, 0.7, 0.8, 0.8, 0.9, 0.6, 0.2, 0.8],
        "复古": [0.3, 0.8, 0.3, 0.5, 0.3, 0.5, 0.6, 0.5, 0.2, 0.3, 0.3],
        "现代": [0.8, 0.3, 0.4, 0.5, 0.7, 0.3, 0.7, 0.3, 0.8, 0.6, 0.5],
        "贝斯": [0.2, 0.4, 0.4, 0.8, 0.2, 0.3, 0.8, 0.5, 0.1, 0.3, 0.2],
        "主音": [0.8, 0.4, 0.5, 0.4, 0.8, 0.3, 0.7, 0.3, 0.8, 0.6, 0.6],
        "铺底": [0.3, 0.6, 0.2, 0.7, 0.4, 0.8, 0.8, 0.8, 0.3, 0.2, 0.4],
        "打击": [0.7, 0.2, 0.7, 0.3, 0.95, 0.1, 0.2, 0.1, 0.6, 0.5, 0.2],
        "弹拨": [0.7, 0.4, 0.5, 0.2, 0.95, 0.2, 0.2, 0.2, 0.7, 0.5, 0.2],
        "失真": [0.4, 0.2, 0.9, 0.8, 0.8, 0.2, 0.4, 0.2, 0.7, 0.9, 0.7],
        "合唱": [0.5, 0.6, 0.2, 0.6, 0.4, 0.5, 0.7, 0.6, 0.5, 0.2, 0.8],
        "延迟": [0.4, 0.5, 0.3, 0.5, 0.4, 0.7, 0.5, 0.8, 0.5, 0.2, 0.7],
        "混响": [0.4, 0.5, 0.2, 0.3, 0.4, 0.8, 0.7, 0.9, 0.5, 0.2, 0.6],
        "钢琴": [0.6, 0.5, 0.2, 0.3, 0.8, 0.5, 0.5, 0.6, 0.6, 0.1, 0.2],
        "弦乐": [0.4, 0.7, 0.3, 0.6, 0.3, 0.6, 0.8, 0.7, 0.4, 0.2, 0.4],
        "铜管": [0.8, 0.6, 0.5, 0.7, 0.7, 0.3, 0.7, 0.3, 0.7, 0.5, 0.4],
        "噪音": [0.5, 0.1, 0.9, 0.8, 0.7, 0.1, 0.2, 0.1, 0.8, 0.9, 0.7],
        "快速": [0.7, 0.3, 0.6, 0.3, 0.95, 0.1, 0.4, 0.1, 0.7, 0.5, 0.4],
        "慢速": [0.3, 0.6, 0.1, 0.5, 0.1, 0.9, 0.8, 0.9, 0.2, 0.1, 0.2],
    }

    modifiers = ["的", "非常", "有点", "超级", "稍微", "极其", "略微"]
    types = ["音色", "音效", "声音", "合成", "乐器", "音源"]

    random.seed(42)
    np.random.seed(42)

    keys = list(sound_profiles.keys())
    data = []

    for i in range(num_samples):
        # 随机选择 1-3 个关键词
        n = random.randint(1, 3)
        selected = random.sample(keys, min(n, len(keys)))

        # 平均参数
        params = [0.0] * 11
        for kw in selected:
            profile = sound_profiles[kw]
            for j in range(11):
                params[j] += profile[j]
        params = [min(1.0, max(0.0, p / n + np.random.uniform(-0.03, 0.03))) for p in params]

        # 构建文本
        text = "".join(selected)
        if random.random() < 0.4:
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

    sp = load_tokenizer()
    print(f"[INFO] Tokenizer loaded, vocab_size={sp.get_piece_size()}")

    # 生成多样化合成数据
    data = generate_diverse_synthetic_data(sp, num_samples=50000)
    print(f"[INFO] Training samples: {len(data)}")

    model = LianCoreFullModel().to(device)
    total_params = sum(p.numel() for p in model.parameters())
    print(f"[INFO] Model parameters: {total_params:,}")

    optimizer = torch.optim.AdamW(model.parameters(), lr=LEARNING_RATE, weight_decay=1e-4)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=EPOCHS)
    mse_criterion = nn.MSELoss()

    model.train()
    for epoch in range(EPOCHS):
        total_loss = 0.0
        total_mse = 0.0
        total_contrast = 0.0
        total_diversity = 0.0
        np.random.shuffle(data)

        for i in range(0, len(data), BATCH_SIZE):
            batch = data[i:i+BATCH_SIZE]
            texts = [item[0] for item in batch]
            targets = torch.tensor([item[1] for item in batch], dtype=torch.float32).to(device)

            # Tokenize
            encoded = [sp.encode(t, out_type=int) for t in texts]
            max_batch_len = min(max(len(e) for e in encoded), MAX_LEN)
            input_ids = np.zeros((len(encoded), max_batch_len), dtype=np.int64)
            for j, e in enumerate(encoded):
                length = min(len(e), max_batch_len)
                input_ids[j, :length] = e[:length]

            input_ids = torch.from_numpy(input_ids).to(device)

            # Forward
            params_pred, embedding = model(input_ids, return_embedding=True)

            # 1. MSE 损失 (参数预测)
            mse_loss = mse_criterion(params_pred, targets)

            # 2. InfoNCE 对比损失
            contrast_loss = info_nce_loss(embedding, TEMPERATURE)

            # 3. 余弦多样性损失
            diversity_loss = cosine_diversity_loss(embedding)

            # 总损失
            loss = mse_loss + CONTRASTIVE_WEIGHT * contrast_loss + COSINE_PENALTY_WEIGHT * diversity_loss

            optimizer.zero_grad()
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            optimizer.step()

            total_loss += loss.item()
            total_mse += mse_loss.item()
            total_contrast += contrast_loss.item()
            total_diversity += diversity_loss.item()

        n_batches = len(data) // BATCH_SIZE + 1
        scheduler.step()
        print(f"  Epoch {epoch+1:2d}/{EPOCHS} | Loss: {total_loss/n_batches:.4f} "
              f"(MSE: {total_mse/n_batches:.4f}, Contrast: {total_contrast/n_batches:.4f}, "
              f"Diversity: {total_diversity/n_batches:.4f})")

    # =========================================================================
    # 导出 ONNX
    # =========================================================================
    print(f"\n[INFO] Exporting Transformer encoder to ONNX...")
    model.eval()
    encoder = model.encoder

    dummy_input = torch.zeros((1, MAX_LEN), dtype=torch.long).to(device)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.normpath(os.path.join(script_dir, ".."))
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

    # 余弦相似度验证
    test_texts = [
        "温暖的贝斯", "尖锐的失真音色", "梦幻的铺底",
        "厚重的打击乐器", "空灵的环境音效", "明亮的复古主音",
        "柔和的钢琴", "极具攻击性的金属", "湿润的合唱效果",
        "干的信号",
    ]
    print(f"\n[INFO] Cosine similarity verification:")
    with torch.no_grad():
        all_emb = []
        for text in test_texts:
            ids = sp.encode(text, out_type=int)[:MAX_LEN]
            input_tensor = torch.zeros((1, MAX_LEN), dtype=torch.long)
            input_tensor[0, :len(ids)] = torch.tensor(ids)
            input_tensor = input_tensor.to(device)
            params, emb = model(input_tensor, return_embedding=True)
            emb = F.normalize(emb, p=2, dim=-1)
            all_emb.append(emb.cpu().numpy()[0])
            params = params.cpu().numpy()[0]
            print(f"  '{text}': params=[{', '.join(f'{p:.3f}' for p in params)}]")

        # 计算余弦相似度
        all_emb = np.array(all_emb)
        sims = []
        for i in range(len(test_texts)):
            for j in range(i+1, len(test_texts)):
                s = np.dot(all_emb[i], all_emb[j]) / (np.linalg.norm(all_emb[i]) * np.linalg.norm(all_emb[j]) + 1e-9)
                sims.append(s)
        sims = np.array(sims)
        print(f"\n  Cosine similarity stats:")
        print(f"    Min: {sims.min():.4f}")
        print(f"    Max: {sims.max():.4f}")
        print(f"    Mean: {sims.mean():.4f}")
        print(f"    Std: {sims.std():.4f}")

        target = 0.9
        if sims.mean() < target:
            print(f"  [PASS] Mean cosine similarity < {target}")
        else:
            print(f"  [WARN] Mean cosine similarity >= {target}, need more training")

    # 保存权重
    weights_path = os.path.join(output_dir, "transformer_encoder.pt")
    torch.save(encoder.state_dict(), weights_path)
    print(f"\n[INFO] Weights saved to: {weights_path}")

    print(f"\n[INFO] ========================================")
    print(f"[INFO] Training complete!")
    print(f"[INFO]   Encoder ONNX: {onnx_path}")
    print(f"[INFO]   Total params: {total_params:,}")
    print(f"[INFO]   d_model: {D_MODEL}, n_head: {N_HEAD}")
    print(f"[INFO] ========================================")


if __name__ == "__main__":
    train()