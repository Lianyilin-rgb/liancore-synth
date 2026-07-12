# =============================================================================
# LianCore - 音频编码器 + 参数回归网络 训练脚本
# Gamma Week 3-4: 音频参考音色复刻
# 架构: 双分支 (波形1D CNN + 频谱2D CNN) → 128维嵌入
#        参数回归: 嵌入128 + 文本128 → 4层MLP → 11维参数
# =============================================================================

import torch
import torch.nn as nn
import torch.nn.functional as F
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
import numpy as np
import os
import sys

# 配置
BATCH_SIZE = 32
EPOCHS = 15
LEARNING_RATE = 1e-3
WEIGHT_DECAY = 1e-5
DATA_DIR = "data/training/audio_timbre"
MODEL_DIR = "models"
DEVICE = torch.device("cuda" if torch.cuda.is_available() else "cpu")

# =============================================================================
# 模型定义
# =============================================================================

class AudioEncoder(nn.Module):
    """轻量音频编码器：波形 1D CNN + Mel 频谱 2D CNN → 128维嵌入"""
    def __init__(self):
        super().__init__()
        # 波形分支 (1D CNN)
        self.wave_conv1 = nn.Conv1d(1, 32, kernel_size=16, stride=4, padding=7)
        self.wave_bn1 = nn.BatchNorm1d(32)
        self.wave_conv2 = nn.Conv1d(32, 64, kernel_size=8, stride=4, padding=3)
        self.wave_bn2 = nn.BatchNorm1d(64)
        self.wave_conv3 = nn.Conv1d(64, 128, kernel_size=4, stride=2, padding=1)
        self.wave_bn3 = nn.BatchNorm1d(128)
        self.wave_pool = nn.AdaptiveAvgPool1d(1)
        
        # 频谱分支 (2D CNN on Mel)
        self.spec_conv1 = nn.Conv2d(1, 16, kernel_size=3, padding=1)
        self.spec_bn1 = nn.BatchNorm2d(16)
        self.spec_conv2 = nn.Conv2d(16, 32, kernel_size=3, padding=1)
        self.spec_bn2 = nn.BatchNorm2d(32)
        self.spec_pool = nn.AdaptiveAvgPool2d((4, 8))
        self.spec_fc = nn.Linear(32 * 4 * 8, 64)
        
        # 融合层
        self.fusion = nn.Linear(128 + 64, 128)
        self.ln = nn.LayerNorm(128)
        
    def forward(self, waveform, mel_spec):
        # waveform: [B, 16384]
        # mel_spec:  [B, 1, 64, 64]
        w = waveform.unsqueeze(1)  # [B, 1, 16384]
        w = F.relu(self.wave_bn1(self.wave_conv1(w)))
        w = F.relu(self.wave_bn2(self.wave_conv2(w)))
        w = F.relu(self.wave_bn3(self.wave_conv3(w)))
        w = self.wave_pool(w).squeeze(-1)  # [B, 128]
        
        s = F.relu(self.spec_bn1(self.spec_conv1(mel_spec)))
        s = F.relu(self.spec_bn2(self.spec_conv2(s)))
        s = self.spec_pool(s)  # [B, 32, 4, 8]
        s = s.reshape(s.size(0), -1)  # [B, 1024]
        s = self.spec_fc(s)  # [B, 64]
        
        fused = torch.cat([w, s], dim=-1)  # [B, 192]
        embedding = self.fusion(fused)  # [B, 128]
        embedding = self.ln(embedding)
        return embedding


class ParamRegression(nn.Module):
    """参数回归网络：音频嵌入 + 文本嵌入 → 11维参数"""
    def __init__(self):
        super().__init__()
        self.fc1 = nn.Linear(256, 128)
        self.fc2 = nn.Linear(128, 64)
        self.fc3 = nn.Linear(64, 32)
        self.fc4 = nn.Linear(32, 11)
        self.dropout = nn.Dropout(0.2)
        
    def forward(self, audio_emb, text_emb):
        x = torch.cat([audio_emb, text_emb], dim=-1)  # [B, 256]
        x = F.relu(self.fc1(x))
        x = self.dropout(x)
        x = F.relu(self.fc2(x))
        x = self.dropout(x)
        x = F.relu(self.fc3(x))
        x = torch.sigmoid(self.fc4(x))
        return x


