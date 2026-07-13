// =============================================================================
// LianCore - PluginProcessor 实现
// =============================================================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace LianCore {

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameterTree_(*this)
    , mpeInstrument_()
{
    // 初始化默认音频图
    initializeDefaultGraph();

    // Gamma: 自动加载 ONNX AI 模型
    initializeAI();

    // 启用 MPE 支持 (Zone 1: 主通道 1, 成员通道 2-15)
    enableMPE(true);
}

PluginProcessor::~PluginProcessor() = default;

// =============================================================================
// AudioProcessor 接口
// =============================================================================
const juce::String PluginProcessor::getName() const {
    return "LianCore";
}

bool PluginProcessor::acceptsMidi() const {
    return true;
}

bool PluginProcessor::producesMidi() const {
    return false;
}

double PluginProcessor::getTailLengthSeconds() const {
    return 0.0;
}

bool PluginProcessor::hasEditor() const {
    return true;
}

juce::AudioProcessorEditor* PluginProcessor::createEditor() {
    return new PluginEditor(*this);
}

// =============================================================================
// 程序管理
// =============================================================================
int PluginProcessor::getNumPrograms() {
    return 1;
}

int PluginProcessor::getCurrentProgram() {
    return currentProgram_;
}

void PluginProcessor::setCurrentProgram(int index) {
    currentProgram_ = index;
}

const juce::String PluginProcessor::getProgramName(int index) {
    return "Default";
}

void PluginProcessor::changeProgramName(int, const juce::String&) {
}

// =============================================================================
// 状态保存/恢复
// =============================================================================
void PluginProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto json = audioGraph_.toJson();
    juce::MemoryOutputStream stream(destData, true);
    json.writeToStream(stream);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes) {
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    auto json = juce::JSON::parse(stream);
    audioGraph_.fromJson(json);
}

// =============================================================================
// 总线布局
// =============================================================================
bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    // 支持立体声输出
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) {
        return false;
    }
    // 不支持音频输入
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled()) {
        return false;
    }
    return true;
}

// =============================================================================
// 音频处理
// =============================================================================
void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    audioGraph_.prepareToPlay(sampleRate, samplesPerBlock);
}

void PluginProcessor::releaseResources() {
    audioGraph_.releaseResources();
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;

    auto startTime = juce::Time::getMillisecondCounterHiRes();

    // MPE: 处理 MPE 区域设置消息 (通道 1 上的 RPN/NRPN)
    if (mpeEnabled_) {
        for (const auto metadata : midi) {
            mpeInstrument_.processNextMidiEvent(metadata.getMessage());
        }
    }

    // 清除输出缓冲区
    for (int ch = 0; ch < getTotalNumOutputChannels(); ++ch) {
        buffer.clear(ch, 0, buffer.getNumSamples());
    }

    // 处理音频图
    audioGraph_.processBlock(buffer, midi);

    // 处理调制矩阵
    modulationMatrix_.processBlock(buffer.getNumSamples());

    // 更新CPU使用率
    auto elapsed = juce::Time::getMillisecondCounterHiRes() - startTime;
    currentCpuUsage_ = elapsed;
}

// =============================================================================
// 默认音频图初始化 (Alpha阶段: 基础信号链)
// =============================================================================
void PluginProcessor::initializeDefaultGraph() {
    // 创建节点: 波表振荡器 → 滤波器 → 音频输出
    oscNodeId_ = audioGraph_.addNode(NodeType::WavetableOscillator, "OSC 1");
    filterNodeId_ = audioGraph_.addNode(NodeType::Filter, "Filter");
    outputNodeId_ = audioGraph_.addNode(NodeType::AudioOutput, "Output");

    // 创建调制器
    envNodeId_ = audioGraph_.addNode(NodeType::Envelope, "Amp Env");
    lfoNodeId_ = audioGraph_.addNode(NodeType::LFO, "LFO 1");

    // 连接信号链: OSC → Filter → Output
    audioGraph_.connect(oscNodeId_, 0, filterNodeId_, 0);
    audioGraph_.connect(filterNodeId_, 0, outputNodeId_, 0);

    // 注册调制源
    if (auto* env = dynamic_cast<ModulationSource*>(audioGraph_.getNode(envNodeId_))) {
        modulationMatrix_.registerSource("env", env);
    }
    if (auto* lfo = dynamic_cast<ModulationSource*>(audioGraph_.getNode(lfoNodeId_))) {
        modulationMatrix_.registerSource("lfo", lfo);
    }

    // 注册调制目标
    if (auto* osc = dynamic_cast<ModulationTarget*>(audioGraph_.getNode(oscNodeId_))) {
        modulationMatrix_.registerTarget("osc_freq", osc);
    }
    if (auto* filter = dynamic_cast<ModulationTarget*>(audioGraph_.getNode(filterNodeId_))) {
        modulationMatrix_.registerTarget("filter_cutoff", filter);
    }

    // 默认调制路由: LFO → 滤波器截止频率
    modulationMatrix_.addModulation("lfo", "filter_cutoff", 0.3f);
    modulationMatrix_.addModulation("env", "osc_freq", 0.1f);
}

