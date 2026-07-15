// =============================================================================
// LianCore - 音频音色分析器 实现
// Gamma Week 3-4: 音频参考音色复刻
// =============================================================================

#include "AudioTimbreAnalyzer.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <juce_dsp/juce_dsp.h>

namespace LianCore {
namespace AI {

// =============================================================================
// 构造/析构
// =============================================================================

AudioTimbreAnalyzer::AudioTimbreAnalyzer() {
    // ONNX Runtime 延迟初始化，避免构造函数中 API 版本不匹配导致崩溃
}

AudioTimbreAnalyzer::~AudioTimbreAnalyzer() = default;

// =============================================================================
// 模型加载
// =============================================================================

bool AudioTimbreAnalyzer::loadModels(const juce::File& audioEncoderPath,
                                      const juce::File& paramRegressorPath) {
#ifdef LIANCORE_HAS_ONNX
    if (!audioEncoderPath.existsAsFile()) {
        return false;
    }
    if (!paramRegressorPath.existsAsFile()) {
        return false;
    }
    
    // 延迟初始化 ONNX Runtime
    if (!ortEnv_) {
        try {
            ortEnv_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "AudioTimbreAnalyzer");
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
        std::wstring encoderPathStr = audioEncoderPath.getFullPathName().toWideCharPointer();
        audioEncoderSession_.reset(new Ort::Session(
            *ortEnv_, encoderPathStr.c_str(), *sessionOptions_));
        audioEncoderLoaded_ = true;
        
        std::wstring regressorPathStr = paramRegressorPath.getFullPathName().toWideCharPointer();
        paramRegressorSession_.reset(new Ort::Session(
            *ortEnv_, regressorPathStr.c_str(), *sessionOptions_));
#else
        std::string encoderPathStr = audioEncoderPath.getFullPathName().toStdString();
        audioEncoderSession_.reset(new Ort::Session(
            *ortEnv_, encoderPathStr.c_str(), *sessionOptions_));
        audioEncoderLoaded_ = true;
        
        std::string regressorPathStr = paramRegressorPath.getFullPathName().toStdString();
        paramRegressorSession_.reset(new Ort::Session(
            *ortEnv_, regressorPathStr.c_str(), *sessionOptions_));
#endif
        paramRegressorLoaded_ = true;
        
        return true;
    } catch (const Ort::Exception& e) {
        audioEncoderLoaded_ = false;
        paramRegressorLoaded_ = false;
        return false;
    }
#else
    (void)audioEncoderPath;
    (void)paramRegressorPath;
    return false;
#endif
}

// =============================================================================
// 音频预处理
// =============================================================================

std::vector<float> AudioTimbreAnalyzer::preprocessAudio(
    const juce::AudioBuffer<float>& audio, double sampleRate) {
    
    const float* readPtr = audio.getReadPointer(0);
    int numSamples = audio.getNumSamples();
    
    std::vector<float> processed(TARGET_SAMPLES, 0.0f);
    
    // 步骤1: 重采样至 44.1kHz (线性插值)
    if (std::abs(sampleRate - TARGET_SR) > 1.0 && numSamples > 0) {
        double ratio = TARGET_SR / sampleRate;
        int outLen = static_cast<int>(numSamples * ratio);
        std::vector<float> resampled(outLen, 0.0f);
        
        for (int i = 0; i < outLen; ++i) {
            double srcIdx = i / ratio;
            int srcIdxInt = static_cast<int>(srcIdx);
            double frac = srcIdx - srcIdxInt;
            
            if (srcIdxInt + 1 < numSamples) {
                resampled[i] = static_cast<float>(
                    readPtr[srcIdxInt] * (1.0 - frac) + readPtr[srcIdxInt + 1] * frac);
            } else if (srcIdxInt < numSamples) {
                resampled[i] = readPtr[srcIdxInt];
            }
        }
        
        int copyLen = std::min(TARGET_SAMPLES, outLen);
        for (int i = 0; i < copyLen; ++i) {
            processed[i] = resampled[i];
        }
    } else {
        // 无需重采样，直接复制
        int copyLen = std::min(TARGET_SAMPLES, numSamples);
        for (int i = 0; i < copyLen; ++i) {
            processed[i] = readPtr[i];
        }
    }
    
    // 步骤2: 预加重 (提升高频)
    for (int i = TARGET_SAMPLES - 1; i > 0; --i) {
        processed[i] = processed[i] - static_cast<float>(PREEMPHASIS) * processed[i - 1];
    }
    
    // 步骤3: 归一化至 [-1, 1]
    float maxVal = 0.0f;
    for (int i = 0; i < TARGET_SAMPLES; ++i) {
        maxVal = std::max(maxVal, std::abs(processed[i]));
    }
    if (maxVal > 1e-8f) {
        float invMax = 1.0f / maxVal;
        for (int i = 0; i < TARGET_SAMPLES; ++i) {
            processed[i] *= invMax;
        }
    }
    
    return processed;
}

// =============================================================================
// Mel 频谱图计算 (简化版: 频带能量近似)
// =============================================================================

std::vector<float> AudioTimbreAnalyzer::computeMelSpectrogram(
    const std::vector<float>& audio) {
    
    // 输出: 64帧 × 64个Mel频带 = 4096 个值
    std::vector<float> melSpec(N_MELS * N_MEL_FRAMES, 0.0f);
    
    int frameSize = TARGET_SAMPLES / N_MEL_FRAMES;  // 16384 / 64 = 256
    
    for (int frame = 0; frame < N_MEL_FRAMES; ++frame) {
        int start = frame * frameSize;
        
        for (int band = 0; band < N_MELS; ++band) {
            // Mel 频带中心频率: 20Hz ~ 20kHz, 对数分布
            double centerFreq = 20.0 * std::pow(1000.0, static_cast<double>(band) / N_MELS);
            double normFreq = centerFreq / (TARGET_SR / 2.0);
            
            // 简易带通滤波器: 使用二阶谐振器近似
            float energy = 0.0f;
            float y1 = 0.0f, y2 = 0.0f;
            
            for (int i = 0; i < frameSize && (start + i) < TARGET_SAMPLES; ++i) {
                float sample = audio[start + i];
                // 二阶谐振器: y[n] = x[n] - a1*y[n-1] - a2*y[n-2]
                double omega = 2.0 * 3.14159265358979323846 * normFreq;
                double r = 0.95;  // 极点半径 (带宽控制)
                double a1 = -2.0 * r * std::cos(omega);
                double a2 = r * r;
                
                float y = sample - static_cast<float>(a1) * y1 - static_cast<float>(a2) * y2;
                y2 = y1;
                y1 = y;
                energy += y * y;
            }
            
            energy = std::sqrt(energy / frameSize + 1e-8f);
            melSpec[frame * N_MELS + band] = std::log(energy + 1e-8f);
        }
    }
    
    // 标准化 (零均值, 单位方差)
    float mean = 0.0f;
    for (float v : melSpec) mean += v;
    mean /= static_cast<float>(melSpec.size());
    
    float variance = 0.0f;
    for (float v : melSpec) {
        float diff = v - mean;
        variance += diff * diff;
    }
    variance /= static_cast<float>(melSpec.size());
    float stdDev = std::sqrt(variance + 1e-8f);
    
    for (float& v : melSpec) {
        v = (v - mean) / stdDev;
    }
    
    return melSpec;
}

// =============================================================================
// 核心推理
// =============================================================================

AudioTimbreAnalyzer::AnalysisResult AudioTimbreAnalyzer::analyze(
    const juce::AudioBuffer<float>& audio,
    double sampleRate,
    const std::string& /*textPrompt*/) {
    
    AnalysisResult result;
    
    if (!isLoaded()) {
        result.errorMessage = "Models not loaded. Call loadModels() first.";
        return result;
    }
    
    if (audio.getNumSamples() == 0) {
        result.errorMessage = "Empty audio buffer.";
        return result;
    }
    
#ifdef LIANCORE_HAS_ONNX
    try {
        // 步骤1: 预处理音频
        auto waveform = preprocessAudio(audio, sampleRate);
        
        // 步骤2: 计算 Mel 频谱图
        auto melSpec = computeMelSpectrogram(waveform);
        
        // 步骤3: 准备 ONNX 输入张量
        Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(
            OrtArenaAllocator, OrtMemTypeDefault);
        
        // 波形输入: [1, 16384]
        std::vector<int64_t> waveformShape = {1, TARGET_SAMPLES};
        Ort::Value waveformTensor = Ort::Value::CreateTensor<float>(
            memoryInfo, waveform.data(), waveform.size(),
            waveformShape.data(), waveformShape.size());
        
        // Mel 频谱输入: [1, 1, 64, 64]
        std::vector<int64_t> melShape = {1, 1, N_MELS, N_MEL_FRAMES};
        Ort::Value melTensor = Ort::Value::CreateTensor<float>(
            memoryInfo, melSpec.data(), melSpec.size(),
            melShape.data(), melShape.size());
        
        // 步骤4: 运行音频编码器推理
        const char* audioInputNames[] = {"waveform", "mel_spec"};
        const char* audioOutputNames[] = {"audio_embedding"};
        
        std::vector<Ort::Value> audioInputs;
        audioInputs.push_back(std::move(waveformTensor));
        audioInputs.push_back(std::move(melTensor));
        
        auto audioOutputs = audioEncoderSession_->Run(
            Ort::RunOptions{nullptr},
            audioInputNames, audioInputs.data(), 2,
            audioOutputNames, 1);
        
        // 提取音频嵌入
        float* audioEmbData = audioOutputs[0].GetTensorMutableData<float>();
        auto audioEmbShape = audioOutputs[0].GetTensorTypeAndShapeInfo().GetShape();
        int embSize = static_cast<int>(audioEmbShape[1]);
        result.audioEmbedding.assign(audioEmbData, audioEmbData + embSize);
        
        // 步骤5: 准备文本嵌入 (当前使用零向量)
        std::vector<float> textEmbedding(128, 0.0f);
        
        // 步骤6: 运行参数回归器推理
        std::vector<int64_t> embShape = {1, 128};
        Ort::Value audioEmbTensor = Ort::Value::CreateTensor<float>(
            memoryInfo, result.audioEmbedding.data(), 128,
            embShape.data(), embShape.size());
        Ort::Value textEmbTensor = Ort::Value::CreateTensor<float>(
            memoryInfo, textEmbedding.data(), 128,
            embShape.data(), embShape.size());
        
        const char* regInputNames[] = {"audio_embedding", "text_embedding"};
        const char* regOutputNames[] = {"parameters"};
        
        std::vector<Ort::Value> regInputs;
        regInputs.push_back(std::move(audioEmbTensor));
        regInputs.push_back(std::move(textEmbTensor));
        
        auto regOutputs = paramRegressorSession_->Run(
            Ort::RunOptions{nullptr},
            regInputNames, regInputs.data(), 2,
            regOutputNames, 1);
        
        // 提取参数
        float* paramData = regOutputs[0].GetTensorMutableData<float>();
        auto paramShape = regOutputs[0].GetTensorTypeAndShapeInfo().GetShape();
        int numParams = static_cast<int>(paramShape[1]);
        result.parameters.assign(paramData, paramData + numParams);
        
        // 步骤7: 计算置信度
        result.confidence = computeConfidence(result.parameters);
        
    } catch (const Ort::Exception& e) {
        result.errorMessage = std::string("ONNX inference error: ") + e.what();
    } catch (const std::exception& e) {
        result.errorMessage = std::string("Error: ") + e.what();
    }
#else
    result.errorMessage = "ONNX Runtime not available. Audio analysis requires ONNX.";
    (void)audio;
    (void)sampleRate;
#endif
    
    return result;
}

// =============================================================================
// 置信度计算
// =============================================================================

float AudioTimbreAnalyzer::computeConfidence(const std::vector<float>& params) {
    if (params.size() != 11) return 0.0f;
    
    float confidence = 1.0f;
    
    // 检查是否有过多极端值 (接近 0 或 1 的参数)
    int extremeCount = 0;
    for (float p : params) {
        if (p < 0.01f || p > 0.99f) {
            extremeCount++;
        }
    }
    confidence -= static_cast<float>(extremeCount) * 0.05f;
    
    // 检查参数分布的合理性: 所有参数不应完全相同
    float minVal = *std::min_element(params.begin(), params.end());
    float maxVal = *std::max_element(params.begin(), params.end());
    if (maxVal - minVal < 0.01f) {
        confidence -= 0.3f;
    }
    
    return std::max(0.0f, std::min(1.0f, confidence));
}

// =============================================================================
// 无ONNX回退: 频谱特征提取 → 合成器参数映射
// =============================================================================

AudioTimbreAnalyzer::AnalysisResult AudioTimbreAnalyzer::analyzeFallback(
    const juce::AudioBuffer<float>& audio, double sampleRate) {
    
    AnalysisResult result;
    result.parameters.resize(11, 0.5f);
    
    if (audio.getNumSamples() == 0) {
        result.errorMessage = "Empty audio buffer.";
        return result;
    }
    
    const float* data = audio.getReadPointer(0);
    int numSamples = audio.getNumSamples();
    
    // ---- 特征1: RMS (响度) ----
    float rms = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        rms += data[i] * data[i];
    }
    rms = std::sqrt(rms / numSamples);
    