class TimbreReplicationModel(nn.Module):
    """端到端音色复刻模型"""
    def __init__(self):
        super().__init__()
        self.audio_encoder = AudioEncoder()
        self.param_regression = ParamRegression()
        
    def forward(self, waveform, mel_spec, text_emb=None):
        if text_emb is None:
            text_emb = torch.zeros(waveform.size(0), 128, device=waveform.device)
        audio_emb = self.audio_encoder(waveform, mel_spec)
        params = self.param_regression(audio_emb, text_emb)
        return params, audio_emb


# =============================================================================
# 数据集
# =============================================================================

class TimbreDataset(Dataset):
    def __init__(self, npz_path):
        data = np.load(npz_path)
        self.params = torch.from_numpy(data["params"].copy()).float()
        self.audio = torch.from_numpy(data["audio"].copy()).float()
        self.mel = torch.from_numpy(data["mel"].copy()).float()
        
    def __len__(self):
        return len(self.params)
    
    def __getitem__(self, idx):
        return self.audio[idx], self.mel[idx], self.params[idx]


# =============================================================================
# 损失函数
# =============================================================================

def timbre_loss(pred_params, target_params, pred_audio_emb):
    """多任务损失：参数MSE + 嵌入一致性"""
    # 连续参数 MSE
    continuous_idx = [1, 2, 4, 5, 6, 7, 8, 9, 10]
    mse_continuous = F.mse_loss(
        pred_params[:, continuous_idx], 
        target_params[:, continuous_idx]
    )
    
    # 离散参数 MSE
    discrete_idx = [0, 3]
    mse_discrete = F.mse_loss(
        pred_params[:, discrete_idx],
        target_params[:, discrete_idx]
    )
    
    # 嵌入一致性
    pred_norm = F.normalize(pred_audio_emb, dim=-1)
    embed_var = pred_norm.var(dim=0).mean()
    embed_loss = -embed_var  # 鼓励嵌入多样性
    
    total = mse_continuous + 0.5 * mse_discrete + 0.01 * embed_loss
    return total, {"mse_continuous": mse_continuous.item(), 
                   "mse_discrete": mse_discrete.item(),
                   "embed_var": embed_var.item()}


# =============================================================================
# 训练
# =============================================================================

def train():
    os.makedirs(MODEL_DIR, exist_ok=True)
    
    # 加载数据
    train_dataset = TimbreDataset(f"{DATA_DIR}/train.npz")
    val_dataset = TimbreDataset(f"{DATA_DIR}/val.npz")
    train_loader = DataLoader(train_dataset, batch_size=BATCH_SIZE, shuffle=True)
    val_loader = DataLoader(val_dataset, batch_size=BATCH_SIZE, shuffle=False)
    
    print(f"Train: {len(train_dataset)}, Val: {len(val_dataset)}, Device: {DEVICE}")
    
    # 初始化
    model = TimbreReplicationModel().to(DEVICE)
    optimizer = optim.Adam(model.parameters(), lr=LEARNING_RATE, weight_decay=WEIGHT_DECAY)
    scheduler = optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=EPOCHS)
    
    best_val_loss = float('inf')
    
    for epoch in range(EPOCHS):
        # 训练
        model.train()
        train_loss = 0.0
        for audio, mel, params in train_loader:
            audio = audio.to(DEVICE)
            mel = mel.unsqueeze(1).to(DEVICE)
            params = params.to(DEVICE)
            
            optimizer.zero_grad()
            pred_params, pred_audio_emb = model(audio, mel)
            loss, losses = timbre_loss(pred_params, params, pred_audio_emb)
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            optimizer.step()
            train_loss += loss.item()
        
        train_loss /= len(train_loader)
        
        # 验证
        model.eval()
        val_loss = 0.0
        val_mse = 0.0
        with torch.no_grad():
            for audio, mel, params in val_loader:
                audio = audio.to(DEVICE)
                mel = mel.unsqueeze(1).to(DEVICE)
                params = params.to(DEVICE)
                pred_params, pred_audio_emb = model(audio, mel)
                loss, _ = timbre_loss(pred_params, params, pred_audio_emb)
                val_loss += loss.item()
                val_mse += F.mse_loss(pred_params, params).item()
        
        val_loss /= len(val_loader)
        val_mse /= len(val_loader)
        scheduler.step()
        
        print(f"Epoch {epoch+1:2d}/{EPOCHS} | Train: {train_loss:.4f} | Val: {val_loss:.4f} | MSE: {val_mse:.4f}")
        
        if val_loss < best_val_loss:
            best_val_loss = val_loss
            torch.save(model.state_dict(), f"{MODEL_DIR}/timbre_replication_best.pt")
    
    print(f"\nTraining complete. Best val loss: {best_val_loss:.4f}")
    export_onnx(model)


