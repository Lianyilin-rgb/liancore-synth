# =============================================================================
# LianCore - ONNX 模型训练与导出脚本 (Gamma 阶段)
# 从真实预设数据库读取数据，训练 MLP 模型 (128→64→32→11)
# 输出: models/liancore_ai_model.onnx
# 用法: python scripts/export_onnx_model.py
# =============================================================================

import numpy as np
import os
import sys
import sqlite3
import json
import time

# 尝试导入 PyTorch
try:
    import torch
    import torch.nn as nn
    import torch.onnx
    HAS_TORCH = True
except ImportError:
    HAS_TORCH = False
    print("[ERROR] PyTorch is required. Install: pip install torch")
    sys.exit(1)

# =============================================================================
# 输出参数列表 (共11个)
# =============================================================================
PARAM_NAMES = [
    "filter_cutoff", "filter_resonance", "osc_waveform",
    "env_attack", "env_decay", "env_sustain", "env_release",
    "lfo_rate", "lfo_depth", "reverb_size", "noise_level"
]

INPUT_DIM = 128
HIDDEN_DIM = 64
HIDDEN_DIM2 = 32
OUTPUT_DIM = len(PARAM_NAMES)


# =============================================================================
# 关键词规则定义 (与 AIInferenceEngine::buildKeywordRules 同步)
# =============================================================================
KEYWORD_RULES = [
    ("明亮", "filter_cutoff", 0.8), ("温暖", "filter_cutoff", 0.3),
    ("暗", "filter_cutoff", 0.15), ("尖锐", "filter_cutoff", 0.9),
    ("柔和", "filter_cutoff", 0.25), ("厚重", "filter_cutoff", 0.2),
    ("轻盈", "filter_cutoff", 0.7),
    ("紧张", "filter_resonance", 0.7), ("放松", "filter_resonance", 0.2),
    ("梦幻", "filter_resonance", 0.6), ("空灵", "filter_resonance", 0.5),
    ("复古", "osc_waveform", 0.25), ("现代", "osc_waveform", 0.5),
    ("电子", "osc_waveform", 0.75), ("经典", "osc_waveform", 0.3),
    ("管弦", "osc_waveform", 0.15), ("贝斯", "osc_waveform", 0.4),
    ("主音", "osc_waveform", 0.6), ("铺底", "osc_waveform", 0.35),
    ("快速", "env_attack", 0.1), ("慢速", "env_attack", 0.5),
    ("长音", "env_release", 0.8), ("短促", "env_release", 0.1),
    ("打击", "env_attack", 0.05), ("弹拨", "env_attack", 0.15),
    ("大厅", "reverb_size", 0.8), ("房间", "reverb_size", 0.3),
    ("环境", "reverb_size", 0.5),
    ("低音", "osc_pitch", 0.0), ("高音", "osc_pitch", 1.0),
    ("噪声", "noise_level", 0.3), ("纯净", "noise_level", 0.0),
]


# =============================================================================
# 文本特征编码 (与 C++ runOnnxInference 中的编码方式一致)
# =============================================================================
def encode_text(text: str, dim: int = 128) -> np.ndarray:
    features = np.zeros(dim, dtype=np.float32)
    text_lower = text.lower()
    for i, ch in enumerate(text_lower):
        if i >= dim:
            break
        features[i % dim] += ord(ch) / 255.0
    max_val = np.max(np.abs(features))
    if max_val > 0:
        features /= max_val
    return features


# =============================================================================
# 从数据库加载真实预设数据
# =============================================================================
def load_presets_from_db(db_path: str, max_samples: int = 50000) -> tuple:
    """从 preset_library.db 读取预设名称和参数，返回 (texts, params)"""
    print(f"[INFO] Loading presets from database: {db_path}")
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()

    # 探测表结构
    cursor.execute("SELECT name FROM sqlite_master WHERE type='table'")
    tables = [r[0] for r in cursor.fetchall()]
    print(f"  Tables: {tables}")

    texts = []
    params = []

    # 尝试不同的表结构
    for table in tables:
        try:
            cursor.execute(f"SELECT * FROM {table} LIMIT 1")
            cols = [d[0] for d in cursor.description]
            print(f"  Table '{table}' columns: {cols}")

            # 查找名称和参数列
            name_col = None
            param_cols = []

            for col in cols:
                col_lower = col.lower()
                if col_lower in ('name', 'preset_name', 'title', 'description', 'text'):
                    name_col = col
                elif col_lower in PARAM_NAMES:
                    param_cols.append(col)

            if name_col and param_cols:
                print(f"  Using table '{table}': name={name_col}, params={param_cols}")

                # 批量读取
                param_col_str = ', '.join(param_cols)
                cursor.execute(
                    f"SELECT {name_col}, {param_col_str} FROM {table} LIMIT {max_samples}"
                )

                for row in cursor.fetchall():
                    text = str(row[0]) if row[0] else ""
                    if not text.strip():
                        continue

                    # 提取参数值
                    vals = np.full(OUTPUT_DIM, 0.5, dtype=np.float32)
                    for j, col in enumerate(param_cols):
                        idx = PARAM_NAMES.index(col) if col in PARAM_NAMES else -1
                        if idx >= 0:
                            v = row[j + 1]
                            if v is not None:
                                vals[idx] = float(v)

                    texts.append(text)
                    params.append(vals)

                break  # 找到可用表后退出
        except Exception as e:
            print(f"  Table '{table}' error: {e}")
            continue

    conn.close()

    if not texts:
        print("[WARN] No valid data found in database. Falling back to synthetic data.")
        return None, None

    print(f"[INFO] Loaded {len(texts)} presets from database")
    return texts, params