    // ---- 特征2: 频谱质心 (亮度) ----
    // 使用FFT计算频谱
    int fftSize = 1;
    while (fftSize < numSamples) fftSize <<= 1;
    if (fftSize > 16384) fftSize = 16384;
    
    juce::dsp::FFT fft(std::log2(fftSize));
    std::vector<float> fftData(fftSize * 2, 0.0f);
    for (int i = 0; i < std::min(numSamples, fftSize); ++i) {
        fftData[i * 2] = data[i];
    }
    fft.performRealOnlyForwardTransform(fftData.data(), true);
    
    // 计算频谱质心 (加权平均频率)
    float spectralCentroid = 0.0f;
    float totalMag = 0.0f;
    int numBins = fftSize / 2;
    for (int i = 1; i < numBins; ++i) {
        float mag = std::sqrt(fftData[i * 2] * fftData[i * 2] + fftData[i * 2 + 1] * fftData[i * 2 + 1]);
        float freq = static_cast<float>(i) * static_cast<float>(sampleRate) / fftSize;
        spectralCentroid += freq * mag;
        totalMag += mag;
    }
    if (totalMag > 0.0f) spectralCentroid /= totalMag;
    float normalizedCentroid = spectralCentroid / (static_cast<float>(sampleRate) / 2.0f);
    
