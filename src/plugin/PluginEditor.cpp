// =============================================================================
// LianCore - PluginEditor 实现 (Beta阶段: Web UI + WebSocket通信)
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
// PluginEditor 实现
// =============================================================================
PluginEditor::PluginEditor(PluginProcessor& processor)
    : AudioProcessorEditor(&processor)
    , processor_(processor) {
    // 设置窗口大小 (Beta阶段扩大窗口)
    setSize(800, 560);

    // 标题
    titleLabel_.setText("LianCore V3 Beta - AI合成器软音源", juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(16.0f, juce::Font::bold));
    titleLabel_.setJustificationType(juce::Justification::centred);
    titleLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF00cec9));
    addAndMakeVisible(titleLabel_);

    // 状态
    statusLabel_.setText("节点: 全合成引擎 | AI推理: ONNX Runtime | Web UI: 就绪", juce::dontSendNotification);
    statusLabel_.setFont(juce::Font(11.0f));
    statusLabel_.setJustificationType(juce::Justification::centred);
    statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF8888a0));
    addAndMakeVisible(statusLabel_);

    // CPU使用率
    cpuLabel_.setText("CPU: 0.0ms | 内存: 0MB", juce::dontSendNotification);
    cpuLabel_.setFont(juce::Font(10.0f));
    cpuLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF555568));
    addAndMakeVisible(cpuLabel_);

    // WebSocket状态
    wsStatusLabel_.setText("WebSocket: 启动中...", juce::dontSendNotification);
    wsStatusLabel_.setFont(juce::Font(10.0f));
    wsStatusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF2ecc71));
    addAndMakeVisible(wsStatusLabel_);

    // 打开Web UI按钮
    openWebUIButton_.setButtonText("打开 Web UI");
    openWebUIButton_.onClick = [this]() {
        juce::URL("http://localhost:5173").launchInDefaultBrowser();
    };
    addAndMakeVisible(openWebUIButton_);

    // AI测试按钮
    aiTestButton_.setButtonText("AI 测试生成");
    aiTestButton_.onClick = [this]() {
        auto prompt = aiPromptInput_.getText();
        if (prompt.isEmpty()) {
            prompt = "明亮的电子合成器主音";
        }

        auto& ai = processor_.getAIEngine();
        auto result = ai.generateParameters(prompt);
        juce::String info = "AI生成: " + result.presetName + "\n";
        info += "置信度: " + juce::String(result.confidence * 100, 0) + "%\n";
        info += "参数数量: " + juce::String(result.parameters.size()) + "\n";
        info += "模型: " + ai.getModelInfo() + "\n";
        info += "推理时间: " + juce::String(ai.getLastInferenceTimeMs(), 2) + "ms";

        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "AI 推理结果",
            info,
            "确定"
        );
    };
    addAndMakeVisible(aiTestButton_);

    // 波表编辑器按钮 (P2-2)
    wavetableEditorButton_.setButtonText("波表编辑器");
    wavetableEditorButton_.onClick = [this]() {
        wavetableEditorVisible_ = !wavetableEditorVisible_;
        wavetableEditor_.setVisible(wavetableEditorVisible_);
        if (wavetableEditorVisible_) {
            wavetableEditorButton_.setButtonText("关闭波表编辑器");
            // 绑定波表A到编辑器
            auto& graph = processor_.getAudioGraph();
            graph.forEachNode([this](AudioNode* node) {
                if (node->getNodeType() == NodeType::WavetableOscillator) {
                    auto* osc = dynamic_cast<WavetableOscillator*>(node);
                    if (osc) {
                        wavetableEditor_.setWavetableBank(&osc->getWavetableA());
                    }
                }
            });
            setSize(800, 800);
        } else {
            wavetableEditorButton_.setButtonText("波表编辑器");
            setSize(800, 560);
        }
        resized();
    };
    addAndMakeVisible(wavetableEditorButton_);
    addChildComponent(wavetableEditor_); // 默认隐藏

    // MPE录制按钮 (P6-1)
    mpeRecordingButton_.setButtonText("MPE 录制");
    mpeRecordingButton_.onClick = [this]() {
        mpeRecordingVisible_ = !mpeRecordingVisible_;
        mpeRecordingUI_.setVisible(mpeRecordingVisible_);
        if (mpeRecordingVisible_) {
            mpeRecordingButton_.setButtonText("关闭 MPE 录制");
            setSize(800, 800);
        } else {
            mpeRecordingButton_.setButtonText("MPE 录制");
            setSize(800, 560);
        }
        resized();
    };
    addAndMakeVisible(mpeRecordingButton_);
    addChildComponent(mpeRecordingUI_); // 默认隐藏

    // 设置 MPE 录制UI的录制器/播放器引用
    mpeRecordingUI_.setRecorderRef([this]() -> MPERecorder* {
        return &processor_.getMPERecorder();
    });
    mpeRecordingUI_.setPlayerRef([this]() -> MPEPlayer* {
        return &processor_.getMPEPlayer();
    });
    mpeRecordingUI_.startUIUpdates(15);

    // AI提示输入
    aiPromptInput_.setMultiLine(false);
    aiPromptInput_.setTextToShowWhenEmpty("描述声音...", juce::Colour(0xFF555568));
    aiPromptInput_.setFont(juce::Font(13.0f));
    aiPromptInput_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF1e1e2a));
    aiPromptInput_.setColour(juce::TextEditor::textColourId, juce::Colour(0xFFe0e0e0));
    aiPromptInput_.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xFF2a2a3a));
    addAndMakeVisible(aiPromptInput_);

    // 设置WebSocket消息处理
    setupMessageHandlers();

    // 启动WebSocket服务器
    uiServer_.start(9001);
    wsStatusLabel_.setText("WebSocket: 端口9001 | 等待Web UI连接...", juce::dontSendNotification);

    // 启动定时器
    startTimerHz(10);
}

