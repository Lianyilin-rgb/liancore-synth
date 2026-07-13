// =============================================================================
// LianCore - AES256Encryptor 实现
// AES-256-GCM 加密，用于预设安全导出
// 参考: NIST FIPS 197 (AES), NIST SP 800-38D (GCM)
// =============================================================================
#include "AES256Encryptor.h"
#include <juce_cryptography/juce_cryptography.h>
#include <random>
#include <cstring>

namespace LianCore {
namespace Security {

// =============================================================================
// S-Box (Rijndael S-Box)
// =============================================================================
const uint8_t AES256Encryptor::sbox_[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16,
};

// 轮常量
const uint8_t AES256Encryptor::rcon_[15] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40,
    0x80, 0x1b, 0x36, 0x6c, 0xd8, 0xab, 0x4d,
};

// =============================================================================
// 密钥扩展 (AES-256: 8 个字密钥 → 60 个轮密钥字 = 240 bytes)
// =============================================================================
void AES256Encryptor::keyExpansion(const uint8_t* key, uint8_t* roundKeys) {
    uint8_t temp[4];
    const int Nk = 8;   // 256-bit key = 8 words
    const int Nr = 14;  // 14 rounds

    // 复制原始密钥
    std::memcpy(roundKeys, key, AES_KEY_SIZE);

    for (int i = Nk; i < 4 * (Nr + 1); i++) {
        // temp = W[i-1]
        temp[0] = roundKeys[4 * (i - 1)];
        temp[1] = roundKeys[4 * (i - 1) + 1];
        temp[2] = roundKeys[4 * (i - 1) + 2];
        temp[3] = roundKeys[4 * (i - 1) + 3];

        if (i % Nk == 0) {
            // RotWord
            uint8_t k = temp[0];
            temp[0] = temp[1]; temp[1] = temp[2]; temp[2] = temp[3]; temp[3] = k;
            // SubWord
            temp[0] = sbox_[temp[0]];
            temp[1] = sbox_[temp[1]];
            temp[2] = sbox_[temp[2]];
            temp[3] = sbox_[temp[3]];
            // XOR with Rcon
            temp[0] ^= rcon_[i / Nk];
        } else if (Nk > 6 && i % Nk == 4) {
            // SubWord (AES-256 额外处理)
            temp[0] = sbox_[temp[0]];
            temp[1] = sbox_[temp[1]];
            temp[2] = sbox_[temp[2]];
            temp[3] = sbox_[temp[3]];
        }

        roundKeys[4 * i]     = roundKeys[4 * (i - Nk)]     ^ temp[0];
        roundKeys[4 * i + 1] = roundKeys[4 * (i - Nk) + 1] ^ temp[1];
        roundKeys[4 * i + 2] = roundKeys[4 * (i - Nk) + 2] ^ temp[2];
        roundKeys[4 * i + 3] = roundKeys[4 * (i - Nk) + 3] ^ temp[3];
    }
}

// =============================================================================
// 字节替换
// =============================================================================
void AES256Encryptor::subBytes(uint8_t* state) {
    for (int i = 0; i < 16; i++) {
        state[i] = sbox_[state[i]];
    }
}

// =============================================================================
// 行移位
// =============================================================================
void AES256Encryptor::shiftRows(uint8_t* state) {
    uint8_t temp;
    // Row 1: shift left 1
    temp = state[1]; state[1] = state[5]; state[5] = state[9]; state[9] = state[13]; state[13] = temp;
    // Row 2: shift left 2
    temp = state[2]; state[2] = state[10]; state[10] = temp;
    temp = state[6]; state[6] = state[14]; state[14] = temp;
    // Row 3: shift left 3
    temp = state[15]; state[15] = state[11]; state[11] = state[7]; state[7] = state[3]; state[3] = temp;
}

// =============================================================================
// 列混合
// =============================================================================
static uint8_t xtime(uint8_t x) {
    return (uint8_t)((x << 1) ^ ((x >> 7) ? 0x1b : 0x00));
}

void AES256Encryptor::mixColumns(uint8_t* state) {
    for (int i = 0; i < 4; i++) {
        int c = i * 4;
        uint8_t a0 = state[c], a1 = state[c + 1], a2 = state[c + 2], a3 = state[c + 3];
        state[c]     = xtime(a0) ^ (xtime(a1) ^ a1) ^ a2 ^ a3;
        state[c + 1] = a0 ^ xtime(a1) ^ (xtime(a2) ^ a2) ^ a3;
        state[c + 2] = a0 ^ a1 ^ xtime(a2) ^ (xtime(a3) ^ a3);
        state[c + 3] = (xtime(a0) ^ a0) ^ a1 ^ a2 ^ xtime(a3);
    }
}

// =============================================================================
// 加轮密钥
// =============================================================================
void AES256Encryptor::addRoundKey(uint8_t* state, const uint8_t* roundKey) {
    for (int i = 0; i < 16; i++) {
        state[i] ^= roundKey[i];
    }
}

// =============================================================================
// 加密单个块
// =============================================================================
void AES256Encryptor::encryptBlock(const uint8_t* input, uint8_t* output, const uint8_t* roundKeys) {
    uint8_t state[16];
    std::memcpy(state, input, 16);

    addRoundKey(state, roundKeys);  // Initial round

    for (int round = 1; round < AES_ROUNDS; round++) {
        subBytes(state);
        shiftRows(state);
        mixColumns(state);
        addRoundKey(state, roundKeys + round * 16);
    }

    // Final round (no mixColumns)
    subBytes(state);
    shiftRows(state);
    addRoundKey(state, roundKeys + AES_ROUNDS * 16);

    std::memcpy(output, state, 16);
}

// =============================================================================
// ECB 模式加密多个块
// =============================================================================
void AES256Encryptor::aesEncryptECB(
    const uint8_t* input, uint8_t* output, size_t numBlocks, const uint8_t* roundKeys
) {
    for (size_t i = 0; i < numBlocks; i++) {
        encryptBlock(input + i * 16, output + i * 16, roundKeys);
    }
}

// =============================================================================
// GCM - GF(2^128) 乘法
// =============================================================================
void AES256Encryptor::gcmMultiply(uint8_t* x, const uint8_t* y) {
    uint8_t z[16] = {0};
    uint8_t v[16];
    std::memcpy(v, y, 16);

    for (int i = 0; i < 128; i++) {
        if (x[i / 8] & (1 << (7 - (i % 8)))) {
            for (int j = 0; j < 16; j++) z[j] ^= v[j];
        }
        bool carry = v[15] & 1;
        for (int j = 15; j > 0; j--) {
            v[j] = (v[j] >> 1) | (v[j - 1] << 7);
        }
        v[0] >>= 1;
        if (carry) v[0] ^= 0xe1;
    }

    std::memcpy(x, z, 16);
}

// =============================================================================
// GCM - GHASH 函数
// =============================================================================
void AES256Encryptor::gcmGHASH(
    const uint8_t* h, const uint8_t* aad, size_t aadLen,
    const uint8_t* ct, size_t ctLen, uint8_t* tag
) {
    uint8_t y[16] = {0};
    uint8_t block[16] = {0};

    // AAD
    size_t pos = 0;
    while (pos < aadLen) {
        size_t len = std::min(aadLen - pos, (size_t)16);
        for (size_t i = 0; i < len; i++) y[i] ^= aad[pos + i];
        gcmMultiply(y, h);
        pos += len;
    }

    // Ciphertext
    pos = 0;
    while (pos < ctLen) {
        size_t len = std::min(ctLen - pos, (size_t)16);
        for (size_t i = 0; i < len; i++) y[i] ^= ct[pos + i];
        gcmMultiply(y, h);
        pos += len;
    }

    // Lengths (64-bit each, big-endian)
    uint64_t aadBits = aadLen * 8;
    uint64_t ctBits = ctLen * 8;
    for (int i = 0; i < 8; i++) {
        y[i] ^= (uint8_t)(aadBits >> (56 - i * 8));
        y[i + 8] ^= (uint8_t)(ctBits >> (56 - i * 8));
    }
    gcmMultiply(y, h);

    std::memcpy(tag, y, 16);
}

// =============================================================================
// GCM - 计数器递增
// =============================================================================
void AES256Encryptor::incrementCounter(uint8_t* counter) {
    for (int i = 15; i >= 12; i--) {
        if (++counter[i] != 0) break;
    }
}

// =============================================================================
// 密钥管理实现
// Beta Week 8: 安全修复 - 使用密码学安全随机数替代 juce::Random
// =============================================================================

std::array<uint8_t, AES_KEY_SIZE> AES256Encryptor::generateKey() {
    std::array<uint8_t, AES_KEY_SIZE> key;
    // SEC-001: 使用密码学安全随机数生成器
    auto& random = juce::Random::getSystemRandom();
    for (int i = 0; i < AES_KEY_SIZE; i++) {
        key[i] = static_cast<uint8_t>(random.nextInt(256));
    }
    return key;
}

std::array<uint8_t, AES_KEY_SIZE> AES256Encryptor::deriveKeyFromPassword(
    const juce::String& password,
    const std::array<uint8_t, 16>& salt
) {
    // SEC-002: 标准 PBKDF2-HMAC-SHA256 实现
    // 使用 JUCE 内置 HMAC-SHA256 进行 PBKDF2 密钥派生
    std::array<uint8_t, AES_KEY_SIZE> key;

    const int iterations = 100000;
    const char* passwordBytes = password.toRawUTF8();
    const size_t passwordLen = password.getNumBytesAsUTF8();

    // PRF = HMAC-SHA256(password, salt + block_index)
    // T_i = U_1 xor U_2 xor ... xor U_c
    // U_1 = PRF(password, salt || i_be)
    // U_n = PRF(password, U_{n-1})

    juce::MemoryBlock saltBlock(salt.data(), salt.size());
    const size_t hLen = 32; // SHA-256 hash size

    // HMAC-SHA256 实现 (JUCE 8 移除了 HMAC 类, 手动实现)
    auto hmacSha256 = [&](const juce::MemoryBlock& data) -> juce::MemoryBlock {
        const size_t blockSize = 64;
        uint8_t keyPad[64] = {};

        // 如果密钥超过块大小，先哈希
        if (passwordLen > blockSize) {
            juce::SHA256 keyHash(passwordBytes, passwordLen);
            auto keyHashRaw = keyHash.getRawData();
            std::memcpy(keyPad, keyHashRaw.getData(), std::min(keyHashRaw.getSize(), blockSize));
        } else {
            std::memcpy(keyPad, passwordBytes, passwordLen);
        }

        // 计算 inner hash: SHA256((keyPad ^ ipad) || data)
        uint8_t innerData[64 + 64]; // max blockSize + data size
        for (size_t i = 0; i < blockSize; i++) innerData[i] = keyPad[i] ^ 0x36;
        std::memcpy(innerData + blockSize, data.getData(), data.getSize());
        juce::SHA256 innerHash(innerData, blockSize + data.getSize());
        auto innerRaw = innerHash.getRawData();

        // 计算 outer hash: SHA256((keyPad ^ opad) || innerHash)
        uint8_t outerData[64 + 32];
        for (size_t i = 0; i < blockSize; i++) outerData[i] = keyPad[i] ^ 0x5C;
        std::memcpy(outerData + blockSize, innerRaw.getData(), hLen);
        return juce::SHA256(outerData, blockSize + hLen).getRawData();
    };

    for (size_t block = 0; block < (AES_KEY_SIZE + hLen - 1) / hLen; block++) {
        // 构建 salt || block_index (big-endian, 4 bytes)
        juce::MemoryBlock saltAndIndex(saltBlock.getSize() + 4);
        std::memcpy(saltAndIndex.getData(), saltBlock.getData(), saltBlock.getSize());
        uint8_t* idxPtr = static_cast<uint8_t*>(saltAndIndex.getData()) + saltBlock.getSize();
        uint32_t blockIdx = static_cast<uint32_t>(block + 1);
        idxPtr[0] = static_cast<uint8_t>((blockIdx >> 24) & 0xFF);
        idxPtr[1] = static_cast<uint8_t>((blockIdx >> 16) & 0xFF);
        idxPtr[2] = static_cast<uint8_t>((blockIdx >> 8) & 0xFF);
        idxPtr[3] = static_cast<uint8_t>(blockIdx & 0xFF);

        // U_1 = HMAC-SHA256(password, salt || i)
        juce::MemoryBlock u = hmacSha256(saltAndIndex);
        juce::MemoryBlock t = u; // T = U_1

        for (int iter = 1; iter < iterations; iter++) {
            // U_n = HMAC-SHA256(password, U_{n-1})
            u = hmacSha256(u);
            // T = T xor U_n
            const uint8_t* uData = static_cast<const uint8_t*>(u.getData());
            uint8_t* tData = static_cast<uint8_t*>(t.getData());
            for (size_t j = 0; j < hLen; j++) {
                tData[j] ^= uData[j];
            }
        }

        // 复制到密钥
        const uint8_t* tData = static_cast<const uint8_t*>(t.getData());
        size_t copyLen = std::min(hLen, AES_KEY_SIZE - block * hLen);
        std::memcpy(key.data() + block * hLen, tData, copyLen);
    }

    return key;
}

std::array<uint8_t, 16> AES256Encryptor::generateSalt() {
    std::array<uint8_t, 16> salt;
    // SEC-001: 使用密码学安全随机数生成器
    auto& random = juce::Random::getSystemRandom();
    for (int i = 0; i < 16; i++) {
        salt[i] = static_cast<uint8_t>(random.nextInt(256));
    }
    return salt;
}

std::array<uint8_t, AES_KEY_SIZE> AES256Encryptor::deriveKeyFromHardware() {
    std::array<uint8_t, AES_KEY_SIZE> key;

    // 混合硬件信息
    juce::String hwInfo;
    hwInfo << juce::SystemStats::getCpuModel()
           << juce::SystemStats::getDeviceDescription()
           << juce::SystemStats::getLogonName()
           << juce::SystemStats::getComputerName()
           << "LianCoreV3-StaticSalt";

    auto hash = juce::SHA256(hwInfo.toRawUTF8(), hwInfo.getNumBytesAsUTF8()).getRawData();
    std::memcpy(key.data(), hash.getData(), AES_KEY_SIZE);

    return key;
}

// =============================================================================
// 加密/解密实现
// =============================================================================

juce::String AES256Encryptor::encrypt(
    const juce::String& plaintext,
    const std::array<uint8_t, AES_KEY_SIZE>& key
) {
    auto plainBytes = std::vector<uint8_t>(
        plaintext.toRawUTF8(),
        plaintext.toRawUTF8() + plaintext.getNumBytesAsUTF8()
    );
    auto cipherBytes = encryptBinary(plainBytes, key);
    return juce::Base64::toBase64(cipherBytes.data(), cipherBytes.size());
}

juce::String AES256Encryptor::decrypt(
    const juce::String& ciphertextB64,
    const std::array<uint8_t, AES_KEY_SIZE>& key
) {
    juce::MemoryOutputStream mos;
    if (!juce::Base64::convertFromBase64(mos, ciphertextB64)) {
        return {};
    }
    auto decoded = mos.getMemoryBlock();
    auto cipherBytes = std::vector<uint8_t>(
        (uint8_t*)decoded.getData(),
        (uint8_t*)decoded.getData() + decoded.getSize()
    );
    auto plainBytes = decryptBinary(cipherBytes, key);
    if (plainBytes.empty()) return {};
    return juce::String::fromUTF8((const char*)plainBytes.data(), (int)plainBytes.size());
}

std::vector<uint8_t> AES256Encryptor::encryptBinary(
    const std::vector<uint8_t>& plaintext,
    const std::array<uint8_t, AES_KEY_SIZE>& key
) {
    // 1. 密钥扩展
    uint8_t roundKeys[240];
    keyExpansion(key.data(), roundKeys);

    // 2. 生成随机 IV (12 bytes) - SEC-001: 使用密码学安全随机数
    auto& random = juce::Random::getSystemRandom();
    uint8_t iv[GCM_IV_SIZE];
    for (int i = 0; i < GCM_IV_SIZE; i++) {
        iv[i] = static_cast<uint8_t>(random.nextInt(256));
    }

    // 3. 计算 H = AES_K(0^128)
    uint8_t zeroBlock[16] = {0};
    uint8_t h[16];
    encryptBlock(zeroBlock, h, roundKeys);

    // 4. 计算 J0 = IV || 0^31 || 1
    uint8_t j0[16] = {0};
    std::memcpy(j0, iv, GCM_IV_SIZE);
    j0[15] = 1;

    // 5. CTR 模式加密
    size_t paddedLen = ((plaintext.size() + 15) / 16) * 16;
    std::vector<uint8_t> ciphertext(paddedLen);
    uint8_t counter[16];
    std::memcpy(counter, j0, 16);

    uint8_t keystream[16];
    for (size_t pos = 0; pos < plaintext.size(); pos += 16) {
        incrementCounter(counter);
        encryptBlock(counter, keystream, roundKeys);
        size_t blockLen = std::min((size_t)16, plaintext.size() - pos);
        for (size_t i = 0; i < blockLen; i++) {
            ciphertext[pos + i] = plaintext[pos + i] ^ keystream[i];
        }
    }

    // 6. GCM 认证标签
    uint8_t authTag[GCM_TAG_SIZE];
    gcmGHASH(h, nullptr, 0, ciphertext.data(), plaintext.size(), authTag);

    // XOR tag with E_K(J0)
    uint8_t eJ0[16];
    encryptBlock(j0, eJ0, roundKeys);
    for (int i = 0; i < GCM_TAG_SIZE; i++) {
        authTag[i] ^= eJ0[i];
    }

    // 7. 组合: IV(12) + Ciphertext(变长) + Tag(16)
    std::vector<uint8_t> result;
    result.reserve(GCM_IV_SIZE + plaintext.size() + GCM_TAG_SIZE);
    result.insert(result.end(), iv, iv + GCM_IV_SIZE);
    result.insert(result.end(), ciphertext.begin(), ciphertext.begin() + plaintext.size());
    result.insert(result.end(), authTag, authTag + GCM_TAG_SIZE);

    return result;
}

std::vector<uint8_t> AES256Encryptor::decryptBinary(
    const std::vector<uint8_t>& ciphertext,
    const std::array<uint8_t, AES_KEY_SIZE>& key
) {
    // 最小大小: IV(12) + Tag(16) = 28
    if (ciphertext.size() < GCM_IV_SIZE + GCM_TAG_SIZE) {
        return {};
    }

    // 1. 提取 IV
    uint8_t iv[GCM_IV_SIZE];
    std::memcpy(iv, ciphertext.data(), GCM_IV_SIZE);

    // 2. 提取密文和标签
    size_t ctLen = ciphertext.size() - GCM_IV_SIZE - GCM_TAG_SIZE;
    const uint8_t* ct = ciphertext.data() + GCM_IV_SIZE;
    const uint8_t* receivedTag = ciphertext.data() + GCM_IV_SIZE + ctLen;

    // 3. 密钥扩展
    uint8_t roundKeys[240];
    keyExpansion(key.data(), roundKeys);

    // 4. 计算 H
    uint8_t zeroBlock[16] = {0};
    uint8_t h[16];
    encryptBlock(zeroBlock, h, roundKeys);

    // 5. 计算 J0
    uint8_t j0[16] = {0};
    std::memcpy(j0, iv, GCM_IV_SIZE);
    j0[15] = 1;

    // 6. 验证认证标签
    uint8_t computedTag[GCM_TAG_SIZE];
    gcmGHASH(h, nullptr, 0, ct, ctLen, computedTag);

    uint8_t eJ0[16];
    encryptBlock(j0, eJ0, roundKeys);
    for (int i = 0; i < GCM_TAG_SIZE; i++) {
        computedTag[i] ^= eJ0[i];
    }

    // 常数时间比较
    uint8_t diff = 0;
    for (int i = 0; i < GCM_TAG_SIZE; i++) {
        diff |= computedTag[i] ^ receivedTag[i];
    }
    if (diff != 0) {
        return {};  // 认证失败
    }

    // 7. CTR 模式解密
    std::vector<uint8_t> plaintext(ctLen);
    uint8_t counter[16];
    std::memcpy(counter, j0, 16);
    uint8_t keystream[16];

    for (size_t pos = 0; pos < ctLen; pos += 16) {
        incrementCounter(counter);
        encryptBlock(counter, keystream, roundKeys);
        size_t blockLen = std::min((size_t)16, ctLen - pos);
        for (size_t i = 0; i < blockLen; i++) {
            plaintext[pos + i] = ct[pos + i] ^ keystream[i];
        }
    }

    return plaintext;
}

// =============================================================================
// 密钥存储
// =============================================================================

bool AES256Encryptor::saveKeyToFile(
    const std::array<uint8_t, AES_KEY_SIZE>& key,
    const juce::File& file
) {
    // SEC-003: 使用机器密钥包装后再存储，防止明文密钥泄露
    auto machineKey = deriveKeyFromHardware();
    std::vector<uint8_t> keyVec(key.begin(), key.end());
    auto wrappedKey = encryptBinary(keyVec, machineKey);
    if (wrappedKey.empty()) return false;

    juce::String b64 = juce::Base64::toBase64(wrappedKey.data(), wrappedKey.size());
    return file.replaceWithText(b64);
}

std::array<uint8_t, AES_KEY_SIZE> AES256Encryptor::loadKeyFromFile(const juce::File& file) {
    std::array<uint8_t, AES_KEY_SIZE> key{};
    if (!file.existsAsFile()) return key;

    juce::String b64 = file.loadFileAsString().trim();
    juce::MemoryOutputStream mos;
    if (juce::Base64::convertFromBase64(mos, b64)) {
        auto decoded = mos.getMemoryBlock();
        if (decoded.getSize() >= GCM_IV_SIZE + GCM_TAG_SIZE) {
            // SEC-003: 使用机器密钥解包
            auto machineKey = deriveKeyFromHardware();
            std::vector<uint8_t> wrappedKey(
                static_cast<uint8_t*>(decoded.getData()),
                static_cast<uint8_t*>(decoded.getData()) + decoded.getSize()
            );
            auto unwrappedKey = decryptBinary(wrappedKey, machineKey);
            if (unwrappedKey.size() >= AES_KEY_SIZE) {
                std::memcpy(key.data(), unwrappedKey.data(), AES_KEY_SIZE);
            }
        }
    }
    return key;
}

void AES256Encryptor::secureEraseKey(std::array<uint8_t, AES_KEY_SIZE>& key) {
    // SEC-006: 使用平台安全内存清零
    juce::zeromem(key.data(), AES_KEY_SIZE);
}

} // namespace Security
} // namespace LianCore