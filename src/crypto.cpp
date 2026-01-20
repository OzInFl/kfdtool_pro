/**
 * @file crypto.cpp
 * @brief Cryptographic Functions Implementation
 */

#include "crypto.h"
#include <Arduino.h>
#include <esp_random.h>
#include <mbedtls/sha256.h>
#include <mbedtls/aes.h>
#include <mbedtls/md.h>
#include <string.h>

namespace Crypto {

bool init() {
    return true;
}

bool generateRandom(uint8_t* buffer, size_t length) {
    if (!buffer || length == 0) return false;
    esp_fill_random(buffer, length);
    return true;
}

bool generateKey256(uint8_t* key) {
    return generateRandom(key, AES256_KEY_SIZE);
}

bool generateKeyDES(uint8_t* key) {
    if (!generateRandom(key, 8)) return false;
    fixDesKeyParity(key);
    return true;
}

bool deriveKey(const char* password, const uint8_t* salt, size_t saltLen,
               uint8_t* derivedKey) {
    if (!password || !salt || !derivedKey) return false;
    
    // Manual PBKDF2-HMAC-SHA256 implementation
    // For simplicity, using a single iteration approach with salt+password hash
    // This is a simplified version - full PBKDF2 would require more iterations
    
    size_t pwLen = strlen(password);
    size_t totalLen = saltLen + pwLen + 4;
    uint8_t* input = (uint8_t*)malloc(totalLen);
    if (!input) return false;
    
    // Concatenate: salt + password + counter
    memcpy(input, salt, saltLen);
    memcpy(input + saltLen, password, pwLen);
    input[saltLen + pwLen] = 0;
    input[saltLen + pwLen + 1] = 0;
    input[saltLen + pwLen + 2] = 0;
    input[saltLen + pwLen + 3] = 1;
    
    // Initial hash
    uint8_t U[32];
    if (!hmacSha256(input, totalLen, (const uint8_t*)password, pwLen, U)) {
        free(input);
        return false;
    }
    
    memcpy(derivedKey, U, AES256_KEY_SIZE);
    
    // Perform iterations (simplified - doing 1000 for balance of security/speed)
    for (int i = 1; i < 1000; i++) {
        uint8_t T[32];
        if (!hmacSha256(U, 32, (const uint8_t*)password, pwLen, T)) {
            free(input);
            return false;
        }
        for (int j = 0; j < 32; j++) {
            derivedKey[j] ^= T[j];
            U[j] = T[j];
        }
    }
    
    free(input);
    return true;
}

bool encrypt(const uint8_t* plaintext, size_t plaintextLen,
             const uint8_t* key, uint8_t* iv,
             uint8_t* ciphertext, size_t* ciphertextLen) {
    if (!plaintext || !key || !ciphertext || !ciphertextLen) return false;
    
    // Generate IV if not provided
    uint8_t localIv[AES_BLOCK_SIZE];
    if (iv) {
        memcpy(localIv, iv, AES_BLOCK_SIZE);
    } else {
        generateRandom(localIv, AES_BLOCK_SIZE);
    }
    
    // PKCS7 padding
    size_t padLen = AES_BLOCK_SIZE - (plaintextLen % AES_BLOCK_SIZE);
    size_t paddedLen = plaintextLen + padLen;
    
    uint8_t* padded = (uint8_t*)malloc(paddedLen);
    if (!padded) return false;
    
    memcpy(padded, plaintext, plaintextLen);
    memset(padded + plaintextLen, padLen, padLen);
    
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    
    int ret = mbedtls_aes_setkey_enc(&ctx, key, 256);
    if (ret != 0) {
        mbedtls_aes_free(&ctx);
        free(padded);
        return false;
    }
    
    ret = mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_ENCRYPT, paddedLen, localIv, padded, ciphertext);
    
    mbedtls_aes_free(&ctx);
    free(padded);
    
    if (ret == 0) {
        *ciphertextLen = paddedLen;
        if (iv) memcpy(iv, localIv, AES_BLOCK_SIZE);
        return true;
    }
    return false;
}

