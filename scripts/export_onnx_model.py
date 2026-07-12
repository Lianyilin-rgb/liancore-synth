# =============================================================================
# LianCore - ONNX 模型导出脚本
# 将关键词规则训练的线性模型导出为 ONNX 格式
# 用法: python scripts/export_onnx_model.py
# 输出: models/liancore_ai_model.onnx
# =============================================================================

import numpy as np
import os
import sys

# 尝试导入 PyTorch
try:
    import torch
    import torch.nn as nn
    import torch.onnx
    HAS_TORCH = True
except ImportError:
    HAS_TORCH = False
    print("[WARN] PyTorch not installed. Will export raw ONNX via onnx helper.")
    try:
        import onnx
        from onnx import helper, TensorProto
        HAS_ONNX_HELPER = True
    except ImportError:
        HAS_ONNX_HELPER = False
        print("[ERROR] Neither PyTorch nor onnx available. Install: pip install torch onnx")
        sys.exit(1)


# =============================================================================
# 关键词规则定义 (与 AIInferenceEngine::buildKeywordRules 同步)
# =============================================================================
KEYWORD_RULES = [
    # 音色形容词 → 滤波器截止频率
    ("明亮", "filter_cutoff", 0.8), ("温暖", "filter_cutoff", 0.3),
    ("暗", "filter_cutoff", 0.15), ("尖锐", "filter_cutoff", 0.9),
    ("柔和", "filter_cutoff", 0.25), ("厚重", "filter_cutoff", 0.2),
    ("轻盈", "filter_cutoff", 0.7),
    # 情感 → 滤波器共振
    ("紧张", "filter_resonance", 0.7), ("放松", "filter_resonance", 0.2),
    ("梦幻", "filter_resonance", 0.6), ("空灵", "filter_resonance", 0.5),
    # 风格 → 振荡器波形
    ("复古", "osc_waveform", 0.25), ("现代", "osc_waveform", 0.5),
    ("电子", "osc_waveform", 0.75), ("经典", "osc_waveform", 0.3),
    ("管弦", "osc_waveform", 0.15), ("贝斯", "osc_waveform", 0.4),
    ("主音", "osc_waveform", 0.6), ("铺底", "osc_waveform", 0.35),
    # 动态 → 包络
    ("快速", "env_attack", 0.1), ("慢速", "env_attack", 0.5),
    ("长音", "env_release", 0.8), ("短促", "env_release", 0.1),
    ("打击", "env_attack", 0.05), ("弹拨", "env_attack", 0.15),
    # 混响相关
    ("大厅", "reverb_size", 0.8), ("房间", "reverb_size", 0.3),
    ("环境", "reverb_size", 0.5),
    # 音高相关
    ("低音", "osc_pitch", 0.0), ("高音", "osc_pitch", 1.0),
    # 噪声层
    ("噪声", "noise_level", 0.3), ("纯净", "noise_level", 0.0),
]

# 输出参数列表 (共11个)
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
# 文本特征编码 (与 C++ runOnnxInference 中的编码方式一致)
# =============================================================================
def encode_text(text: str, dim: int = 128) -> np.ndarray:
    features = np.zeros(dim, dtype=np.float32)
    text_lower = text.lower()
    for i, ch in enumerate(text_lower):
        if i >= dim:
            break
        features[i % dim] += ord(ch) / 255.0
    # 归一化
    max_val = np.max(np.abs(features))
    if max_val > 0:
        features /= max_val
    return features


