# LianCore Beta 安全最佳实践审查报告

**日期**: 2026-07-12 | **阶段**: Beta Week 8 | **审查范围**: AES-256-GCM 加密 + WebSocket 通信

---

## 执行摘要

对 LianCore 合成器的 AES-256-GCM 加密模块和 WebSocket 通信层进行了安全审查。发现 **2 个严重漏洞**（密码学强度不足）、**2 个高危漏洞**（密钥存储/消息认证缺失）和 **3 个中低危改善建议**。所有严重和高危漏洞均在 Beta Week 8 中修复。

---

## 严重漏洞 (CRITICAL)

### [SEC-001] 密钥生成使用非密码学安全随机数 (CRITICAL)

**影响**: 攻击者可预测 AES 密钥，导致加密数据完全破解。

**位置**: `src/security/AES256Encryptor.cpp` 第 248-253 行 (`generateKey()`) 和第 280-285 行 (`generateSalt()`)

**问题**: `juce::Random` 是 Mersenne Twister 伪随机数生成器，种子基于系统时间，**不适合密码学用途**。MITM 攻击者可以暴力枚举时间窗口内的种子，恢复密钥。

**修复**: 替换为 `juce::CryptographicallySecureRandom`。

### [SEC-002] 密钥派生函数不是标准 PBKDF2 (CRITICAL)

**影响**: 派生密钥强度远低于标准 PBKDF2，易受暴力破解和彩虹表攻击。

**位置**: `src/security/AES256Encryptor.cpp` 第 256-276 行 (`deriveKeyFromPassword()`)

**问题**: 多次 SHA256 迭代不等于 PBKDF2。PBKDF2 需要 HMAC 和依次异或，当前实现将 SHA256 输出作为字符串拼接再哈希，破坏了密码学安全性。

**修复**: 使用 JUCE 的 `juce::RSAKey` 中的 PBKDF2 或自行实现标准 HMAC-SHA256 PBKDF2。

---

## 高危漏洞 (HIGH)

### [SEC-003] 密钥以明文 Base64 存储到文件 (HIGH)

**影响**: 任何有文件系统访问权限的进程/用户可读取密钥。

**位置**: `src/security/AES256Encryptor.cpp` 第 475-481 行 (`saveKeyToFile()`)

**修复**: 添加密钥包装层（使用机器特定密钥加密存储的密钥）。

### [SEC-004] WebSocket 无消息认证机制 (HIGH)

**影响**: 本地任意进程可连接 `ws://localhost:9001` 发送伪造命令，控制合成器参数、注入恶意预设。

**位置**: `src/plugin/PluginEditor.cpp` 的 `setupMessageHandlers()` 和 `ui/src/websocket.ts`

**修复**: 添加基于共享密钥的 HMAC 消息签名验证。

---

## 中危漏洞 (MEDIUM)

### [SEC-005] 无 WebSocket 消息速率限制 (MEDIUM)

**位置**: `src/plugin/PluginEditor.cpp` 消息处理器

**修复**: 添加消息速率限制机制，防止消息洪泛导致 DoS。

### [SEC-006] `secureEraseKey()` 使用 `volatile` 但不保证 (MEDIUM)

**位置**: `src/security/AES256Encryptor.cpp` 第 495-501 行

**修复**: 使用操作系统安全内存清零函数（Windows: `SecureZeroMemory`，macOS: `memset_s`）。

---

## 低危建议 (LOW)

### [SEC-007] WebSocket 无重连退避策略 (LOW)

**位置**: `ui/src/websocket.ts` 第 183-189 行

**建议**: 添加指数退避策略，最大重试次数限制。

---

## 修复状态

| ID | 严重度 | 状态 |
|----|--------|------|
| SEC-001 | CRITICAL | 已修复 |
| SEC-002 | CRITICAL | 已修复 |
| SEC-003 | HIGH | 已修复 |
| SEC-004 | HIGH | 已修复 |
| SEC-005 | MEDIUM | 已修复 |
| SEC-006 | MEDIUM | 已修复 |
| SEC-007 | LOW | 已修复 |