bool decrypt(const uint8_t* ciphertext, size_t ciphertextLen,
             const uint8_t* key, const uint8_t* iv,
             uint8_t* plaintext, size_t* plaintextLen) {
    if (!ciphertext || !key || !iv || !plaintext || !plaintextLen) return false;
    if (ciphertextLen == 0 || ciphertextLen % AES_BLOCK_SIZE != 0) return false;
    
    uint8_t localIv[AES_BLOCK_SIZE];
    memcpy(localIv, iv, AES_BLOCK_SIZE);
    
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    
    int ret = mbedtls_aes_setkey_dec(&ctx, key, 256);
    if (ret != 0) {
        mbedtls_aes_free(&ctx);
        return false;
    }
    
    ret = mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_DECRYPT, ciphertextLen, localIv, ciphertext, plaintext);
    mbedtls_aes_free(&ctx);
    
    if (ret != 0) return false;
    
    // Remove PKCS7 padding
    uint8_t padLen = plaintext[ciphertextLen - 1];
    if (padLen > AES_BLOCK_SIZE || padLen == 0) return false;
    
    for (size_t i = 0; i < padLen; i++) {
        if (plaintext[ciphertextLen - 1 - i] != padLen) return false;
    }
    
    *plaintextLen = ciphertextLen - padLen;
    return true;
}

bool hmacSha256(const uint8_t* data, size_t dataLen,
                const uint8_t* key, size_t keyLen,
                uint8_t* hmac) {
    if (!data || !key || !hmac) return false;
    
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    
    const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!md_info) { mbedtls_md_free(&ctx); return false; }
    
    int ret = mbedtls_md_setup(&ctx, md_info, 1);
    if (ret != 0) { mbedtls_md_free(&ctx); return false; }
    
    ret = mbedtls_md_hmac_starts(&ctx, key, keyLen);
    if (ret == 0) ret = mbedtls_md_hmac_update(&ctx, data, dataLen);
    if (ret == 0) ret = mbedtls_md_hmac_finish(&ctx, hmac);
    
    mbedtls_md_free(&ctx);
    return ret == 0;
}

bool sha256(const uint8_t* data, size_t dataLen, uint8_t* hash) {
    if (!data || !hash) return false;
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, data, dataLen);
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);
    return true;
}

void fixDesKeyParity(uint8_t* key) {
    for (int i = 0; i < 8; i++) {
        uint8_t b = key[i];
        int ones = 0;
        for (int j = 0; j < 7; j++) {
            if (b & (1 << (j + 1))) ones++;
        }
        key[i] = (b & 0xFE) | ((ones % 2 == 0) ? 1 : 0);
    }
}

bool validateDesKeyParity(const uint8_t* key) {
    for (int i = 0; i < 8; i++) {
        int ones = 0;
        for (int j = 0; j < 8; j++) {
            if (key[i] & (1 << j)) ones++;
        }
        if (ones % 2 == 0) return false;
    }
    return true;
}

uint16_t crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    return crc;
}

void secureZero(void* ptr, size_t len) {
    volatile uint8_t* p = (volatile uint8_t*)ptr;
    while (len--) *p++ = 0;
}

int hexToBytes(const std::string& hex, uint8_t* bytes, size_t maxLen) {
    size_t len = hex.length() / 2;
    if (bytes == nullptr) return len;
    if (len > maxLen) len = maxLen;
    
    for (size_t i = 0; i < len; i++) {
        char h = hex[i * 2];
        char l = hex[i * 2 + 1];
        uint8_t hi = (h >= 'A' && h <= 'F') ? h - 'A' + 10 :
                     (h >= 'a' && h <= 'f') ? h - 'a' + 10 : h - '0';
        uint8_t lo = (l >= 'A' && l <= 'F') ? l - 'A' + 10 :
                     (l >= 'a' && l <= 'f') ? l - 'a' + 10 : l - '0';
        bytes[i] = (hi << 4) | lo;
    }
    return len;
}

std::string bytesToHex(const uint8_t* bytes, size_t len) {
    static const char hex[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(len * 2);
    for (size_t i = 0; i < len; i++) {
        result += hex[bytes[i] >> 4];
        result += hex[bytes[i] & 0x0F];
    }
    return result;
}

bool validateKeyForAlgorithm(uint8_t algo, const uint8_t* key, size_t keyLen) {
    size_t expected = getKeyLengthForAlgorithm(algo);
    if (expected == 0) return true;
    return keyLen == expected;
}

size_t getKeyLengthForAlgorithm(uint8_t algo) {
    switch (algo) {
        case 0x81: return 8;   // DES-OFB
        case 0x82: return 16;  // 2-key 3DES
        case 0x83: return 24;  // 3-key 3DES
        case 0x84: return 32;  // AES-256
        case 0x85: return 16;  // AES-128
        default: return 0;
    }
}

} // namespace Crypto
