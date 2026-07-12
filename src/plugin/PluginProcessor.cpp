// =============================================================================
// LianCore - PluginProcessor 实现
// =============================================================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace LianCore {

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameterTree_(*this)
{
    // 初始化默认音频图
    initializeDefaultGraph();
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

} // namespace LianCore

// =============================================================================
// JUCE 插件创建函数
// =============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new LianCore::PluginProcessor();
}