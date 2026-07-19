// =============================================================================
// LianCore - SimpleHTTPServer 嵌入式HTTP静态文件服务器
// 在 VST3 插件内部运行，为 WebBrowserComponent 提供 Web UI 静态文件
// 端口 9000，从 VST3 Bundle 的 Contents/Resources/ui/ 目录提供文件
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include <atomic>

namespace LianCore {

class SimpleHTTPServer : private juce::Thread {
public:
    SimpleHTTPServer();
    ~SimpleHTTPServer() override;

    // 启动服务器
    // port: 监听端口 (默认 9000)
    // rootDir: 静态文件根目录 (VST3 Bundle 的 Contents/Resources/ui/)
    bool start(int port, const juce::File& rootDir);

    // 停止服务器
    void stop();

    // 是否正在运行
    bool isRunning() const { return running_; }

    // 获取端口号
    int getPort() const { return port_; }

private:
    void run() override;

    // 处理单个 HTTP 客户端请求
    void handleClient(juce::StreamingSocket& client);

    // 根据文件扩展名获取 MIME 类型
    static juce::String getMimeType(const juce::String& filename);

    // 构建 HTTP 响应
    static juce::String buildHttpResponse(
        int statusCode,
        const juce::String& contentType,
        const juce::MemoryBlock& body);

    // 读取文件内容
    static juce::MemoryBlock readFileContent(const juce::File& file);

    int port_ = 9000;
    juce::File rootDir_;
    std::unique_ptr<juce::StreamingSocket> serverSocket_;
    std::atomic<bool> running_{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimpleHTTPServer)
};

} // namespace LianCore