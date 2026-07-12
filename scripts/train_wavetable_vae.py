# =============================================================================
# LianCore - 波表生成 VAE 训练脚本
# Gamma Week 5-6: 文本到波表合成
# 架构: 轻量条件VAE (编码器 ~300K + 解码器 ~150K)
# 输出: 256帧 × 2048采样 波表
# =============================================================================

import torch
import torch.nn as nn
import torch.nn.functional as F
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
import numpy as np
import os
import sys
from time import time

# 配置
BATCH_SIZE = 16
EPOCHS = 20
LEARNING_RATE = 1e-3
WEIGHT_DECAY = 1e-5
LATENT_DIM = 64
TEXT_DIM = 128
WT_FRAMES = 256
WT_SIZE = 2048
MODEL_DIR = "models"
DEVICE = torch.device("cuda" if torch.cuda.is_available() else "cpu")

# =============================================================================
# 合成波表数据生成
# =============================================================================

# 预计算谐波正弦波 (全局缓存)
_MAX_HARMONICS = 21
_HARMONIC_RANGE = np.arange(1, _MAX_HARMONICS + 1, dtype=np.float32).reshape(-1, 1)  # (21, 1)

def generate_synthetic_wavetable(wave_type, num_frames=WT_FRAMES, table_size=WT_SIZE, t=None):
    """向量化合成波表生成 - 所有帧同时计算"""
    if t is None:
        t = np.linspace(0, 1, table_size, endpoint=False).astype(np.float32)
    progress = np.linspace(0, 1, num_frames, dtype=np.float32).reshape(-1, 1)  # (256, 1)
    h = _HARMONIC_RANGE.reshape(1, -1)  # (1, 21)
    all_sines = np.sin(2 * np.pi * _HARMONIC_RANGE * t)  # (21, 2048)

    if wave_type == "sine":
        num_h = (1 + progress * 15).astype(np.int32)  # (256, 1)
        mask = (h <= num_h).astype(np.float32)  # (256, 21)
        weights = mask / (h + 1e-8)  # 1/h per active harmonic
        wave = weights @ all_sines  # (256, 21) @ (21, 2048) = (256, 2048)

    elif wave_type == "saw":
        num_h = (1 + progress * 20).astype(np.int32)
        mask = (h <= num_h).astype(np.float32)
        weights = mask / (h + 1e-8)
        wave = weights @ all_sines

    elif wave_type == "square":
        num_h = (1 + progress * 10) * 2  # odd harmonics only
        is_odd = ((h.astype(int) % 2) == 1).astype(np.float32)  # (1, 21)
        mask = ((h <= num_h).astype(np.float32)) * is_odd
        weights = mask / (h + 1e-8)
        wave = weights @ all_sines

    elif wave_type == "triangle":
        num_h = (1 + progress * 8) * 2
        is_odd = ((h.astype(int) % 2) == 1).astype(np.float32)
        mask = ((h <= num_h).astype(np.float32)) * is_odd
        sign = (-1.0) ** ((h.astype(int) - 1) // 2)  # (1, 21)
        weights = mask * sign / (h * h + 1e-8)
        wave = weights @ all_sines

    elif wave_type == "mix":
        # saw branch (harmonics 1-7)
        saw_mask = (h <= 7).astype(np.float32)
        saw_weights = saw_mask / (h + 1e-8)
        saw = saw_weights @ all_sines  # (1, 2048) broadcast
        # square branch (odd harmonics 1-7)
        sq_mask = saw_mask * ((h.astype(int) % 2) == 1).astype(np.float32)
        sq_weights = sq_mask / (h + 1e-8)
        sq = sq_weights @ all_sines
        wave = (1 - progress) * saw + progress * sq  # (256, 2048)

    elif wave_type == "pulse":
        pulse_width = 0.1 + progress * 0.4  # (256, 1)
        wave = np.where(np.fmod(t.reshape(1, -1), 1.0) < pulse_width, 1.0, -1.0).astype(np.float32)

    elif wave_type == "noise":
        noise = np.random.randn(num_frames, table_size).astype(np.float32)
        amp = 0.1 + progress * 0.5  # (256, 1)
        env = np.sin(2 * np.pi * t).reshape(1, -1)  # (1, 2048)
        wave = noise * amp * env

    else:
        wave = np.tile(np.sin(2 * np.pi * t).reshape(1, -1), (num_frames, 1)).astype(np.float32)

    # 失真 (向量化)
    drive = 1.0 + progress * 2.0  # (256, 1)
    wave = np.tanh(wave * drive)
    max_abs = np.max(np.abs(wave), axis=1, keepdims=True) + 1e-8
    wave = wave / max_abs

    return wave.astype(np.float32)


def generate_training_data(num_samples=2000):
    """向量化生成训练数据集"""
    wave_types = ["sine", "saw", "square", "triangle", "mix", "pulse", "noise"]
    n_types = len(wave_types)

    # 预分配数组
    all_wt = np.empty((num_samples, WT_FRAMES, WT_SIZE), dtype=np.float32)
    all_text_emb = np.empty((num_samples, TEXT_DIM), dtype=np.float32)

    # 预计算 t 向量 (所有波表共用)
    t = np.linspace(0, 1, WT_SIZE, endpoint=False).astype(np.float32)

    # 按类型批量生成 (每种类型约 num_samples/7 个)
    for type_idx, wt_type in enumerate(wave_types):
        count = num_samples // n_types + (1 if type_idx < num_samples % n_types else 0)
        if count == 0:
            continue
        start_idx = type_idx * (num_samples // n_types) + min(type_idx, num_samples % n_types)
        end_idx = start_idx + count

        print(f"  Generating {count} {wt_type} wavetables...")
        for i in range(start_idx, end_idx):
            all_wt[i] = generate_synthetic_wavetable(wt_type, WT_FRAMES, WT_SIZE, t)

            # 伪文本嵌入
            emb = np.random.randn(TEXT_DIM).astype(np.float32)
            emb[type_idx] += 2.0
            emb = emb / (np.linalg.norm(emb) + 1e-8)
            all_text_emb[i] = emb

    return all_wt, all_text_emb


class WavetableDataset(Dataset):
    def __init__(self, wavetables, text_embs):
        self.wt = torch.from_numpy(wavetables).float()
        self.text = torch.from_numpy(text_embs).float()

    def __len__(self):
        return len(self.wt)

    def __getitem__(self, idx):
        return self.wt[idx], self.text[idx]


# =============================================================================
# VAE 模型
# =============================================================================

class WavetableEncoder(nn.Module):
    """编码器: 256×2048 → μ/logvar [64] (~180K params)"""
    def __init__(self, latent_dim=LATENT_DIM):
        super().__init__()
        self.conv = nn.Sequential(
            nn.Conv1d(256, 64, kernel_size=7, stride=4, padding=3),
            nn.BatchNorm1d(64), nn.ReLU(),
            nn.Conv1d(64, 32, kernel_size=5, stride=4, padding=2),
            nn.BatchNorm1d(32), nn.ReLU(),
            nn.Conv1d(32, 16, kernel_size=3, stride=2, padding=1),
            nn.BatchNorm1d(16), nn.ReLU(),
            nn.AdaptiveAvgPool1d(8),
        )
        self.fc = nn.Sequential(
            nn.Flatten(),
            nn.Linear(128, 128), nn.ReLU(),
        )
        self.mu = nn.Linear(128, latent_dim)
        self.logvar = nn.Linear(128, latent_dim)

    def forward(self, x):
        h = self.conv(x)
        h = self.fc(h)
        mu = self.mu(h)
        logvar = self.logvar(h)
        return mu, logvar


class WavetableDecoder(nn.Module):
    """解码器: z [64] + text [128] → 256×2048 (~350K params)"""
    def __init__(self, latent_dim=LATENT_DIM, text_dim=TEXT_DIM):
        super().__init__()
        input_dim = latent_dim + text_dim
        self.fc = nn.Sequential(
            nn.Linear(input_dim, 256), nn.ReLU(),
            nn.Linear(256, 256), nn.ReLU(),
            nn.Linear(256, 16 * 32),  # 16 channels × 32 time
        )
        self.refine = nn.Sequential(
            nn.Upsample(scale_factor=64, mode='linear'),
            nn.Conv1d(16, 32, kernel_size=5, padding=2),
            nn.BatchNorm1d(32), nn.ReLU(),
            nn.Conv1d(32, 64, kernel_size=5, padding=2),
            nn.BatchNorm1d(64), nn.ReLU(),
            nn.Conv1d(64, 256, kernel_size=5, padding=2),
            nn.Tanh(),
        )

    def forward(self, z, text_emb):
        x = torch.cat([z, text_emb], dim=-1)
        x = self.fc(x)
        x = x.view(-1, 16, 32)
        x = self.refine(x)
        return x


class WavetableVAE(nn.Module):
    def __init__(self, latent_dim=LATENT_DIM, text_dim=TEXT_DIM):
        super().__init__()
        self.encoder = WavetableEncoder(latent_dim)
        self.decoder = WavetableDecoder(latent_dim, text_dim)
        self.latent_dim = latent_dim

    def reparameterize(self, mu, logvar):
        std = torch.exp(0.5 * logvar)
        eps = torch.randn_like(std)
        return mu + eps * std

    def forward(self, x, text_emb):
        mu, logvar = self.encoder(x)
        z = self.reparameterize(mu, logvar)
        recon = self.decoder(z, text_emb)
        return recon, mu, logvar

    @torch.no_grad()
    def generate(self, text_emb):
        z = torch.randn(text_emb.size(0), self.latent_dim, device=text_emb.device)
        return self.decoder(z, text_emb)


# =============================================================================
# 损失函数
# =============================================================================

def multi_scale_stft_loss(pred, target, scales=[256, 512, 1024]):
    """多尺度 STFT 损失"""
    total = 0.0
    pred_flat = pred.reshape(-1, WT_SIZE)
    target_flat = target.reshape(-1, WT_SIZE)

    for n_fft in scales:
        pred_stft = torch.stft(pred_flat, n_fft=n_fft, hop_length=n_fft // 4,
                               return_complex=True, window=torch.hann_window(n_fft, device=pred.device))
        target_stft = torch.stft(target_flat, n_fft=n_fft, hop_length=n_fft // 4,
                                 return_complex=True, window=torch.hann_window(n_fft, device=pred.device))

        mag_loss = F.l1_loss(pred_stft.abs(), target_stft.abs())
        log_loss = F.l1_loss(
            torch.log(pred_stft.abs() + 1e-8),
            torch.log(target_stft.abs() + 1e-8),
        )
        total += mag_loss + log_loss

    return total / len(scales)


def vae_loss(recon, target, mu, logvar, beta=0.001):
    """VAE 总损失"""
    # 多尺度 STFT 重建
    recon_loss = multi_scale_stft_loss(recon, target)

    # KL 散度
    kl_loss = -0.5 * torch.sum(1 + logvar - mu.pow(2) - logvar.exp()) / mu.size(0)

    # 帧间平滑损失
    smooth_loss = F.l1_loss(recon[:, 1:, :], recon[:, :-1, :])

    total = recon_loss + beta * kl_loss + 0.01 * smooth_loss
    return total, {"recon": recon_loss.item(), "kl": kl_loss.item(), "smooth": smooth_loss.item()}


# =============================================================================
# 训练
# =============================================================================

def train():
    os.makedirs(MODEL_DIR, exist_ok=True)

    # 生成数据
    print("Generating synthetic wavetable data...", flush=True)
    t0 = time()
    wt_data, text_data = generate_training_data(2000)
    print(f"  Data generation took {time() - t0:.1f}s", flush=True)
    split = int(len(wt_data) * 0.9)
    train_ds = WavetableDataset(wt_data[:split], text_data[:split])
    val_ds = WavetableDataset(wt_data[split:], text_data[split:])
    train_loader = DataLoader(train_ds, batch_size=BATCH_SIZE, shuffle=True)
    val_loader = DataLoader(val_ds, batch_size=BATCH_SIZE, shuffle=False)
    print(f"Train: {len(train_ds)}, Val: {len(val_ds)}, Device: {DEVICE}", flush=True)

    # 初始化
    model = WavetableVAE().to(DEVICE)
    total_params = sum(p.numel() for p in model.parameters())
    encoder_params = sum(p.numel() for p in model.encoder.parameters())
    decoder_params = sum(p.numel() for p in model.decoder.parameters())
    print(f"Params: total={total_params/1000:.1f}K, enc={encoder_params/1000:.1f}K, dec={decoder_params/1000:.1f}K")

    optimizer = optim.Adam(model.parameters(), lr=LEARNING_RATE, weight_decay=WEIGHT_DECAY)
    scheduler = optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=EPOCHS)

    best_val_loss = float("inf")

    for epoch in range(EPOCHS):
        # 训练
        model.train()
        train_loss = 0.0
        for wt, text in train_loader:
            wt = wt.to(DEVICE)
            text = text.to(DEVICE)

            optimizer.zero_grad()
            recon, mu, logvar = model(wt, text)
            loss, losses = vae_loss(recon, wt, mu, logvar)
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            optimizer.step()
            train_loss += loss.item()

        train_loss /= len(train_loader)

        # 验证
        model.eval()
        val_loss = 0.0
        with torch.no_grad():
            for wt, text in val_loader:
                wt = wt.to(DEVICE)
                text = text.to(DEVICE)
                recon, mu, logvar = model(wt, text)
                loss, _ = vae_loss(recon, wt, mu, logvar)
                val_loss += loss.item()

        val_loss /= len(val_loader)
        scheduler.step()

        print(f"Epoch {epoch+1:2d}/{EPOCHS} | Train: {train_loss:.4f} | Val: {val_loss:.4f}", flush=True)

        if val_loss < best_val_loss:
            best_val_loss = val_loss
            torch.save(model.state_dict(), f"{MODEL_DIR}/wavetable_vae_best.pt")

    print(f"\nTraining complete. Best val loss: {best_val_loss:.4f}", flush=True)
    export_onnx(model)


# =============================================================================
# ONNX 导出 (仅解码器)
# =============================================================================

def export_onnx(model):
    model.eval()

    dummy_z = torch.randn(1, LATENT_DIM, device=DEVICE)
    dummy_text = torch.randn(1, TEXT_DIM, device=DEVICE)

    decoder_path = f"{MODEL_DIR}/wavetable_vae_decoder.onnx"
    torch.onnx.export(
        model.decoder,
        (dummy_z, dummy_text),
        decoder_path,
        input_names=["latent_z", "text_embedding"],
        output_names=["wavetable"],
        dynamic_axes={"latent_z": {0: "batch"}, "text_embedding": {0: "batch"},
                      "wavetable": {0: "batch"}},
        opset_version=14,
    )
    size_kb = os.path.getsize(decoder_path) / 1024
    print(f"Exported wavetable_vae_decoder.onnx ({size_kb:.1f} KB)", flush=True)

    # 验证 ONNX
    import onnxruntime as ort
    s = ort.InferenceSession(decoder_path)
    test_z = np.random.randn(1, LATENT_DIM).astype(np.float32)
    test_text = np.random.randn(1, TEXT_DIM).astype(np.float32)
    out = s.run(None, {"latent_z": test_z, "text_embedding": test_text})
    wt = out[0]
    print(f"  Output shape: {wt.shape}", flush=True)
    print(f"  Value range: [{wt.min():.4f}, {wt.max():.4f}]", flush=True)

    # 验证不同文本产生不同波表
    test_text2 = np.random.randn(1, TEXT_DIM).astype(np.float32)
    out2 = s.run(None, {"latent_z": test_z, "text_embedding": test_text2})
    diff = np.mean(np.abs(out[0] - out2[0]))
    print(f"  Mean diff between different texts: {diff:.6f}", flush=True)


if __name__ == "__main__":
    train()