# =============================================================================
# 合成训练数据: 从关键词规则生成 (数据库回退)
# =============================================================================
def generate_training_data(num_samples: int = 3000) -> tuple:
    np.random.seed(42)

    X = np.zeros((num_samples, INPUT_DIM), dtype=np.float32)
    Y = np.zeros((num_samples, OUTPUT_DIM), dtype=np.float32)
    default_params = np.full(OUTPUT_DIM, 0.5, dtype=np.float32)

    for i in range(num_samples):
        num_keywords = np.random.randint(1, 6)
        selected = np.random.choice(len(KEYWORD_RULES), num_keywords, replace=False)
        words = [KEYWORD_RULES[j][0] for j in selected]
        text = " ".join(words)
        X[i] = encode_text(text)

        targets = default_params.copy()
        for j in selected:
            _, param_name, value = KEYWORD_RULES[j]
            if param_name in PARAM_NAMES:
                idx = PARAM_NAMES.index(param_name)
                targets[idx] = value * 0.7 + targets[idx] * 0.3
        Y[i] = targets

    return X, Y


# =============================================================================
# PyTorch MLP 模型 (128→64→32→11)
# =============================================================================
class LianCoreAIModel(nn.Module):
    def __init__(self):
        super().__init__()
        self.fc1 = nn.Linear(INPUT_DIM, HIDDEN_DIM)
        self.bn1 = nn.BatchNorm1d(HIDDEN_DIM)
        self.fc2 = nn.Linear(HIDDEN_DIM, HIDDEN_DIM2)
        self.bn2 = nn.BatchNorm1d(HIDDEN_DIM2)
        self.fc3 = nn.Linear(HIDDEN_DIM2, OUTPUT_DIM)
        self.relu = nn.ReLU()
        self.sigmoid = nn.Sigmoid()
        self.dropout = nn.Dropout(0.1)

    def forward(self, x):
        x = self.relu(self.bn1(self.fc1(x)))
        x = self.dropout(x)
        x = self.relu(self.bn2(self.fc2(x)))
        x = self.sigmoid(self.fc3(x))
        return x