# =============================================================================
# 合成训练数据: 从关键词规则生成 (文本, 参数向量) 对
# =============================================================================
def generate_training_data(num_samples: int = 2000) -> tuple:
    np.random.seed(42)
    
    X = np.zeros((num_samples, INPUT_DIM), dtype=np.float32)
    Y = np.zeros((num_samples, OUTPUT_DIM), dtype=np.float32)
    
    # 默认值 (中间值)
    default_params = np.full(OUTPUT_DIM, 0.5, dtype=np.float32)
    
    for i in range(num_samples):
        # 随机选择 1-5 个关键词组合
        num_keywords = np.random.randint(1, 6)
        selected = np.random.choice(len(KEYWORD_RULES), num_keywords, replace=False)
        
        # 构建文本
        words = [KEYWORD_RULES[j][0] for j in selected]
        text = " ".join(words)
        
        # 编码文本
        X[i] = encode_text(text)
        
        # 计算目标参数
        targets = default_params.copy()
        for j in selected:
            _, param_name, value = KEYWORD_RULES[j]
            if param_name in PARAM_NAMES:
                idx = PARAM_NAMES.index(param_name)
                targets[idx] = value * 0.7 + targets[idx] * 0.3  # 加权平均
        
        Y[i] = targets
    
    return X, Y


# =============================================================================
# PyTorch 模型定义
# =============================================================================
class LianCoreAIModel(nn.Module):
    def __init__(self):
        super().__init__()
        self.fc1 = nn.Linear(INPUT_DIM, HIDDEN_DIM)
        self.fc2 = nn.Linear(HIDDEN_DIM, HIDDEN_DIM2)
        self.fc3 = nn.Linear(HIDDEN_DIM2, OUTPUT_DIM)
        self.relu = nn.ReLU()
        self.sigmoid = nn.Sigmoid()
    
    def forward(self, x):
        x = self.relu(self.fc1(x))
        x = self.relu(self.fc2(x))
        x = self.sigmoid(self.fc3(x))
        return x


# =============================================================================
# 使用 PyTorch 训练并导出 ONNX
# =============================================================================
def export_with_pytorch(output_path: str):
    print("[INFO] Generating training data...")
    X, Y = generate_training_data(3000)
    
    # 转换为 PyTorch tensors
    X_tensor = torch.from_numpy(X)
    Y_tensor = torch.from_numpy(Y)
    
    # 创建模型
    model = LianCoreAIModel()
    criterion = nn.MSELoss()
    optimizer = torch.optim.Adam(model.parameters(), lr=0.001)
    
    # 训练
    print("[INFO] Training model...")
    model.train()
    batch_size = 32
    num_epochs = 100
    
    for epoch in range(num_epochs):
        total_loss = 0.0
        num_batches = 0
        
        for i in range(0, len(X_tensor), batch_size):
            batch_x = X_tensor[i:i + batch_size]
            batch_y = Y_tensor[i:i + batch_size]
            
            optimizer.zero_grad()
            outputs = model(batch_x)
            loss = criterion(outputs, batch_y)
            loss.backward()
            optimizer.step()
            
            total_loss += loss.item()
            num_batches += 1
        
        if (epoch + 1) % 20 == 0:
            avg_loss = total_loss / max(num_batches, 1)
            print(f"  Epoch {epoch + 1}/{num_epochs}, Loss: {avg_loss:.6f}")
    
    # 评估
    model.eval()
    with torch.no_grad():
        test_outputs = model(X_tensor[:10])
        test_loss = criterion(test_outputs, Y_tensor[:10])
        print(f"[INFO] Test loss (10 samples): {test_loss.item():.6f}")
    
    # 导出 ONNX
    print(f"[INFO] Exporting ONNX model to: {output_path}")
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
    
    print("[INFO] ONNX model exported successfully!")
    print(f"  Input:  text_features [batch, {INPUT_DIM}]")
    print(f"  Output: parameters [batch, {OUTPUT_DIM}]")
    print(f"  File:   {output_path}")


