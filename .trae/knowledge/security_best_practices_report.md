# LianCore V3 安全最佳实践审查报告

> **审查日期**: 2026-07-12 | **审查范围**: src/ + ui/ 全量代码
> **语言**: C++ (插件核心) / TypeScript (Web UI) / Python (脚本)

---

## 执行摘要

本次审查覆盖 LianCore V3 项目的 C++ 插件核心、Web UI 前端和 Python 脚本。发现 **1 个高危**、**3 个中危**、**2 个低危** 安全问题。高危问题为 SQL 注入风险（PresetManager.cpp 使用字符串拼接构建 SQL），需立即修复。同时创建了 AES-256-GCM 加密工具类，用于预设的安全导出。

---

## 发现清单

### [SEC-001] 高危 - SQL 注入：PresetManager 使用字符串拼接构建 SQL

**文件**: [`src/params/PresetManager.cpp`](file:///f:/LianCore软音源合成器V3版本商业正式版/src/params/PresetManager.cpp#L75-L88)

**影响**: 攻击者可通过精心构造的预设名称/标签/JSON 数据注入恶意 SQL，导致数据泄露、篡改或删除。

**当前代码** (L75-L88):
```cpp
juce::String sql = juce::String::formatted(
    "INSERT INTO presets (name, category, tags, description, author, json_data, ai_prompt, ai_confidence) "
    "VALUES ('%s', '%s', '%s', '%s', '%s', '%s', '%s', %f)",
    entry.name.replace("'", "''").toRawUTF8(),  // 仅转义单引号，不充分
    ...
);
```

**问题分析**:
- `replace("'", "''")` 仅处理单引号，无法防御二阶注入或 Unicode 绕过
- 涉及 `savePreset` (L75)、`updatePreset` (L116)、`searchPresets` (L153)、`getPresetsByCategory` (L173)、`getPresetsByTag` (L190)、`savePresetVersion` (L244)、`restorePresetVersion` (L280) 共 7 处

**修复建议**: 使用 JUCE SQLite 的参数化查询接口（`juce::SQLite::Statement`），将用户数据作为绑定参数而非 SQL 字符串拼接。

---

### [SEC-002] 中危 - WebSocket 无认证机制

**文件**: [`src/plugin/PluginEditor.cpp`](file:///f:/LianCore软音源合成器V3版本商业正式版/src/plugin/PluginEditor.cpp#L47-L51)

**影响**: 任何能访问 localhost:9001 的进程都可以向合成器发送命令，包括修改参数、触发 AI 生成、读/写预设数据。

**当前代码** (L47-L51):
```cpp
if (auto* newSocket = socket_.waitForNextSocketConnecting(100)) {
    auto client = std::make_unique<ClientConnection>();
    client->socket.reset(newSocket);
    clients_.push_back(std::move(client));  // 无认证，直接接受
}
```

**修复建议**:
- 添加基于 token 的握手认证（WebSocket 首帧携带认证 token）
- 限制连接来源为 localhost only（绑定时指定 `127.0.0.1` 而非 `0.0.0.0`）
- 限制最大并发连接数

---

### [SEC-003] 中危 - JSON 解析无输入验证

**文件**: [`src/plugin/PluginEditor.cpp`](file:///f:/LianCore软音源合成器V3版本商业正式版/src/plugin/PluginEditor.cpp#L65-L81)

**影响**: 恶意构造的超大 JSON 可导致内存耗尽（DoS）；未验证的消息类型可能触发未预期的行为。

**当前代码** (L65-L81):
```cpp
auto msg = parseWebSocketMessage(client->buffer);
if (msg.isEmpty()) break;
auto json = juce::JSON::parse(msg);  // 未限制大小
if (json.isObject()) {
    auto type = json.getProperty("type", "");
    auto payload = json.getProperty("payload", juce::var());
    // 直接调用 handler，无 schema 验证
    handlerIt->second(payload);
}
```

**修复建议**:
- 限制单条消息最大大小（如 1MB）
- 添加消息 schema 验证
- 对未知消息类型静默丢弃而非抛出异常

---

### [SEC-004] 中危 - 预设导出无加密保护

**文件**: [`src/params/PresetManager.cpp`](file:///f:/LianCore软音源合成器V3版本商业正式版/src/params/PresetManager.cpp#L289-L303)

**影响**: 商业预设以明文 JSON 导出，可被轻易复制和分发，缺乏知识产权保护。

**当前代码** (L289-L303):
```cpp
bool PresetManager::exportPreset(int id, const juce::File& file) {
    // ... 构建 JSON ...
    return file.replaceWithText(juce::JSON::toString(juce::var(obj.get())));
    // 明文写入，无加密
}
```

**修复建议**:
- 使用 AES-256-GCM 加密预设数据（已提供 `AES256Encryptor` 工具类）
- 密钥由用户设定或从硬件指纹派生
- 加密后添加 HMAC 签名防篡改

---

### [SEC-005] 低危 - 固定大小缓冲区

**文件**: [`src/plugin/PluginEditor.cpp`](file:///f:/LianCore软音源合成器V3版本商业正式版/src/plugin/PluginEditor.cpp#L57)

**影响**: 4096 字节固定缓冲区，大消息可能被截断，但不会导致溢出。

**当前代码** (L57):
```cpp
char buffer[4096];  // 固定大小
int bytesRead = client->socket->read(buffer, sizeof(buffer) - 1, false);
```

**修复建议**: 使用 `std::vector<char>` 动态分配缓冲区，或使用 `juce::MemoryBlock`。

---

### [SEC-006] 低危 - 缺少 JSON 解析错误处理

**文件**: [`src/params/ParameterMapping.cpp`](file:///f:/LianCore软音源合成器V3版本商业正式版/src/params/ParameterMapping.cpp#L933-L934)

**影响**: 解析失败时静默返回 false，不提供错误信息，用户难以排查问题。

**修复建议**: 添加日志记录解析失败原因，向 UI 返回有意义的错误消息。

---

## 新建安全工具

### AES-256-GCM 加密工具类

**文件**: [`src/security/AES256Encryptor.h`](file:///f:/LianCore软音源合成器V3版本商业正式版/src/security/AES256Encryptor.h)

提供以下功能：
- `encrypt(plaintext, key)` → Base64 编码的密文（含 IV + 认证标签）
- `decrypt(ciphertext, key)` → 明文
- `generateKey()` → 随机 256-bit 密钥
- `deriveKeyFromPassword(password, salt)` → PBKDF2 密钥派生
- GCM 模式提供认证加密（保密性 + 完整性）
- 使用 JUCE 内置 `juce::BigInteger` 和位运算实现（零外部依赖）

---

## 修复优先级

| 优先级 | ID | 问题 | 建议时间 |
|--------|-----|------|---------|
| P0 | SEC-001 | SQL 注入 | 立即修复 |
| P1 | SEC-004 | 预设导出加密 | 本周内 |
| P2 | SEC-002 | WebSocket 认证 | 下个迭代 |
| P2 | SEC-003 | JSON 输入验证 | 下个迭代 |
| P3 | SEC-005 | 缓冲区大小 | 后续优化 |
| P3 | SEC-006 | 错误处理 | 后续优化 |

---

> **复审**: 所有 P0/P1 修复完成后，建议进行第二轮安全审查。