    // ---- 特征3: 高频能量比 (>5kHz) ----
    float highFreqEnergy = 0.0f;
    float lowFreqEnergy = 0.0f;
    int highFreqBin = static_cast<int>(5000.0f * fftSize / sampleRate);
    for (int i = 1; i < numBins; ++i) {
        float mag = std::sqrt(fftData[i * 2] * fftData[i * 2] + fftData[i * 2 + 1] * fftData[i * 2 + 1]);
        if (i > highFreqBin) {
            highFreqEnergy += mag;
        } else {
            lowFreqEnergy += mag;
        }
    }
    float highFreqRatio = (highFreqEnergy + lowFreqEnergy > 0.0f) ?
        highFreqEnergy / (highFreqEnergy + lowFreqEnergy) : 0.0f;
    
    // ---- 特征4: 谐波丰富度 (峰值数) ----
    int peakCount = 0;
    for (int i = 3; i < numBins - 1; ++i) {
        float mag = std::sqrt(fftData[i * 2] * fftData[i * 2] + fftData[i * 2 + 1] * fftData[i * 2 + 1]);
        float magPrev = std::sqrt(fftData[(i-1) * 2] * fftData[(i-1) * 2] + fftData[(i-1) * 2 + 1] * fftData[(i-1) * 2 + 1]);
        float magNext = std::sqrt(fftData[(i+1) * 2] * fftData[(i+1) * 2] + fftData[(i+1) * 2 + 1] * fftData[(i+1) * 2 + 1]);
        if (mag > magPrev * 1.5f && mag > magNext * 1.5f && mag > 0.01f) {
            peakCount++;
        }
    }
    float harmonicRichness = std::min(1.0f, peakCount / 30.0f);
    