# =============================================================================
# 使用 onnx.helper 直接构建 ONNX 模型 (无需 PyTorch)
# =============================================================================
def export_with_onnx_helper(output_path: str):
    print("[INFO] Building ONNX model with onnx.helper...")
    
    # 生成随机权重 (模拟训练后的权重)
    np.random.seed(42)
    W1 = np.random.randn(INPUT_DIM, HIDDEN_DIM).astype(np.float32) * 0.1
    b1 = np.zeros(HIDDEN_DIM, dtype=np.float32)
    W2 = np.random.randn(HIDDEN_DIM, HIDDEN_DIM2).astype(np.float32) * 0.1
    b2 = np.zeros(HIDDEN_DIM2, dtype=np.float32)
    W3 = np.random.randn(HIDDEN_DIM2, OUTPUT_DIM).astype(np.float32) * 0.1
    b3 = np.zeros(OUTPUT_DIM, dtype=np.float32)
    
    # 创建节点和初始化器
    nodes = [
        helper.make_node("MatMul", ["text_features", "W1"], ["fc1_raw"], "fc1_mm"),
        helper.make_node("Add", ["fc1_raw", "b1"], ["fc1"], "fc1_add"),
        helper.make_node("Relu", ["fc1"], ["relu1"], "relu1"),
        helper.make_node("MatMul", ["relu1", "W2"], ["fc2_raw"], "fc2_mm"),
        helper.make_node("Add", ["fc2_raw", "b2"], ["fc2"], "fc2_add"),
        helper.make_node("Relu", ["fc2"], ["relu2"], "relu2"),
        helper.make_node("MatMul", ["relu2", "W3"], ["fc3_raw"], "fc3_mm"),
        helper.make_node("Add", ["fc3_raw", "b3"], ["fc3"], "fc3_add"),
        helper.make_node("Sigmoid", ["fc3"], ["parameters"], "sigmoid"),
    ]
    
    initializers = [
        helper.make_tensor("W1", TensorProto.FLOAT, [INPUT_DIM, HIDDEN_DIM], W1.flatten().tolist()),
        helper.make_tensor("b1", TensorProto.FLOAT, [HIDDEN_DIM], b1.tolist()),
        helper.make_tensor("W2", TensorProto.FLOAT, [HIDDEN_DIM, HIDDEN_DIM2], W2.flatten().tolist()),
        helper.make_tensor("b2", TensorProto.FLOAT, [HIDDEN_DIM2], b2.tolist()),
        helper.make_tensor("W3", TensorProto.FLOAT, [HIDDEN_DIM2, OUTPUT_DIM], W3.flatten().tolist()),
        helper.make_tensor("b3", TensorProto.FLOAT, [OUTPUT_DIM], b3.tolist()),
    ]
    
    # 输入输出
    inputs = [
        helper.make_tensor_value_info("text_features", TensorProto.FLOAT, ["batch_size", INPUT_DIM]),
    ]
    outputs = [
        helper.make_tensor_value_info("parameters", TensorProto.FLOAT, ["batch_size", OUTPUT_DIM]),
    ]
    
    # 创建图
    graph = helper.make_graph(
        nodes=nodes,
        name="LianCoreAIModel",
        inputs=inputs,
        outputs=outputs,
        initializer=initializers,
    )
    
    # 创建模型
    model = helper.make_model(
        graph,
        producer_name="LianCore",
        producer_version="3.0.0",
        opset_imports=[helper.make_opsetid("", 14)],
    )
    
    # 验证
    onnx.checker.check_model(model)
    
    # 保存
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    onnx.save(model, output_path)
    
    print("[INFO] ONNX model exported successfully!")
    print(f"  Input:  text_features [batch, {INPUT_DIM}]")
    print(f"  Output: parameters [batch, {OUTPUT_DIM}]")
    print(f"  File:   {output_path}")
    print(f"  Size:   {os.path.getsize(output_path) / 1024:.1f} KB")


# =============================================================================
# 主入口
# =============================================================================
if __name__ == "__main__":
    # 输出路径
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    output_path = os.path.join(project_root, "models", "liancore_ai_model.onnx")
    
    if HAS_TORCH:
        export_with_pytorch(output_path)
    elif HAS_ONNX_HELPER:
        export_with_onnx_helper(output_path)
    else:
        print("[ERROR] Cannot export ONNX model. Install PyTorch or onnx package.")
        sys.exit(1)