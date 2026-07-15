// =============================================================================
// LianCore - 音频音色分析器 (Audio Timbre Analyzer)
// Gamma Week 3-4: 音频参考音色复刻
// 输入 WAV 音频 → 提取音频嵌入 → 预测合成器参数
// =============================================================================

#pragma once

#include <JuceHeader.h>
#ifdef LIANCORE_HAS_ONNX
#include <onnxruntime_cxx_api.h>
#else
#include "OnnxStubs.h"
#endif
#include <vector>
#include <string>
#include <memory>

namespace LianCore {
namespace AI {

/**
 * 音频音色分析器
 * 
 * 功能: 用户拖入 WAV 音频片段 → 自动提取音色特征 → 预测 11 个合成器参数
 * 
 * 架构:
 *   - 音频编码器 (ONNX): 波形 1D CNN + Mel 频谱 2D CNN → 128维音频嵌入
 *   - 参数回归器 (ONNX): 音频嵌入(128) + 文本嵌入(128) → 11维参数 [0,1]
 * 
 * 使用示例:
 *   AudioTimbreAnalyzer analyzer;
 *   analyzer.loadModels(encoderPath, regressorPath);
 *   auto result = analyzer.analyze(audioBuffer, 44100.0);
 *   if (result.isValid()) {
 *       applyParameters(result.parameters);
 *   }
 */
class AudioTimbreAnalyzer {
public:
    /** 分析结果 */
    struct AnalysisResult {
        std::vector<float> parameters;      // 11个合成器参数 [0.0-1.0]
        std::vector<float> audioEmbedding;  // 128维音频嵌入
        float confidence = 0.0f;            // 置信度 [0.0-1.0]
        std::string errorMessage;
        
        bool isValid() const {
            return errorMessage.empty() && parameters.size() == 11 && confidence >= 0.0f;
        }
    };
    
    AudioTimbreAnalyzer();
    ~AudioTimbreAnalyzer();
    
    /** 加载 ONNX 模型 (音频编码器 + 参数回归器) */
    bool loadModels(const juce::File& audioEncoderPath,
                    const juce::File& paramRegressorPath);
    
    /**
     * 分析音频并预测合成器参数
     * @param audio 输入音频缓冲区 (单声道/多声道，取第一声道)
     * @param sampleRate 原始采样率 (自动重采样至 44.1kHz)
     * @param textPrompt 可选文本辅助描述 (中文)
     * @return AnalysisResult 包含 11个参数和音频嵌入
     */
    AnalysisResult analyze(const juce::AudioBuffer<float>& audio,
                           double sampleRate,
                           const std::string& textPrompt = "");
    
    /** 是否已加载模型 */
    bool isLoaded() const {
        return audioEncoderLoaded_ && paramRegressorLoaded_;
    }
    
    /**
     * 无ONNX回退: 基于频谱特征提取直接分析
     * 提取 RMS, 频谱质心, 高频能量, 谐波丰富度等特征
     * 映射到合成器参数 (11个参数)
     */
    AnalysisResult analyzeFallback(const juce::AudioBuffer<float>& audio,
                                    double sampleRate);
    
    /** 常量 */
    static constexpr int getEmbeddingDim() { return 128; }
    static constexpr int getTargetSampleRate() { return 44100.0; }
    static constexpr int getTargetSampleCount() { return 16384; }
    static constexpr int getNumParams() { return 11; }
    
    /** 参数名称映射 */
    static juce::String getParamName(int index);
    static juce::String getParamDescription(int index);
    
private:
    // ---- 预处理 ----
    std::vector<float> preprocessAudio(const juce::AudioBuffer<float>& audio,
                                        double sampleRate);
    std::vector<float> computeMelSpectrogram(const std::vector<float>& audio);
    
    // ---- 后处理 ----
    float computeConfidence(const std::vector<float>& params);
    
    // ---- ONNX Runtime ----
#ifdef LIANCORE_HAS_ONNX
    std::unique_ptr<Ort::Env> ortEnv_;
    std::unique_ptr<Ort::SessionOptions> sessionOptions_;
    std::unique_ptr<Ort::Session> audioEncoderSession_;
    std::unique_ptr<Ort::Session> paramRegressorSession_;
#endif
    
    bool audioEncoderLoaded_ = false;
    bool paramRegressorLoaded_ = false;
    
    // ---- 常量 ----
    static constexpr int TARGET_SAMPLES = 16384;
    static constexpr double TARGET_SR = 44100.0;
    static constexpr double PREEMPHASIS = 0.97;
    static constexpr int N_MELS = 64;
    static constexpr int N_MEL_FRAMES = 64;  // 16384 / 256
    static constexpr int N_FFT = 1024;
    static constexpr int HOP_LENGTH = 256;
};

} // namespace AI
} // namespace LianCore