    // ---- 特征5: 瞬态/冲击性 (零交叉率) ----
    int zeroCrossings = 0;
    for (int i = 1; i < numSamples; ++i) {
        if ((data[i] >= 0.0f && data[i - 1] < 0.0f) ||
            (data[i] < 0.0f && data[i - 1] >= 0.0f)) {
            zeroCrossings++;
        }
    }
    float zcr = static_cast<float>(zeroCrossings) / numSamples;
    float attack = zcr * 2.0f; // 高零交叉率 → 高Attack
    
    // ---- 特征6: 低频能量 (温暖度) ----
    int lowFreqBin = static_cast<int>(250.0f * fftSize / sampleRate);
    float lowEnergy = 0.0f;
    for (int i = 1; i <= lowFreqBin; ++i) {
        float mag = std::sqrt(fftData[i * 2] * fftData[i * 2] + fftData[i * 2 + 1] * fftData[i * 2 + 1]);
        lowEnergy += mag;
    }
    float warmth = (totalMag > 0.0f) ? lowEnergy / totalMag : 0.5f;
    
    // ---- 映射到11个合成器参数 ----
    // 参数顺序: [0]波形类型, [1]滤波器截止, [2]滤波器共振, [3]包络Attack,
    //           [4]包络Decay, [5]包络Sustain, [6]包络Release,
    //           [7]调制深度, [8]混响量, [9]延迟量, [10]失真量
    
