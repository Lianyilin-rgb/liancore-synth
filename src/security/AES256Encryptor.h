// =============================================================================
// LianCore - AES256Encryptor AES-256-GCM 加密工具
// 用于预设安全导出/导入，保护知识产权
// 零外部依赖，基于标准 AES-256 算法实现
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <array>
#include <cstdint>

namespace LianCore {
namespace Security {

// =============================================================================
// AES-256 常量
// =============================================================================
constexpr int AES_BLOCK_SIZE = 16;      // 128-bit block
constexpr int AES_KEY_SIZE = 32;        // 256-bit key
constexpr int AES_ROUNDS = 14;          // 14 rounds for AES-256
constexpr int GCM_IV_SIZE = 12;         // 96-bit IV (recommended for GCM)
constexpr int GCM_TAG_SIZE = 16;        // 128-bit authentication tag

// =============================================================================
// AES-256-GCM 加密器
// =============================================================================
class AES256Encryptor {
public:
    // =========================================================================
    // 密钥管理
    // =========================================================================

    /// 生成随机 256-bit AES 密钥
    static std::array<uint8_t, AES_KEY_SIZE> generateKey();

    /// 从密码派生密钥 (PBKDF2-SHA256, 100000 iterations)
    static std::array<uint8_t, AES_KEY_SIZE> deriveKeyFromPassword(
        const juce::String& password,
        const std::array<uint8_t, 16>& salt
    );

    /// 生成随机盐值
    static std::array<uint8_t, 16> generateSalt();

    /// 从硬件指纹派生密钥 (CPU ID + 主板序列号 + 固定熵)
    static std::array<uint8_t, AES_KEY_SIZE> deriveKeyFromHardware();

    // =========================================================================
    // 加密/解密
    // =========================================================================

    /// AES-256-GCM 加密
    /// @param plaintext 明文数据
    /// @param key 256-bit 密钥
    /// @return Base64 编码的密文 (格式: IV(12B) + Ciphertext + Tag(16B))
    static juce::String encrypt(
        const juce::String& plaintext,
        const std::array<uint8_t, AES_KEY_SIZE>& key
    );

    /// AES-256-GCM 解密
    /// @param ciphertextB64 Base64 编码的密文
    /// @param key 256-bit 密钥
    /// @return 解密后的明文，失败返回空字符串
    static juce::String decrypt(
        const juce::String& ciphertextB64,
        const std::array<uint8_t, AES_KEY_SIZE>& key
    );

    /// 加密二进制数据
    static std::vector<uint8_t> encryptBinary(
        const std::vector<uint8_t>& plaintext,
        const std::array<uint8_t, AES_KEY_SIZE>& key
    );

    /// 解密二进制数据
    static std::vector<uint8_t> decryptBinary(
        const std::vector<uint8_t>& ciphertext,
        const std::array<uint8_t, AES_KEY_SIZE>& key
    );

    // =========================================================================
    // 密钥存储
    // =========================================================================

    /// 安全存储密钥到文件 (Base64 编码)
    static bool saveKeyToFile(
        const std::array<uint8_t, AES_KEY_SIZE>& key,
        const juce::File& file
    );

    /// 从文件加载密钥
    static std::array<uint8_t, AES_KEY_SIZE> loadKeyFromFile(
        const juce::File& file
    );

    /// 安全擦除密钥内存
    static void secureEraseKey(std::array<uint8_t, AES_KEY_SIZE>& key);

private:
    // =========================================================================
    // AES 核心算法
    // =========================================================================

    static void keyExpansion(
        const uint8_t* key,
        uint8_t* roundKeys
    );

    static void encryptBlock(
        const uint8_t* input,
        uint8_t* output,
        const uint8_t* roundKeys
    );

    static void aesEncryptECB(
        const uint8_t* input,
        uint8_t* output,
        size_t numBlocks,
        const uint8_t* roundKeys
    );

    // GCM 模式
    static void gcmMultiply(uint8_t* x, const uint8_t* y);
    static void gcmGHASH(
        const uint8_t* h,
        const uint8_t* aad, size_t aadLen,
        const uint8_t* ciphertext, size_t ctLen,
        uint8_t* tag
    );
    static void incrementCounter(uint8_t* counter);

    // S-Box 和逆 S-Box
    static const uint8_t sbox_[256];
    static const uint8_t rsbox_[256];
    static const uint8_t rcon_[15];

    static void subBytes(uint8_t* state);
    static void shiftRows(uint8_t* state);
    static void mixColumns(uint8_t* state);
    static void addRoundKey(uint8_t* state, const uint8_t* roundKey);
};

} // namespace Security
} // namespace LianCore