# =============================================================================
# 训练并导出 ONNX
# =============================================================================
def train_and_export(output_path: str, db_path: str):
    # 1. 加载数据 (优先数据库, 回退到合成数据)
    texts, params = load_presets_from_db(db_path)

    if texts is not None and len(texts) > 100:
        # 使用真实数据
        print(f"[INFO] Encoding {len(texts)} real presets...")
        X = np.zeros((len(texts), INPUT_DIM), dtype=np.float32)
        Y = np.array(params, dtype=np.float32)
        for i, text in enumerate(texts):
            X[i] = encode_text(text)
    else:
        # 回退到合成数据
        print("[INFO] Using synthetic training data...")
        X, Y = generate_training_data(5000)

    print(f"  Training data: X={X.shape}, Y={Y.shape}")

    # 2. 划分训练/验证集
    split = int(len(X) * 0.85)
    indices = np.random.permutation(len(X))
    train_idx, val_idx = indices[:split], indices[split:]
    X_train, Y_train = X[train_idx], Y[train_idx]
    X_val, Y_val = X[val_idx], Y[val_idx]

    # 3. 创建模型
    model = LianCoreAIModel()
    criterion = nn.MSELoss()
    optimizer = torch.optim.Adam(model.parameters(), lr=0.001, weight_decay=1e-5)
    scheduler = torch.optim.lr_scheduler.StepLR(optimizer, step_size=40, gamma=0.5)

    # 4. 训练
    print("[INFO] Training model (128→64→32→11)...")
    model.train()
    batch_size = 64
    num_epochs = 150

    X_train_t = torch.from_numpy(X_train)
    Y_train_t = torch.from_numpy(Y_train)
    X_val_t = torch.from_numpy(X_val)
    Y_val_t = torch.from_numpy(Y_val)

    best_val_loss = float('inf')
    train_start = time.time()

    for epoch in range(num_epochs):
        total_loss = 0.0
        num_batches = 0

        for i in range(0, len(X_train_t), batch_size):
            batch_x = X_train_t[i:i + batch_size]
            batch_y = Y_train_t[i:i + batch_size]

            optimizer.zero_grad()
            outputs = model(batch_x)
            loss = criterion(outputs, batch_y)
            loss.backward()
            optimizer.step()

            total_loss += loss.item()
            num_batches += 1

        scheduler.step()

        if (epoch + 1) % 30 == 0:
            val_pred = model(X_val_t)
            val_loss = criterion(val_pred, Y_val_t).item()
            avg_loss = total_loss / max(num_batches, 1)
            print(f"  Epoch {epoch + 1}/{num_epochs}, Train: {avg_loss:.6f}, Val: {val_loss:.6f}")

            if val_loss < best_val_loss:
                best_val_loss = val_loss

    train_time = time.time() - train_start
    print(f"[INFO] Training completed in {train_time:.1f}s")

    # 5. 评估
    model.eval()
    with torch.no_grad():
        test_pred = model(X_val_t[:min(50, len(X_val_t))])
        test_loss = criterion(test_pred, Y_val_t[:min(50, len(X_val_t))]).item()
        print(f"[INFO] Final validation loss: {test_loss:.6f}")

        # 验证输出范围
        pred_min = test_pred.min().item()
        pred_max = test_pred.max().item()
        print(f"[INFO] Output range: [{pred_min:.4f}, {pred_max:.4f}] (expected: [0.0, 1.0])")

        # 示例推理
        print("\n[INFO] Sample inferences:")
        test_texts = [
            "温暖的贝斯",
            "明亮的合成器主音",
            "梦幻的铺底音色",
            "尖锐的电子音色",
            "柔和的钢琴音色",
            "厚重的打击音色",
            "空灵的环境音色",
            "复古的管弦乐",
            "现代的贝斯音色",
            "快速的弹拨音色",
        ]
        for tt in test_texts:
            feats = encode_text(tt)
            feat_t = torch.from_numpy(feats).unsqueeze(0)
            out = model(feat_t).squeeze().numpy()
            print(f"  '{tt}': {[f'{v:.2f}' for v in out]}")

    # 6. 导出 ONNX
    print(f"\n[INFO] Exporting ONNX model to: {output_path}")
    os.makedirs(os.path.dirname(output_path), exist_ok=True)

    dummy_input = torch.randn(1, INPUT_DIM)

    torch.onnx.export(
        model,
        dummy_input,
        output_path,
        export_params=True,
        opset_version=14,
        do_constant_folding=True,
        input_names=["text_features"],
        output_names=["parameters"],
        dynamic_axes={
            "text_features": {0: "batch_size"},
            "parameters": {0: "batch_size"},
        },
    )

    file_size_kb = os.path.getsize(output_path) / 1024
    print(f"[INFO] ONNX model exported successfully!")
    print(f"  Architecture: {INPUT_DIM}→{HIDDEN_DIM}→{HIDDEN_DIM2}→{OUTPUT_DIM}")
    print(f"  Input:  text_features [batch, {INPUT_DIM}]")
    print(f"  Output: parameters [batch, {OUTPUT_DIM}]")
    print(f"  File:   {output_path}")
    print(f"  Size:   {file_size_kb:.1f} KB")

    # 验证 ONNX 模型可加载
    try:
        import onnxruntime as ort
        session = ort.InferenceSession(output_path)
        test_input = np.random.randn(1, INPUT_DIM).astype(np.float32)
        test_output = session.run(None, {"text_features": test_input})
        ort_time = time.time()
        for _ in range(100):
            session.run(None, {"text_features": test_input})
        avg_inference_ms = (time.time() - ort_time) / 100 * 1000
        print(f"[INFO] ONNX Runtime verification: OK")
        print(f"  Inference latency: {avg_inference_ms:.2f}ms (target <50ms)")
        if avg_inference_ms < 50:
            print(f"  ✓ Latency meets Gamma requirement")
        else:
            print(f"  ⚠ Latency exceeds 50ms target")
    except Exception as e:
        print(f"[WARN] ONNX Runtime verification failed: {e}")


# =============================================================================
# 主入口
# =============================================================================
if __name__ == "__main__":
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    output_path = os.path.join(project_root, "models", "liancore_ai_model.onnx")
    db_path = os.path.join(project_root, "data", "preset_library.db")

    if not os.path.exists(db_path):
        print(f"[WARN] Database not found: {db_path}")
        print("[INFO] Training with synthetic data only...")
        db_path = None

    train_and_export(output_path, db_path if os.path.exists(db_path) else None)