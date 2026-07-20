// =============================================================================
// LianCore - PluginEditor 实现 (WebBrowserComponent + 内置HTTP服务器)
// 嵌入 JUCE WebBrowserComponent 加载 Web UI，无需外部浏览器
// WebSocket 端口 9001 用于 Web UI ↔ C++ 核心实时通信
// =============================================================================
#include "PluginEditor.h"
#include "../ai/AIInferenceEngine.h"
#include "../ai/EmotionToParameterMapper.h"
#include "../synthesis/WavetableOscillator.h"
#include "../plugin/MPERecorder.h"

namespace LianCore {

// 前向声明
static juce::String parseWebSocketMessage(juce::String& buffer);

// =============================================================================
// UIMessageServer 实现
// =============================================================================
UIMessageServer::UIMessageServer()
    : juce::Thread("LianCore-UI-Server") {
}

UIMessageServer::~UIMessageServer() {
    stop();
}

void UIMessageServer::start(int port) {
    if (running_) return;
    port_ = port;

    // 创建TCP服务器socket
    if (socket_.createListener(port_)) {
        running_ = true;
        startThread(juce::Thread::Priority::background);
        DBG("[LianCore] WebSocket服务器启动于端口 " << port_);
    } else {
        DBG("[LianCore] WebSocket服务器启动失败");
    }
}

void UIMessageServer::stop() {
    running_ = false;
    socket_.close();
    stopThread(1000);
    clients_.clear();
}

void UIMessageServer::run() {
    while (running_ && !threadShouldExit()) {
        // 接受新连接
        if (auto* newSocket = socket_.waitForNextConnection()) {
            DBG("[LianCore] Web UI 已连接");
            auto client = std::make_unique<ClientConnection>();
            client->socket.reset(newSocket);
            clients_.push_back(std::move(client));
        }

        // 处理客户端消息
        for (auto it = clients_.begin(); it != clients_.end();) {
            auto& client = *it;
            char buffer[4096];
            int bytesRead = client->socket->read(buffer, sizeof(buffer) - 1, false);

            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                client->buffer += juce::String::fromUTF8(buffer, bytesRead);

                // 处理WebSocket帧
                while (true) {
                    auto msg = parseWebSocketMessage(client->buffer);
                    if (msg.isEmpty()) break;

                    // 解析JSON消息
                    auto json = juce::JSON::parse(msg);
                    if (json.isObject()) {
                        auto type = json.getProperty("type", "");
                        auto payload = json.getProperty("payload", juce::var());

                        juce::ScopedLock sl(lock_);
                        auto handlerIt = handlers_.find(type.toString().toStdString());
                        if (handlerIt != handlers_.end()) {
                            handlerIt->second(payload);
                        }
                    }
                }
                ++it;
            } else if (bytesRead < 0 || !client->socket->isConnected()) {
                it = clients_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void UIMessageServer::sendToUI(const juce::String& type, const juce::var& payload) {
    juce::DynamicObject msg;
    msg.setProperty("type", type);
    msg.setProperty("payload", payload);
    msg.setProperty("timestamp", juce::Time::getCurrentTime().toMilliseconds());

    auto jsonStr = juce::JSON::toString(juce::var(new juce::DynamicObject(std::move(msg))));

    // WebSocket帧封装: text frame, FIN=1, opcode=1, mask=0(server→client)
    juce::MemoryBlock frame;
    frame.append("\x81", 1); // FIN + Text opcode
    if (jsonStr.length() < 126) {
        auto lenByte = static_cast<char>(jsonStr.length());
        frame.append(&lenByte, 1);
    } else if (jsonStr.length() < 65536) {
        frame.append("\x7E", 1);
        uint16_t len = static_cast<uint16_t>(jsonStr.length());
        len = juce::ByteOrder::swapIfBigEndian(len);
        frame.append(&len, 2);
    }
    frame.append(jsonStr.toRawUTF8(), jsonStr.getNumBytesAsUTF8());

    for (auto& client : clients_) {
        if (client->socket && client->socket->isConnected()) {
            client->socket->write(frame.getData(), static_cast<int>(frame.getSize()));
        }
    }
}

void UIMessageServer::onMessage(const juce::String& type, MessageHandler handler) {
    juce::ScopedLock sl(lock_);
    handlers_[type.toStdString()] = std::move(handler);
}

void UIMessageServer::broadcastStatus(double cpuMs, size_t memoryBytes) {
    juce::DynamicObject status;
    status.setProperty("usage", cpuMs);
    status.setProperty("mb", static_cast<double>(memoryBytes) / 1024.0 / 1024.0);
    sendToUI("cpu_usage", juce::var(new juce::DynamicObject(std::move(status))));
}

// 简单WebSocket帧解析 (server端, 客户端发送的是masked帧)
static juce::String parseWebSocketMessage(juce::String& buffer) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buffer.toRawUTF8());
    size_t size = buffer.getNumBytesAsUTF8();

    if (size < 2) return {};

    uint8_t opcode = data[0] & 0x0F;
    bool masked = (data[1] & 0x80) != 0;
    uint64_t payloadLen = data[1] & 0x7F;

    size_t headerSize = 2;
    if (payloadLen == 126) {
        if (size < 4) return {};
        payloadLen = (static_cast<uint16_t>(data[2]) << 8) | data[3];
        headerSize = 4;
    } else if (payloadLen == 127) {
        if (size < 10) return {};
        payloadLen = 0;
        for (int i = 0; i < 8; ++i) {
            payloadLen = (payloadLen << 8) | data[2 + i];
        }
        headerSize = 10;
    }

    size_t maskOffset = headerSize;
    if (masked) headerSize += 4;
    if (size < headerSize + payloadLen) return {};

    // 解码
    juce::MemoryBlock decoded(static_cast<size_t>(payloadLen));
    if (masked) {
        const uint8_t* mask = data + maskOffset;
        const uint8_t* payload = data + headerSize;
        for (uint64_t i = 0; i < payloadLen; ++i) {
            decoded[i] = payload[i] ^ mask[i % 4];
        }
    } else {
        std::memcpy(decoded.getData(), data + headerSize, static_cast<size_t>(payloadLen));
    }

    // 移除已处理的数据
    buffer = buffer.substring(static_cast<int>(headerSize + payloadLen));

    if (opcode == 0x1) { // Text frame
        return juce::String::fromUTF8(static_cast<const char*>(decoded.getData()), static_cast<int>(payloadLen));
    }
    return {};
}

// =============================================================================
// Bundle UI 路径获取
// 跨平台获取 VST3 Bundle 的 Contents/Resources/ui/ 目录
// Windows: LianCore.vst3/Contents/x86_64-win/LianCore.vst3 → 向上两级 → Contents/Resources/ui/
// macOS:   LianCore.vst3/Contents/MacOS/LianCore → 向上两级 → Contents/Resources/ui/
// =============================================================================
juce::File PluginEditor::getBundleUIPath() {
    auto execFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    auto bundleRoot = execFile.getParentDirectory()  // Windows: x86_64-win/  macOS: MacOS/
                              .getParentDirectory(); // Windows: Contents/   macOS: Contents/
    auto uiDir = bundleRoot.getChildFile("Resources").getChildFile("ui");

    DBG("[LianCore] Bundle UI 路径: " << uiDir.getFullPathName());
    return uiDir;
}

// =============================================================================
// PluginEditor 实现
// =============================================================================
PluginEditor::PluginEditor(PluginProcessor& processor)
    : AudioProcessorEditor(&processor)
    , processor_(processor) {
    // 设置窗口大小（Web UI 全屏显示）
    setSize(800, 560);

    // =========================================================================
    // 步骤1: 获取 VST3 Bundle 内的 UI 资源目录
    // =========================================================================
    auto uiDir = getBundleUIPath();

    // =========================================================================
    // 步骤2: 启动内置 HTTP 静态文件服务器（端口 9000）
    // 从 VST3 Bundle 的 Contents/Resources/ui/ 目录提供文件
    // =========================================================================
    httpServer_ = std::make_unique<SimpleHTTPServer>();
    if (!httpServer_->start(9000, uiDir)) {
        DBG("[LianCore] HTTP 服务器启动失败，Web UI 将不可用");
    }

    // =========================================================================
    // 步骤3: 设置 WebSocket 消息处理（必须在 WebBrowserComponent 之前）
    // =========================================================================
    setupMessageHandlers();

    // =========================================================================
    // 步骤4: 启动 WebSocket 消息服务器（端口 9001）
    // =========================================================================
    uiServer_.start(9001);

    // =========================================================================
    // 步骤5: 创建嵌入式 Web 浏览器组件
    // 加载内置 HTTP 服务器提供的 Web UI
    // JUCE 8.0.14 API: Windows 使用 WebView2，macOS 使用 WKWebView
    // =========================================================================
    juce::WebBrowserComponent::Options webOptions;
    webOptions.withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
              .withNativeIntegrationEnabled();

    webBrowser_ = std::make_unique<juce::WebBrowserComponent>(webOptions);

    // 加载 Web UI（通过内置 HTTP 服务器）
    // JUCE 8.0.14: goToURL 接受 const juce::String&
    juce::String webUIUrl = "http://localhost:" + juce::String(httpServer_->getPort()) + "/index.html";
    webBrowser_->goToURL(webUIUrl);

    addAndMakeVisible(webBrowser_.get());

    DBG("[LianCore] WebBrowserComponent 已创建，加载: " << webUIUrl);

    // 启动定时器（用于 CPU/内存状态广播）
    startTimerHz(10);
}

PluginEditor::~PluginEditor() {
    stopTimer();

    // 先停止 WebSocket 服务器
    uiServer_.stop();

    // 再停止 HTTP 服务器
    if (httpServer_) {
        httpServer_->stop();
    }

    // 移除 WebBrowserComponent
    if (webBrowser_) {
        webBrowser_.reset();
    }
}

void PluginEditor::paint(juce::Graphics& g) {
    // 背景色：纯黑，确保 WebBrowserComponent 加载前和卸载后视觉一致
    g.fillAll(juce::Colour(0xFF0a0a0f));

    // 广播 CPU/内存状态到 Web UI
    uiServer_.broadcastStatus(processor_.getCpuUsage(), processor_.getMemoryUsage());
}

void PluginEditor::resized() {
    // WebBrowserComponent 填满整个编辑器窗口
    if (webBrowser_) {
        webBrowser_->setBounds(getLocalBounds());
    }
}

void PluginEditor::timerCallback() {
    // 定时更新 UI 状态
    repaint();
}

void PluginEditor::setupMessageHandlers() {
    // AI生成请求
    uiServer_.onMessage("ai_generate_request", [this](const juce::var& payload) {
        juce::String prompt = payload.getProperty("prompt", "");

        // 发送进度
        uiServer_.sendToUI("ai_generate_progress", juce::var());

        // 执行AI推理
        auto& ai = processor_.getAIEngine();
        auto result = ai.generateParameters(prompt);

        // 构建结果
        juce::DynamicObject resultObj;
        resultObj.setProperty("presetName", result.presetName);
        resultObj.setProperty("confidence", result.confidence);

        juce::Array<juce::var> params;
        for (const auto& param : result.parameters) {
            juce::DynamicObject p;
            p.setProperty("parameterId", param.parameterId);
            p.setProperty("value", param.value);
            p.setProperty("explanation", param.explanation);
            params.add(juce::var(new juce::DynamicObject(std::move(p))));
        }
        resultObj.setProperty("parameters", params);

        uiServer_.sendToUI("ai_generate_result", juce::var(new juce::DynamicObject(std::move(resultObj))));
    });

    // 预设列表请求
    uiServer_.onMessage("preset_list", [this](const juce::var&) {
        auto& presetManager = processor_.getPresetManager();
        auto presets = presetManager.getAllPresets();

        juce::Array<juce::var> presetList;
        for (const auto& preset : presets) {
            juce::DynamicObject p;
            p.setProperty("id", preset.id);
            p.setProperty("name", preset.name);
            p.setProperty("category", preset.category);

            juce::Array<juce::var> tags;
            for (const auto& tag : preset.tags) {
                tags.add(juce::String::charToString(tag));
            }
            p.setProperty("tags", tags);
            p.setProperty("rating", preset.rating);
            presetList.add(juce::var(new juce::DynamicObject(std::move(p))));
        }

        juce::DynamicObject resp;
        resp.setProperty("presets", presetList);
        uiServer_.sendToUI("preset_list", juce::var(new juce::DynamicObject(std::move(resp))));
    });

    // 预设加载请求
    uiServer_.onMessage("preset_load", [this](const juce::var& payload) {
        int presetId = payload.getProperty("presetId", 0);
        auto& presetManager = processor_.getPresetManager();
        PresetEntry entry;
        if (presetManager.loadPreset(presetId, entry)) {
            uiServer_.sendToUI("preset_load", juce::var("ok"));
        }
    });

    // 预设保存请求
    uiServer_.onMessage("preset_save", [this](const juce::var& payload) {
        juce::String name = payload.getProperty("name", "");
        juce::String category = payload.getProperty("category", "");
        // 保存当前状态为预设
        auto& presetManager = processor_.getPresetManager();
        PresetEntry entry;
        entry.name = name;
        entry.category = category;
        presetManager.savePreset(entry);
        uiServer_.sendToUI("preset_list", juce::var());
    });

    // 参数变更
    uiServer_.onMessage("param_change", [this](const juce::var& payload) {
        juce::String paramId = payload.getProperty("parameterId", "");
        float value = payload.getProperty("value", 0.0f);
        auto& params = processor_.getParameterTree();
        params.setParameter(paramId, value);
    });

    // 批量参数变更
    uiServer_.onMessage("param_batch", [this](const juce::var& payload) {
        auto params = payload.getProperty("parameters", juce::var());
        if (params.isArray()) {
            auto& paramTree = processor_.getParameterTree();
            for (const auto& p : *params.getArray()) {
                paramTree.setParameter(
                    p.getProperty("parameterId", ""),
                    p.getProperty("value", 0.0f)
                );
            }
        }
    });

    // 情感滑块实时映射 (Beta Week 6)
    // 直接调用 EmotionToParameterMapper 更新参数，无需 AI 推理
    uiServer_.onMessage("emotion", [this](const juce::var& payload) {
        float warmth  = static_cast<float>(payload.getProperty("warmth", 0.5));
        float energy  = static_cast<float>(payload.getProperty("energy", 0.5));
        float tension = static_cast<float>(payload.getProperty("tension", 0.5));

        // 使用直接映射规则 (快速路径, < 1ms)
        auto emotionParams = EmotionToParameterMapper::mapEmotionDirect(warmth, energy, tension);

        // 应用参数到合成器
        auto& paramTree = processor_.getParameterTree();
        for (const auto& mapping : emotionParams) {
            paramTree.setParameter(mapping.parameterId, mapping.value);
        }

        // 回传确认
        juce::DynamicObject ack;
        ack.setProperty("warmth", warmth);
        ack.setProperty("energy", energy);
        ack.setProperty("tension", tension);
        ack.setProperty("paramCount", static_cast<int>(emotionParams.size()));
        uiServer_.sendToUI("emotion_applied", juce::var(new juce::DynamicObject(std::move(ack))));
    });

    // 联合生成请求 (文本 + 情感) (Beta Week 6)
    // 当 generate 消息中包含 emotion 数据时调用 generateParametersWithEmotion
    uiServer_.onMessage("generate", [this](const juce::var& payload) {
        juce::String text = payload.getProperty("text", "");
        auto emotionObj = payload.getProperty("emotion", juce::var());
        auto styleTagsVar = payload.getProperty("styleTags", juce::var());

        std::vector<juce::String> styleTags;
        if (styleTagsVar.isArray()) {
            for (const auto& tag : *styleTagsVar.getArray()) {
                styleTags.push_back(tag.toString());
            }
        }

        // 发送进度
        uiServer_.sendToUI("ai_generate_progress", juce::var());

        auto& ai = processor_.getAIEngine();
        AIInferenceEngine::GenerationResult result;

        if (emotionObj.isObject()) {
            // 包含情感数据 → 使用情感增强推理
            float warmth  = static_cast<float>(emotionObj.getProperty("warmth", 0.5));
            float energy  = static_cast<float>(emotionObj.getProperty("energy", 0.5));
            float tension = static_cast<float>(emotionObj.getProperty("tension", 0.5));

            result = ai.generateParametersWithEmotion(text, warmth, energy, tension, styleTags);
        } else {
            // 纯文本推理
            result = ai.generateParameters(text, nullptr, styleTags);
        }

        // 构建结果
        juce::DynamicObject resultObj;
        resultObj.setProperty("presetName", result.presetName);
        resultObj.setProperty("confidence", result.confidence);

        juce::Array<juce::var> params;
        for (const auto& param : result.parameters) {
            juce::DynamicObject p;
            p.setProperty("parameterId", param.parameterId);
            p.setProperty("value", param.value);
            p.setProperty("explanation", param.explanation);
            params.add(juce::var(new juce::DynamicObject(std::move(p))));
        }
        resultObj.setProperty("parameters", params);

        // Beta Week 8: 附加推理引擎信息
        resultObj.setProperty("inferenceTimeMs", ai.getLastInferenceTimeMs());
        resultObj.setProperty("modelInfo", ai.getModelInfo());

        uiServer_.sendToUI("ai_generate_result", juce::var(new juce::DynamicObject(std::move(resultObj))));
    });

    // =========================================================================
    // Beta Week 8: morphTo 渐变过渡消息处理
    // Web UI 可触发参数渐变，例如预设切换时平滑过渡
    // =========================================================================
    uiServer_.onMessage("morph", [this](const juce::var& payload) {
        auto targetsVar = payload.getProperty("targets", juce::var());
        int durationMs = payload.getProperty("durationMs", 300);

        if (!targetsVar.isArray()) {
            uiServer_.sendToUI("error", juce::var("morph: targets must be an array"));
            return;
        }

        // 构建 morph 目标列表
        std::vector<LianCoreParameterTree::MorphTarget> targets;
        for (const auto& t : *targetsVar.getArray()) {
            LianCoreParameterTree::MorphTarget target;
            target.parameterId = t.getProperty("parameterId", "").toString();
            target.targetValue = static_cast<float>(t.getProperty("targetValue", 0.5));
            if (target.parameterId.isNotEmpty()) {
                targets.push_back(target);
            }
        }

        if (targets.empty()) {
            uiServer_.sendToUI("error", juce::var("morph: no valid targets"));
            return;
        }

        // 启动渐变
        auto& paramTree = processor_.getParameterTree();
        paramTree.morphTo(targets, durationMs);

        // 发送确认
        juce::DynamicObject ack;
        ack.setProperty("targetCount", static_cast<int>(targets.size()));
        ack.setProperty("durationMs", durationMs);
        uiServer_.sendToUI("morph_started", juce::var(new juce::DynamicObject(std::move(ack))));
    });

    // =========================================================================
    // Beta Week 8: ONNX 模型状态查询
    // =========================================================================
    uiServer_.onMessage("onnx_status", [this](const juce::var&) {
        pushOnnxStatus();
    });

    // =========================================================================
    // Beta Week 8: morph 进度查询
    // =========================================================================
    uiServer_.onMessage("morph_status", [this](const juce::var&) {
        auto& paramTree = processor_.getParameterTree();
        if (paramTree.isMorphing()) {
            pushMorphProgress(0.5f, "morphing");
        } else {
            pushMorphProgress(1.0f, "idle");
        }
    });
}

// =============================================================================
// Beta Week 8: ONNX 状态推送
// =============================================================================
void PluginEditor::pushOnnxStatus() {
    auto& ai = processor_.getAIEngine();
    juce::DynamicObject status;

    status.setProperty("modelLoaded", ai.isModelLoaded());
    status.setProperty("modelInfo", ai.getModelInfo());
    status.setProperty("lastInferenceTimeMs", ai.getLastInferenceTimeMs());
    status.setProperty("onnxAvailable",
#ifdef LIANCORE_HAS_ONNX
        true
#else
        false
#endif
    );

    uiServer_.sendToUI("onnx_status", juce::var(new juce::DynamicObject(std::move(status))));
}

// =============================================================================
// Beta Week 8: morph 渐变进度推送
// =============================================================================
void PluginEditor::pushMorphProgress(float progress, const juce::String& status) {
    juce::DynamicObject info;
    info.setProperty("progress", progress);
    info.setProperty("status", status);
    uiServer_.sendToUI("morph_progress", juce::var(new juce::DynamicObject(std::move(info))));
}

// =============================================================================
// Beta Week 8: morph 渐变完成回调
// =============================================================================
void PluginEditor::onMorphComplete() {
    pushMorphProgress(1.0f, "complete");
}

} // namespace LianCore