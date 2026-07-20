// =============================================================================
// LianCore - PluginEditor VST3插件编辑器
// 正常模式: WebBrowserComponent + 内置HTTP服务器
// 安全模式: 原生 JUCE 组件（CJKLookAndFeel + 基本控件）
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#ifndef LIANCORE_SAFE_MODE
#include "SimpleHTTPServer.h"
#endif
#include <unordered_map>
#include <functional>

namespace LianCore {

// =============================================================================
// 简单WebSocket服务器 (用于与Web UI通信)
// 仅在非安全模式下编译
// =============================================================================
#ifndef LIANCORE_SAFE_MODE
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
#endif // !LIANCORE_SAFE_MODE

// =============================================================================
// PluginEditor
// 正常模式: WebBrowserComponent 嵌入式 Web UI
//   架构：SimpleHTTPServer(端口9000) → WebBrowserComponent → UIMessageServer(端口9001)
// 安全模式: 原生 JUCE 组件（CJKLookAndFeel + 参数标签+滑块）
//   不依赖 HTTP/WebSocket/WebView2，确保最稳定
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

#ifndef LIANCORE_SAFE_MODE
    // =========================================================================
    // 正常模式: Web 浏览器组件 + 服务器
    // =========================================================================

    // 嵌入式 Web 浏览器组件 (Windows: WebView2/Edge, macOS: WKWebView)
    std::unique_ptr<juce::WebBrowserComponent> webBrowser_;

    // 内置 HTTP 静态文件服务器 (端口 9000)
    // 从 VST3 Bundle 的 Contents/Resources/ui/ 目录提供 Web UI 文件
    std::unique_ptr<SimpleHTTPServer> httpServer_;

    // WebSocket 消息服务器 (端口 9001)
    // Web UI ↔ C++ 核心 实时双向通信
    UIMessageServer uiServer_;

    // 异步加载状态
    // URL 是否已加载（防止重复加载）
    std::atomic<bool> urlLoaded_{false};
    // 超时计数器（10Hz tick，50 次 = 5 秒）
    int timeoutCounter_ = 0;
    // 待加载的 Web UI URL
    juce::String webUIUrl_;
    // 回退模式：超时后显示原生 UI 提示
    bool fallbackMode_ = false;

    // 处理Web UI消息
    void setupMessageHandlers();

    // 推送 ONNX 模型状态到 Web UI
    void pushOnnxStatus();

    // 推送 morph 渐变进度到 Web UI
    void pushMorphProgress(float progress, const juce::String& status);

    // morph 渐变完成回调
    void onMorphComplete();

    // 获取 VST3 Bundle 内 UI 资源目录路径
    static juce::File getBundleUIPath();
#else
    // =========================================================================
    // 安全模式: 原生 JUCE UI 组件
    // 使用 CJKLookAndFeel 确保中文不乱码
    // =========================================================================

    // 状态标签
    std::unique_ptr<juce::Label> statusLabel_;
    std::unique_ptr<juce::Label> infoLabel_;

    // 核心参数滑块
    std::unique_ptr<juce::Slider> volumeSlider_;
    std::unique_ptr<juce::Label> volumeLabel_;

    // 自定义 CJK 兼容 LookAndFeel
    std::unique_ptr<juce::LookAndFeel> lookAndFeel_;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace LianCore