// =============================================================================
// 性能监控
// =============================================================================
double PluginProcessor::getCpuUsage() const {
    return currentCpuUsage_;
}

size_t PluginProcessor::getMemoryUsage() const {
    return audioGraph_.getTotalMemoryUsage();
}

// =============================================================================
// Gamma: ONNX AI 模型自动加载
// =============================================================================

juce::File PluginProcessor::getModelDirectory() const {
    // 1. 尝试从插件二进制文件所在目录查找
    //    (JUCE 插件在不同宿主中以不同方式加载)
    // 2. 回退到当前工作目录
    // 3. 回退到项目根目录

    juce::File binaryDir = juce::File::getSpecialLocation(
        juce::File::currentExecutableFile).getParentDirectory();

    // 检查 models/ 子目录
    juce::File modelsDir = binaryDir.getChildFile("models");
    if (modelsDir.exists() && modelsDir.getChildFile("liancore_ai_model.onnx").exists()) {
        return modelsDir;
    }

    // 回退: 当前工作目录
    juce::File cwd = juce::File::getCurrentWorkingDirectory();
    juce::File cwdModels = cwd.getChildFile("models");
    if (cwdModels.exists() && cwdModels.getChildFile("liancore_ai_model.onnx").exists()) {
        return cwdModels;
    }

    // 回退: 项目根目录 (开发模式)
    juce::File projectRoot = cwd;
    while (projectRoot.getParentDirectory().exists() && !projectRoot.isRoot()) {
        if (projectRoot.getChildFile("models").exists() &&
            projectRoot.getChildFile("models").getChildFile("liancore_ai_model.onnx").exists()) {
            return projectRoot.getChildFile("models");
        }
        if (projectRoot.getFileName() == "LianCore") {
            return projectRoot.getChildFile("models");
        }
        projectRoot = projectRoot.getParentDirectory();
    }

    return modelsDir; // 返回默认路径
}

void PluginProcessor::initializeAI() {
    juce::File modelsDir = getModelDirectory();

    // 1. 加载 MLP 参数预测模型
    juce::File modelPath = modelsDir.getChildFile("liancore_ai_model.onnx");
    if (modelPath.existsAsFile()) {
        juce::String modelPathStr = modelPath.getFullPathName();
        bool loaded = aiEngine_.loadOnnxModel(modelPathStr.toStdString());

        if (loaded) {
            DBG("LianCore: ONNX AI model loaded from " << modelPathStr);
            DBG("  Model info: " << aiEngine_.getModelInfo());
        } else {
            DBG("LianCore: Failed to load ONNX model from " << modelPathStr);
            DBG("  Falling back to rule-based inference");
        }
    } else {
        DBG("LianCore: ONNX model not found at " << modelPath.getFullPathName());
        DBG("  Using rule-based inference as fallback");
    }

    // 2. Gamma: 加载 Transformer 文本编码器
    juce::File tokenizerPath = modelsDir.getChildFile("tokenizer").getChildFile("tokenizer.model");
    juce::File transformerPath = modelsDir.getChildFile("transformer_encoder.onnx");

    if (tokenizerPath.existsAsFile() && transformerPath.existsAsFile()) {
        bool transformerLoaded = aiEngine_.loadTransformerModel(tokenizerPath, transformerPath);
        if (transformerLoaded) {
            DBG("LianCore: Transformer text encoder loaded");
            DBG("  Vocab size: " << aiEngine_.getTransformerEncoder().getVocabSize());
        } else {
            DBG("LianCore: Transformer encoder load failed, using char-hash fallback");
        }
    } else {
        DBG("LianCore: Transformer model files not found");
        DBG("  Tokenizer: " << tokenizerPath.getFullPathName() << " exists=" << (int)tokenizerPath.existsAsFile());
        DBG("  Transformer: " << transformerPath.getFullPathName() << " exists=" << (int)transformerPath.existsAsFile());
    }
}

bool PluginProcessor::isAIModelLoaded() const {
    return aiEngine_.isModelLoaded();
}

// =============================================================================
// MPE (MIDI Polyphonic Expression) 支持
// =============================================================================

void PluginProcessor::enableMPE(bool enable) {
    if (enable == mpeEnabled_) return;

    mpeEnabled_ = enable;

    if (enable) {
        // 配置 MPE Zone 1:
        //   主通道: 1 (MIDI通道 1)
        //   成员通道: 2-15 (14个复音通道)
        //   弯音范围: ±48半音 (标准MPE)
        juce::MPEZoneLayout layout;
        layout.setLowerZone(15, 48, 2);
        mpeInstrument_.setZoneLayout(layout);

        DBG("LianCore: MPE enabled (Zone 1: master ch=1, member ch=2-15, pitchbend=±48)");
    } else {
        mpeInstrument_.setZoneLayout(juce::MPEZoneLayout());
        DBG("LianCore: MPE disabled");
    }
}

bool PluginProcessor::isMPEEnabled() const {
    return mpeEnabled_;
}

} // namespace LianCore

// =============================================================================
// JUCE 插件创建函数
// =============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new LianCore::PluginProcessor();
}