    // [0] 波形类型: 基于谐波丰富度 (0=正弦, 1=锯齿)
    result.parameters[0] = harmonicRichness;
    
    // [1] 滤波器截止: 基于频谱质心
    result.parameters[1] = std::max(0.1f, std::min(0.95f, normalizedCentroid * 2.5f));
    
    // [2] 滤波器共振: 基于峰值数
    result.parameters[2] = harmonicRichness * 0.8f;
    
    // [3] Attack: 基于零交叉率/瞬态
    result.parameters[3] = std::max(0.0f, std::min(1.0f, 1.0f - attack));
    
    // [4] Decay: 中等
    result.parameters[4] = 0.5f;
    
    // [5] Sustain: 基于RMS
    result.parameters[5] = std::min(1.0f, rms * 3.0f);
    
    // [6] Release: 基于高频能量 (高频多→短Release)
    result.parameters[6] = std::max(0.1f, 1.0f - highFreqRatio * 0.8f);
    
    // [7] 调制深度: 基于谐波丰富度
    result.parameters[7] = harmonicRichness * 0.7f;
    
    // [8] 混响量: 基于Sustain
    result.parameters[8] = 0.3f + warmth * 0.4f;
    
    // [9] 延迟量: 基于Attack
    result.parameters[9] = 0.2f + (1.0f - attack) * 0.3f;
    
    // [10] 失真量: 基于高频能量
    result.parameters[10] = highFreqRatio * 0.6f;
    
    result.audioEmbedding.resize(128, 0.0f);
    result.confidence = 0.7f; // 回退模式置信度固定
    
    return result;
}

// =============================================================================
// 参数名称/描述
// =============================================================================

juce::String AudioTimbreAnalyzer::getParamName(int index) {
    static const char* names[] = {
        "波形类型", "滤波器截止", "滤波器共振",
        "Attack", "Decay", "Sustain", "Release",
        "调制深度", "混响量", "延迟量", "失真量"
    };
    return (index >= 0 && index < 11) ? names[index] : "Unknown";
}

juce::String AudioTimbreAnalyzer::getParamDescription(int index) {
    static const char* descs[] = {
        "0=正弦波, 0.5=方波, 1=锯齿波",
        "20Hz ~ 20kHz 低通滤波器截止频率",
        "滤波器共振Q值",
        "包络起音时间",
        "包络衰减时间",
        "包络保持电平",
        "包络释放时间",
        "LFO/包络调制深度",
        "混响效果发送量",
        "延迟效果发送量",
        "失真/饱和量"
    };
    return (index >= 0 && index < 11) ? descs[index] : "Unknown";
}

} // namespace AI
} // namespace LianCore