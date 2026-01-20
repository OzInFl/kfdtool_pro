#pragma once

/**
 * @file crypto.h
 * @brief AES-256 Encryption Module for KFDtool Professional
 * 
 * Provides FIPS-compliant cryptographic operations for:
 * - Container encryption/decryption
 * - Key derivation from PIN
 * - Secure random number generation
 * - Key validation
 */

#include <stdint.h>
#include <stddef.h>
#include <string>
#include <vector>

// AES-256 key size (32 bytes = 256 bits)
#define AES256_KEY_SIZE 32
// AES block size (16 bytes = 128 bits)
#define AES_BLOCK_SIZE 16
// PBKDF2 iteration count (FIPS 140-2 recommends minimum 1000)
#define PBKDF2_ITERATIONS 10000
// Salt size for key derivation
#define SALT_SIZE 16
// HMAC-SHA256 size
#define HMAC_SIZE 32

namespace Crypto {

/**
 * @brief Initialize the crypto module
 * @return true if initialization successful
 */
bool init();

/**
 * @brief Generate cryptographically secure random bytes
 * @param buffer Output buffer
 * @param length Number of bytes to generate
 * @return true if successful
 */
bool generateRandom(uint8_t* buffer, size_t length);

/**
 * @brief Generate a random AES-256 key (for TEK/KEK generation)
 * @param key Output buffer (must be AES256_KEY_SIZE bytes)
 * @return true if successful
 */
bool generateKey256(uint8_t* key);

/**
 * @brief Generate a random DES key with proper odd parity
 * @param key Output buffer (must be 8 bytes)
 * @return true if successful
 */
bool generateKeyDES(uint8_t* key);

/**
 * @brief Derive an encryption key from a password using PBKDF2-SHA256
 * @param password User password/PIN
 * @param salt Salt value (should be random, stored with ciphertext)
 * @param saltLen Length of salt
 * @param derivedKey Output buffer (must be AES256_KEY_SIZE bytes)
 * @return true if successful
 */
bool deriveKey(const char* password, const uint8_t* salt, size_t saltLen, 
               uint8_t* derivedKey);

/**
 * @brief Encrypt data using AES-256-CBC with PKCS7 padding
 * @param plaintext Input data
 * @param plaintextLen Length of input
 * @param key 256-bit encryption key
 * @param iv 128-bit initialization vector (will be generated if NULL)
 * @param ciphertext Output buffer (must be at least plaintextLen + AES_BLOCK_SIZE)
 * @param ciphertextLen Output: actual ciphertext length
 * @return true if successful
 */
bool encrypt(const uint8_t* plaintext, size_t plaintextLen,
             const uint8_t* key, uint8_t* iv,
             uint8_t* ciphertext, size_t* ciphertextLen);

/**
 * @brief Decrypt data using AES-256-CBC with PKCS7 padding
 * @param ciphertext Input encrypted data
 * @param ciphertextLen Length of ciphertext
 * @param key 256-bit decryption key
 * @param iv 128-bit initialization vector
 * @param plaintext Output buffer
 * @param plaintextLen Output: actual plaintext length
 * @return true if successful (including padding validation)
 */
bool decrypt(const uint8_t* ciphertext, size_t ciphertextLen,
             const uint8_t* key, const uint8_t* iv,
             uint8_t* plaintext, size_t* plaintextLen);

/**
 * @brief Calculate HMAC-SHA256 for data integrity
 * @param data Input data
 * @param dataLen Length of data
 * @param key HMAC key
 * @param keyLen Length of key
 * @param hmac Output buffer (must be HMAC_SIZE bytes)
 * @return true if successful
 */
bool hmacSha256(const uint8_t* data, size_t dataLen,
                const uint8_t* key, size_t keyLen,
                uint8_t* hmac);

/**
 * @brief Calculate SHA-256 hash
 * @param data Input data
 * @param dataLen Length of data
 * @param hash Output buffer (must be 32 bytes)
 * @return true if successful
 */
bool sha256(const uint8_t* data, size_t dataLen, uint8_t* hash);

/**
 * @brief Fix DES key parity bits (odd parity per byte)
 * @param key DES key (8 bytes), modified in place
 */
void fixDesKeyParity(uint8_t* key);

/**
 * @brief Validate DES key parity
 * @param key DES key to validate
 * @return true if parity is correct
 */
bool validateDesKeyParity(const uint8_t* key);

/**
 * @brief Calculate CRC-16 (CCITT) for TWI protocol
 * @param data Input data
 * @param len Data length
 * @return CRC-16 value
 */
uint16_t crc16(const uint8_t* data, size_t len);

/**
 * @brief Securely zero memory (prevents compiler optimization)
 * @param ptr Pointer to memory
 * @param len Length to zero
 */
void secureZero(void* ptr, size_t len);

/**
 * @brief Convert hex string to bytes
 * @param hex Input hex string
 * @param bytes Output buffer
 * @param maxLen Maximum output length
 * @return Number of bytes written, or -1 on error
 */
int hexToBytes(const std::string& hex, uint8_t* bytes, size_t maxLen);

/**
 * @brief Convert bytes to hex string
 * @param bytes Input bytes
 * @param len Number of bytes
 * @return Hex string (uppercase)
 */
std::string bytesToHex(const uint8_t* bytes, size_t len);

/**
 * @brief Validate key for specified algorithm
 * @param algo Algorithm ID (0x81=DES-OFB, 0x84=AES-256, etc)
 * @param key Key bytes
 * @param keyLen Key length
 * @return true if key is valid for algorithm
 */
bool validateKeyForAlgorithm(uint8_t algo, const uint8_t* key, size_t keyLen);

/**
 * @brief Get expected key length for algorithm
 * @param algo Algorithm ID
 * @return Expected key length in bytes, or 0 if unknown
 */
size_t getKeyLengthForAlgorithm(uint8_t algo);

} // namespace Crypto
