// =============================================================================
// LianCore - PluginEditor VST3插件编辑器 (Beta阶段: Web UI + WebSocket)
// 集成React前端UI, 通过WebSocket与C++核心通信
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "../ui/WavetableEditor.h"
#include "../ui/MPERecordingUI.h"
#include "../ui/GranularEngineUI.h"
#include <unordered_map>
#include <functional>

namespace LianCore {

// =============================================================================
// 简单WebSocket服务器 (用于与Web UI通信)
// =============================================================================
class UIMessageServer : private juce::Thread {
public:
    using MessageHandler = std::function<void(const juce::var&)>;

    UIMessageServer();
    ~UIMessageServer() override;

    void start(int port = 9001);
    void stop();

    // 发送消息到Web UI
    void sendToUI(const juce::String& type, const juce::var& payload);

    // 注册消息处理
    void onMessage(const juce::String& type, MessageHandler handler);

    // 广播CPU/内存状态
    void broadcastStatus(double cpuMs, size_t memoryBytes);

private:
    void run() override;

    juce::StreamingSocket socket_;
    bool running_ = false;
    int port_ = 9001;

    struct ClientConnection {
        std::unique_ptr<juce::StreamingSocket> socket;
        juce::String buffer;
    };
    std::vector<std::unique_ptr<ClientConnection>> clients_;

    std::unordered_map<std::string, MessageHandler> handlers_;
    juce::CriticalSection lock_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UIMessageServer)
};

// =============================================================================
// PluginEditor (Beta阶段)
// =============================================================================
class PluginEditor : public juce::AudioProcessorEditor,
                     private juce::Timer {
public:
    explicit PluginEditor(PluginProcessor& processor);
    ~PluginEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    PluginProcessor& processor_;

    // Beta阶段UI元素
    juce::Label titleLabel_;
    juce::Label statusLabel_;
    juce::Label cpuLabel_;
    juce::Label wsStatusLabel_;
    juce::TextButton openWebUIButton_;
    juce::TextButton aiTestButton_;
    juce::TextButton wavetableEditorButton_;
    juce::TextButton mpeRecordingButton_;
    juce::TextButton granularEngineButton_;
    juce::TextEditor aiPromptInput_;

    // 波表编辑器 (P2-2)
    WavetableEditor wavetableEditor_;
    bool wavetableEditorVisible_ = false;

    // MPE录制UI (P6-1)
    MPERecordingUI mpeRecordingUI_;
    bool mpeRecordingVisible_ = false;

    // 粒子合成引擎UI (P6-2)
    GranularEngineUI granularEngineUI_;
    bool granularEngineVisible_ = false;

    // WebSocket消息服务器
    UIMessageServer uiServer_;

    // 处理Web UI消息
    void setupMessageHandlers();

    // =========================================================================
    // Beta Week 8: morphTo 渐变 + ONNX 推理结果推送
    // =========================================================================

    // 推送 ONNX 模型状态到 Web UI
    void pushOnnxStatus();

    // 推送 morph 渐变进度到 Web UI
    void pushMorphProgress(float progress, const juce::String& status);

    // morph 渐变完成回调
    void onMorphComplete();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace LianCore