PluginEditor::~PluginEditor() {
    stopTimer();
    uiServer_.stop();
}

void PluginEditor::paint(juce::Graphics& g) {
    // 背景 - 深色极简风格
    g.fillAll(juce::Colour(0xFF0a0a0f));

    // 顶部装饰线
    g.setColour(juce::Colour(0xFF6c5ce7));
    g.drawLine(30, 65, getWidth() - 30, 65, 1.0f);

    // 底部装饰线
    g.setColour(juce::Colour(0xFF2a2a3a));
    g.drawLine(30, getHeight() - 35, getWidth() - 30, getHeight() - 35, 1.0f);

    // 更新CPU/内存显示
    cpuLabel_.setText(
        juce::String::formatted("CPU: %.2fms | 内存: %.1fMB",
            processor_.getCpuUsage(),
            processor_.getMemoryUsage() / 1024.0 / 1024.0),
        juce::dontSendNotification
    );

    // 广播状态到Web UI
    uiServer_.broadcastStatus(processor_.getCpuUsage(), processor_.getMemoryUsage());
}

void PluginEditor::resized() {
    auto area = getLocalBounds().reduced(20);

    // 标题
    titleLabel_.setBounds(area.removeFromTop(35));
    area.removeFromTop(10);

    // 状态行
    statusLabel_.setBounds(area.removeFromTop(20));
    area.removeFromTop(10);

    // AI输入区域
    auto aiRow = area.removeFromTop(35);
    aiPromptInput_.setBounds(aiRow.removeFromLeft(aiRow.getWidth() - 120));
    aiTestButton_.setBounds(aiRow.withWidth(120).reduced(4, 2));

    area.removeFromTop(10);

    // 按钮行
    auto buttonRow = area.removeFromTop(35);
    wavetableEditorButton_.setBounds(buttonRow.removeFromLeft(120).reduced(4, 2));
    buttonRow.removeFromLeft(4);
    mpeRecordingButton_.setBounds(buttonRow.removeFromLeft(120).reduced(4, 2));
    buttonRow.removeFromLeft(4);
    openWebUIButton_.setBounds(buttonRow.withWidth(140).reduced(4, 2));

    area.removeFromTop(10);

    // WebSocket状态
    wsStatusLabel_.setBounds(area.removeFromTop(20));

    // CPU状态
    cpuLabel_.setBounds(10, getHeight() - 30, 300, 20);

    // 波表编辑器 (下半部分)
    if (wavetableEditorVisible_) {
        wavetableEditor_.setBounds(0, 280, getWidth(), getHeight() - 280);
    }

    // MPE录制UI (下半部分, P6-1)
    if (mpeRecordingVisible_) {
        mpeRecordingUI_.setBounds(0, 280, getWidth(), getHeight() - 280);
    }
}

void PluginEditor::timerCallback() {
    // 定时更新UI状态
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