# =============================================================================
# ONNX 导出
# =============================================================================

def export_onnx(model):
    """导出音频编码器和参数回归器为 ONNX"""
    model.eval()
    
    # 1. 导出音频编码器
    dummy_wave = torch.randn(1, 16384, device=DEVICE)
    dummy_mel = torch.randn(1, 1, 64, 64, device=DEVICE)
    
    encoder_path = f"{MODEL_DIR}/audio_encoder.onnx"
    torch.onnx.export(
        model.audio_encoder,
        (dummy_wave, dummy_mel),
        encoder_path,
        input_names=["waveform", "mel_spec"],
        output_names=["audio_embedding"],
        dynamic_axes={"waveform": {0: "batch"}, "mel_spec": {0: "batch"},
                      "audio_embedding": {0: "batch"}},
        opset_version=14,
    )
    size_kb = os.path.getsize(encoder_path) / 1024
    print(f"Exported audio_encoder.onnx ({size_kb:.1f} KB)")
    
    # 2. 导出参数回归器
    dummy_audio_emb = torch.randn(1, 128, device=DEVICE)
    dummy_text_emb = torch.randn(1, 128, device=DEVICE)
    
    regressor_path = f"{MODEL_DIR}/param_regressor.onnx"
    torch.onnx.export(
        model.param_regression,
        (dummy_audio_emb, dummy_text_emb),
        regressor_path,
        input_names=["audio_embedding", "text_embedding"],
        output_names=["parameters"],
        dynamic_axes={"audio_embedding": {0: "batch"}, "text_embedding": {0: "batch"},
                      "parameters": {0: "batch"}},
        opset_version=14,
    )
    size_kb = os.path.getsize(regressor_path) / 1024
    print(f"Exported param_regressor.onnx ({size_kb:.1f} KB)")
    
    # 3. 验证 ONNX
    import onnxruntime as ort
    print("\nVerifying ONNX models...")
    
    s1 = ort.InferenceSession(encoder_path)
    test_wave = np.random.randn(1, 16384).astype(np.float32)
    test_mel = np.random.randn(1, 1, 64, 64).astype(np.float32)
    out1 = s1.run(None, {"waveform": test_wave, "mel_spec": test_mel})
    emb = out1[0]
    print(f"  Audio encoder output: {emb.shape}")
    
    s2 = ort.InferenceSession(regressor_path)
    test_text = np.zeros((1, 128), dtype=np.float32)
    out2 = s2.run(None, {"audio_embedding": emb, "text_embedding": test_text})
    params = out2[0]
    print(f"  Param regressor output: {params.shape}")
    print(f"  Parameters: [{params.min():.4f}, {params.max():.4f}]")


if __name__ == "__main__":
    train()