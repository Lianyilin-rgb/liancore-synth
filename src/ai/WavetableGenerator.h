// =============================================================================
// LianCore - 波表生成器 (Wavetable Generator)
// Gamma Week 5-6: 文本到波表合成
// 使用 WavetableVAE 解码器从文本嵌入生成波表 (256帧 × 2048采样)
// =============================================================================

#pragma once

#include <JuceHeader.h>
#include <onnxruntime_cxx_api.h>
#include <vector>
#include <string>
#include <memory>
#include <random>

namespace LianCore {
namespace AI {

/**
 * 波表生成器
 * 
 * 功能: 根据文本嵌入向量，使用 WavetableVAE 解码器生成波表
 * 
 * 架构:
 *   - WavetableVAE Decoder (ONNX): 潜在向量(64) + 文本嵌入(128) → 波表(256×2048)
 * 
 * 使用示例:
 *   WavetableGenerator generator;
 *   generator.loadModel(decoderPath);
 *   auto result = generator.generate(textEmbedding);
 *   if (result.isValid()) {
 *       // result.wavetable 包含 256×2048 个浮点值，范围 [-1, 1]
 *   }
 */
class WavetableGenerator {
public:
    /** 波表生成结果 */
    struct WavetableResult {
        std::vector<float> wavetable;   // 256帧 × 2048采样，行优先 [frame*samples + sample]
        int numFrames = 0;              // 帧数 (256)
        int tableSize = 0;              // 每帧采样数 (2048)
        std::string errorMessage;
        
        bool isValid() const {
            return errorMessage.empty() && numFrames == 256 && tableSize == 2048
                   && wavetable.size() == static_cast<size_t>(numFrames * tableSize);
        }
        
        /** 获取指定帧的波表数据 */
        const float* getFrame(int frameIndex) const {
            if (frameIndex < 0 || frameIndex >= numFrames) return nullptr;
            return &wavetable[frameIndex * tableSize];
        }
    };
    
    WavetableGenerator();
    ~WavetableGenerator();
    
    /**
     * 加载 WavetableVAE 解码器 ONNX 模型
     * @param onnxPath wavetable_vae_decoder.onnx 文件路径
     * @return true 如果加载成功
     */
    bool loadModel(const juce::File& onnxPath);
    
    /**
     * 生成波表
     * @param textEmbedding 128维文本嵌入向量 (如 Transformer 编码器输出)
     * @return WavetableResult 包含 256×2048 波表数据
     */
    WavetableResult generate(const std::vector<float>& textEmbedding);
    
    /** 是否已加载模型 */
    bool isLoaded() const { return modelLoaded_; }
    
    /** 常量 */
    static constexpr int getNumFrames() { return 256; }
    static constexpr int getTableSize() { return 2048; }
    static constexpr int getLatentDim() { return 64; }
    static constexpr int getTextDim() { return 128; }
    
private:
    /** 帧间平滑: 对相邻帧做线性插值混合，消除突变 */
    void applyFrameSmoothing(std::vector<float>& wavetable, float strength = 0.3f);
    
    /** 抗混叠后处理: 对每帧波表应用低通滤波 (去除奈奎斯特以上频率) */
    void applyAntiAliasing(std::vector<float>& wavetable);
    
    /** 随机数生成器 (用于采样潜在向量) */
    std::mt19937 rng_;
    std::normal_distribution<float> normalDist_;
    
    // ---- ONNX Runtime ----
    std::unique_ptr<Ort::Env> ortEnv_;
    std::unique_ptr<Ort::SessionOptions> sessionOptions_;
    std::unique_ptr<Ort::Session> session_;
    
    bool modelLoaded_ = false;
};

} // namespace AI
} // namespace LianCore