// =============================================================================
// LianCore - 波表生成器 实现
// Gamma Week 5-6: 文本到波表合成
// =============================================================================

#include "WavetableGenerator.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <chrono>

namespace LianCore {
namespace AI {

// =============================================================================
// 构造/析构
// =============================================================================

WavetableGenerator::WavetableGenerator()
    : rng_(static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count()))
    , normalDist_(0.0f, 1.0f) {
}

WavetableGenerator::~WavetableGenerator() = default;

// =============================================================================
// 模型加载
// =============================================================================

bool WavetableGenerator::loadModel(const juce::File& onnxPath) {
    if (!onnxPath.existsAsFile()) {
        return false;
    }
    
    // 延迟初始化 ONNX Runtime
    if (!ortEnv_) {
        try {
            ortEnv_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "WavetableGenerator");
            sessionOptions_ = std::make_unique<Ort::SessionOptions>();
            sessionOptions_->SetIntraOpNumThreads(2);
            sessionOptions_->SetGraphOptimizationLevel(
                GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
        } catch (const Ort::Exception&) {
            ortEnv_.reset();
            sessionOptions_.reset();
            return false;
        } catch (const std::exception&) {
            ortEnv_.reset();
            sessionOptions_.reset();
            return false;
        }
    }
    
    try {
#ifdef _WIN32
        std::wstring pathStr = onnxPath.getFullPathName().toWideCharPointer();
        session_.reset(new Ort::Session(*ortEnv_, pathStr.c_str(), *sessionOptions_));
#else
        std::string pathStr = onnxPath.getFullPathName().toStdString();
        session_.reset(new Ort::Session(*ortEnv_, pathStr.c_str(), *sessionOptions_));
#endif
        modelLoaded_ = true;
        return true;
    } catch (const Ort::Exception&) {
        modelLoaded_ = false;
        return false;
    }
}

// =============================================================================
// 核心推理
// =============================================================================

WavetableGenerator::WavetableResult WavetableGenerator::generate(
    const std::vector<float>& textEmbedding) {
    
    WavetableResult result;
    result.numFrames = 256;
    result.tableSize = 2048;
    
    if (!modelLoaded_) {
        result.errorMessage = "Model not loaded. Call loadModel() first.";
        return result;
    }
    
    if (textEmbedding.size() != 128) {
        result.errorMessage = "Text embedding must be 128-dimensional. Got " 
                              + std::to_string(textEmbedding.size()) + ".";
        return result;
    }
    
    try {
        // 步骤1: 生成随机潜在向量 z ~ N(0, 1)
        std::vector<float> latentZ(64);
        for (int i = 0; i < 64; ++i) {
            latentZ[i] = normalDist_(rng_);
        }
        
        // 步骤2: 准备 ONNX 输入张量
        Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(
            OrtArenaAllocator, OrtMemTypeDefault);
        
        // 潜在向量: [1, 64]
        std::vector<int64_t> latentShape = {1, 64};
        Ort::Value latentTensor = Ort::Value::CreateTensor<float>(
            memoryInfo, latentZ.data(), latentZ.size(),
            latentShape.data(), latentShape.size());
        
        // 文本嵌入: [1, 128]
        std::vector<int64_t> textShape = {1, 128};
        // 需要复制因为 CreateTensor 不拥有数据
        std::vector<float> textCopy = textEmbedding;
        Ort::Value textTensor = Ort::Value::CreateTensor<float>(
            memoryInfo, textCopy.data(), textCopy.size(),
            textShape.data(), textShape.size());
        
        // 步骤3: 运行 ONNX 推理
        const char* inputNames[] = {"latent_z", "text_embedding"};
        const char* outputNames[] = {"wavetable"};
        
        std::vector<Ort::Value> inputs;
        inputs.push_back(std::move(latentTensor));
        inputs.push_back(std::move(textTensor));
        
        auto outputs = session_->Run(
            Ort::RunOptions{nullptr},
            inputNames, inputs.data(), 2,
            outputNames, 1);
        
        // 步骤4: 提取输出波表
        float* wtData = outputs[0].GetTensorMutableData<float>();
        auto wtShape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
        
        if (wtShape.size() != 3 || wtShape[1] != 256 || wtShape[2] != 2048) {
            result.errorMessage = "Unexpected output shape: expected [1, 256, 2048]";
            return result;
        }
        
        int totalElements = static_cast<int>(wtShape[1] * wtShape[2]);
        result.wavetable.assign(wtData, wtData + totalElements);
        
        // 步骤5: 后处理
        applyFrameSmoothing(result.wavetable);
        applyAntiAliasing(result.wavetable);
        
        // 步骤6: 最终裁剪到 [-1, 1]
        for (float& v : result.wavetable) {
            v = std::max(-1.0f, std::min(1.0f, v));
        }
        
    } catch (const Ort::Exception& e) {
        result.errorMessage = std::string("ONNX inference error: ") + e.what();
    } catch (const std::exception& e) {
        result.errorMessage = std::string("Error: ") + e.what();
    }
    
    return result;
}

// =============================================================================
// 帧间平滑
// 对相邻帧做线性混合，消除突变 (避免扫描时产生咔嗒声)
// =============================================================================

void WavetableGenerator::applyFrameSmoothing(std::vector<float>& wavetable, float strength) {
    const int numFrames = 256;
    const int tableSize = 2048;
    
    if (wavetable.size() != static_cast<size_t>(numFrames * tableSize)) {
        return;
    }
    
    // 前向-后向平滑 (避免相位偏移)
    std::vector<float> smoothed = wavetable;
    
    for (int frame = 1; frame < numFrames - 1; ++frame) {
        float* prevFrame = &wavetable[(frame - 1) * tableSize];
        float* currFrame = &wavetable[frame * tableSize];
        float* nextFrame = &wavetable[(frame + 1) * tableSize];
        float* outFrame = &smoothed[frame * tableSize];
        
        for (int i = 0; i < tableSize; ++i) {
            // 三点加权平均: 0.5*当前帧 + 0.25*前一帧 + 0.25*后一帧
            float blend = (1.0f - strength) * currFrame[i]
                        + strength * 0.5f * (prevFrame[i] + nextFrame[i]);
            outFrame[i] = blend;
        }
    }
    
    wavetable = std::move(smoothed);
}

// =============================================================================
// 抗混叠后处理
// 对每帧波表应用低通滤波 (15kHz cutoff, 去除奈奎斯特以上谐波)
// 使用 1 阶 IIR 低通滤波器 (简化但有效)
// =============================================================================

void WavetableGenerator::applyAntiAliasing(std::vector<float>& wavetable) {
    const int numFrames = 256;
    const int tableSize = 2048;
    const float sampleRate = 44100.0f;
    
    if (wavetable.size() != static_cast<size_t>(numFrames * tableSize)) {
        return;
    }
    
    // 1阶 IIR 低通滤波器: y[n] = alpha * x[n] + (1 - alpha) * y[n-1]
    // 截止频率 fc = 15kHz, 采样率 fs = 44.1kHz
    // alpha = 2*pi*fc / (2*pi*fc + fs) ≈ 2*pi*fc / fs
    const float fc = 15000.0f;
    const float alpha = 2.0f * 3.14159265358979323846f * fc / sampleRate;
    const float oneMinusAlpha = 1.0f - alpha;
    
    for (int frame = 0; frame < numFrames; ++frame) {
        float* frameData = &wavetable[frame * tableSize];
        
        float prevOut = frameData[0];  // 初始条件
        for (int i = 0; i < tableSize; ++i) {
            float input = frameData[i];
            float output = alpha * input + oneMinusAlpha * prevOut;
            frameData[i] = output;
            prevOut = output;
        }
    }
}

} // namespace AI
} // namespace LianCore