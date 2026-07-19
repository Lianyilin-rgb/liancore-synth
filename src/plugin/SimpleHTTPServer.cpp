// =============================================================================
// LianCore - SimpleHTTPServer 嵌入式HTTP静态文件服务器实现
// 在 VST3 插件内部运行，为 WebBrowserComponent 提供 Web UI 静态文件
// 端口 9000，从 VST3 Bundle 的 Contents/Resources/ui/ 目录提供文件
// =============================================================================
#include "SimpleHTTPServer.h"
#include <JuceHeader.h>

namespace LianCore {

// =============================================================================
// 构造与析构
// =============================================================================
SimpleHTTPServer::SimpleHTTPServer()
    : juce::Thread("LianCore-HTTP-Server") {
}

SimpleHTTPServer::~SimpleHTTPServer() {
    stop();
}

// =============================================================================
// 启动服务器
// =============================================================================
bool SimpleHTTPServer::start(int port, const juce::File& rootDir) {
    if (running_) {
        DBG("[LianCore-HTTP] 服务器已在运行中");
        return true;
    }

    port_ = port;
    rootDir_ = rootDir;

    if (!rootDir_.exists()) {
        DBG("[LianCore-HTTP] 静态文件根目录不存在: " << rootDir_.getFullPathName());
        // 不阻止启动，让 WebBrowserComponent 显示 404 也比崩溃好
    }

    serverSocket_ = std::make_unique<juce::StreamingSocket>();

    if (!serverSocket_->createListener(port_)) {
        DBG("[LianCore-HTTP] 端口 " << port_ << " 监听失败，尝试端口 9000");
        // 回退到默认端口
        port_ = 9000;
        if (!serverSocket_->createListener(port_)) {
            DBG("[LianCore-HTTP] 端口 9000 也监听失败");
            serverSocket_.reset();
            return false;
        }
    }

    running_ = true;
    startThread(juce::Thread::Priority::background);
    DBG("[LianCore-HTTP] 服务器启动成功，端口: " << port_ << "，根目录: " << rootDir_.getFullPathName());
    return true;
}

// =============================================================================
// 停止服务器
// =============================================================================
void SimpleHTTPServer::stop() {
    running_ = false;

    // 关闭监听 socket 以解除 waitForNextConnection 阻塞
    if (serverSocket_) {
        serverSocket_->close();
    }

    stopThread(2000);
    serverSocket_.reset();
    DBG("[LianCore-HTTP] 服务器已停止");
}

// =============================================================================
// 主循环：接受连接并处理
// =============================================================================
void SimpleHTTPServer::run() {
    while (running_ && !threadShouldExit()) {
        // 接受新连接（非阻塞，超时 100ms）
        if (auto* clientSocket = serverSocket_->waitForNextConnection(100)) {
            if (clientSocket->isConnected()) {
                handleClient(*clientSocket);
            }
            delete clientSocket;
        }
    }
}

// =============================================================================
// 处理单个 HTTP 客户端请求
// 解析 HTTP GET 请求，读取文件，发送响应
// =============================================================================
void SimpleHTTPServer::handleClient(juce::StreamingSocket& client) {
    // 设置读取超时（1秒）
    // 读取 HTTP 请求头
    char buffer[8192];
    int bytesRead = client.read(buffer, sizeof(buffer) - 1, false);

    if (bytesRead <= 0) {
        return;
    }

    buffer[bytesRead] = '\0';
    juce::String request(buffer, bytesRead);

    // 解析请求行：GET /path HTTP/1.1
    auto lines = juce::StringArray::fromLines(request);
    if (lines.isEmpty()) {
        return;
    }

    auto requestLine = lines[0];
    auto tokens = juce::StringArray::fromTokens(requestLine, " ", "\"");
    if (tokens.size() < 2) {
        return;
    }

    juce::String method = tokens[0];
    juce::String path = tokens[1];

    // 只支持 GET 请求
    if (method != "GET") {
        juce::String response = buildHttpResponse(405, "text/plain",
            juce::MemoryBlock("Method Not Allowed", 17));
        client.write(response.toRawUTF8(), response.getNumBytesAsUTF8());
        return;
    }

    // URL 解码路径（处理 %20 等）
    path = juce::URL::removeEscapeChars(path);

    // 安全检查：防止路径遍历攻击
    if (path.contains("..")) {
        juce::String response = buildHttpResponse(403, "text/plain",
            juce::MemoryBlock("Forbidden", 9));
        client.write(response.toRawUTF8(), response.getNumBytesAsUTF8());
        return;
    }

    // 默认文件：根路径 → index.html
    if (path == "/" || path.isEmpty()) {
        path = "/index.html";
    }

    // 去除前导 /
    if (path.startsWith("/")) {
        path = path.substring(1);
    }

    // 构建文件路径
    juce::File file = rootDir_.getChildFile(path);

    if (!file.existsAsFile()) {
        // 尝试 SPA 回退：非文件路径 → index.html
        // 这是为了支持 React Router 等前端路由
        juce::File indexFile = rootDir_.getChildFile("index.html");
        if (indexFile.existsAsFile()) {
            auto content = readFileContent(indexFile);
            juce::String response = buildHttpResponse(200, "text/html; charset=utf-8", content);
            client.write(response.toRawUTF8(), response.getNumBytesAsUTF8());
            return;
        }

        juce::String body = "404 Not Found: " + path;
        juce::String response = buildHttpResponse(404, "text/plain; charset=utf-8",
            juce::MemoryBlock(body.toRawUTF8(), body.getNumBytesAsUTF8()));
        client.write(response.toRawUTF8(), response.getNumBytesAsUTF8());
        return;
    }

    // 读取文件内容
    auto content = readFileContent(file);
    juce::String mimeType = getMimeType(file.getFileName());

    // 对于文本类文件，添加 charset=utf-8
    if (mimeType.startsWith("text/") || mimeType == "application/javascript"
        || mimeType == "application/json") {
        mimeType += "; charset=utf-8";
    }

    juce::String response = buildHttpResponse(200, mimeType, content);
    client.write(response.toRawUTF8(), response.getNumBytesAsUTF8());
}

// =============================================================================
// 根据文件扩展名获取 MIME 类型
// =============================================================================
juce::String SimpleHTTPServer::getMimeType(const juce::String& filename) {
    juce::String ext = filename.fromLastOccurrenceOf(".", false, false).toLowerCase();

    if (ext == "html" || ext == "htm") return "text/html";
    if (ext == "css")                    return "text/css";
    if (ext == "js")                     return "application/javascript";
    if (ext == "mjs")                    return "application/javascript";
    if (ext == "json")                   return "application/json";
    if (ext == "xml")                    return "application/xml";
    if (ext == "svg")                    return "image/svg+xml";
    if (ext == "png")                    return "image/png";
    if (ext == "jpg" || ext == "jpeg")   return "image/jpeg";
    if (ext == "gif")                    return "image/gif";
    if (ext == "ico")                    return "image/x-icon";
    if (ext == "webp")                   return "image/webp";
    if (ext == "woff")                   return "font/woff";
    if (ext == "woff2")                  return "font/woff2";
    if (ext == "ttf")                    return "font/ttf";
    if (ext == "otf")                    return "font/otf";
    if (ext == "mp3")                    return "audio/mpeg";
    if (ext == "wav")                    return "audio/wav";
    if (ext == "ogg")                    return "audio/ogg";
    if (ext == "mp4")                    return "video/mp4";
    if (ext == "webm")                   return "video/webm";
    if (ext == "wasm")                   return "application/wasm";
    if (ext == "map")                    return "application/json";

    // 默认二进制流
    return "application/octet-stream";
}

// =============================================================================
// 构建 HTTP 响应
// =============================================================================
juce::String SimpleHTTPServer::buildHttpResponse(
    int statusCode,
    const juce::String& contentType,
    const juce::MemoryBlock& body) {
    juce::String statusText;
    switch (statusCode) {
        case 200: statusText = "OK"; break;
        case 403: statusText = "Forbidden"; break;
        case 404: statusText = "Not Found"; break;
        case 405: statusText = "Method Not Allowed"; break;
        default:  statusText = "Internal Server Error"; break;
    }

    juce::String response;
    response << "HTTP/1.1 " << juce::String(statusCode) << " " << statusText << "\r\n";
    response << "Content-Type: " << contentType << "\r\n";
    response << "Content-Length: " << juce::String(static_cast<int>(body.getSize())) << "\r\n";
    // CORS 头：允许 WebBrowserComponent 跨域加载（不同端口）
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Access-Control-Allow-Methods: GET, OPTIONS\r\n";
    response << "Access-Control-Allow-Headers: Content-Type\r\n";
    response << "Connection: close\r\n";
    response << "Server: LianCore-HTTP/1.0\r\n";
    // 缓存控制：开发阶段禁用缓存，确保每次加载最新文件
    response << "Cache-Control: no-cache, no-store, must-revalidate\r\n";
    response << "Pragma: no-cache\r\n";
    response << "Expires: 0\r\n";
    response << "\r\n";

    // 追加响应体
    response += juce::String::fromUTF8(
        static_cast<const char*>(body.getData()),
        static_cast<int>(body.getSize()));

    return response;
}

// =============================================================================
// 读取文件内容到 MemoryBlock
// =============================================================================
juce::MemoryBlock SimpleHTTPServer::readFileContent(const juce::File& file) {
    juce::MemoryBlock result;

    juce::FileInputStream stream(file);
    if (stream.openedOk()) {
        auto fileSize = file.getSize();
        if (fileSize > 0) {
            result.ensureSize(static_cast<size_t>(fileSize));
            auto bytesRead = stream.read(result.getData(), static_cast<int>(fileSize));
            result.setSize(static_cast<size_t>(bytesRead));
        }
    }

    return result;
}

} // namespace LianCore