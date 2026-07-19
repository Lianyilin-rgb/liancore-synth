// =============================================================================
// LianCore - PluginEditor VST3插件编辑器 (WebBrowserComponent + 内置HTTP服务器)
// 嵌入 JUCE WebBrowserComponent 加载 Web UI，通过内置 HTTP 服务器提供静态文件
// WebSocket 端口 9001 用于 UI ↔ C++ 核心实时通信
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "SimpleHTTPServer.h"
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
// PluginEditor (WebBrowserComponent 嵌入式 Web UI)
// 架构：
//   PluginEditor → WebBrowserComponent → http://localhost:9000/index.html
//                      ↑
//             SimpleHTTPServer (端口 9000, 服务 VST3 Bundle 内静态文件)
//                      ↑
//             UIMessageServer (端口 9001, WebSocket 实时通信)
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

    // 嵌入式 Web 浏览器组件 (Windows: WebView2/Edge, macOS: WKWebView)
    std::unique_ptr<juce::WebBrowserComponent> webBrowser_;

    // 内置 HTTP 静态文件服务器 (端口 9000)
    // 从 VST3 Bundle 的 Contents/Resources/ui/ 目录提供 Web UI 文件
    std::unique_ptr<SimpleHTTPServer> httpServer_;

    // WebSocket 消息服务器 (端口 9001)
    // Web UI ↔ C++ 核心 实时双向通信
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

    // 获取 VST3 Bundle 内 UI 资源目录路径
    // 返回 Contents/Resources/ui/ 的 File 对象
    static juce::File getBundleUIPath();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace LianCore