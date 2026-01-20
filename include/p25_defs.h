#pragma once

/**
 * @file p25_defs.h
 * @brief P25 TIA-102.AACD-A Protocol Definitions for KFDtool
 * 
 * Defines constants, message IDs, and structures for:
 * - Key Management Messages (KMM)
 * - Three-Wire Interface (TWI/3WI) protocol
 * - Algorithm IDs
 * - Operation status codes
 */

#include <stdint.h>
#include <vector>
#include <string>

namespace P25 {

// =============================================================================
// Algorithm IDs (TIA-102.AACD-A Table 7.1)
// =============================================================================
enum AlgorithmId : uint8_t {
    ALGO_ACCORDION_1_3   = 0x00,  // Type 1
    ALGO_BATON_AUTO      = 0x01,  // Type 1 Auto
    ALGO_FIREFLY_TYPE1   = 0x02,  // Type 1 Firefly
    ALGO_MAYFLY_TYPE1    = 0x03,  // Type 1 Mayfly
    ALGO_SAVILLE         = 0x04,  // Type 1 Saville
    ALGO_PADSTONE        = 0x05,  // Type 1 Padstone
    ALGO_ACCORDION_4     = 0x41,  // Type 3
    ALGO_BATON_TYPE3     = 0x42,  // Type 3
    ALGO_DES_OFB         = 0x81,  // DES-OFB (Type 3)
    ALGO_2_KEY_3DES      = 0x82,  // 2-key Triple DES
    ALGO_3_KEY_3DES      = 0x83,  // 3-key Triple DES
    ALGO_AES_256         = 0x84,  // AES-256 (Type 3)
    ALGO_AES_128         = 0x85,  // AES-128
    ALGO_AES_CBC         = 0x86,  // AES-CBC
    ALGO_ARC4            = 0x9F,  // ARC4 (legacy)
    ALGO_ADP             = 0xAA,  // Motorola ADP
    ALGO_CLEAR           = 0x80,  // Unencrypted
};

// Get algorithm name string
inline const char* getAlgorithmName(uint8_t algo) {
    switch (algo) {
        case ALGO_DES_OFB:     return "DES-OFB";
        case ALGO_2_KEY_3DES:  return "2-KEY 3DES";
        case ALGO_3_KEY_3DES:  return "3-KEY 3DES";
        case ALGO_AES_256:     return "AES-256";
        case ALGO_AES_128:     return "AES-128";
        case ALGO_AES_CBC:     return "AES-CBC";
        case ALGO_ARC4:        return "ARC4";
        case ALGO_ADP:         return "ADP";
        case ALGO_CLEAR:       return "CLEAR";
        default:               return "UNKNOWN";
    }
}

// Get expected key length for algorithm
inline size_t getKeyLength(uint8_t algo) {
    switch (algo) {
        case ALGO_DES_OFB:     return 8;   // 64 bits
        case ALGO_2_KEY_3DES:  return 16;  // 128 bits
        case ALGO_3_KEY_3DES:  return 24;  // 192 bits
        case ALGO_AES_128:     return 16;  // 128 bits
        case ALGO_AES_256:     return 32;  // 256 bits
        case ALGO_AES_CBC:     return 32;  // 256 bits
        case ALGO_ARC4:        return 13;  // Variable, typically 104 bits
        case ALGO_ADP:         return 5;   // 40 bits
        default:               return 0;
    }
}

// =============================================================================
// KMM Message IDs (TIA-102.AACD-A Table 7.2)
// =============================================================================
enum MessageId : uint8_t {
    // Commands (KFD -> MR)
    MSG_INVENTORY_CMD             = 0x00,
    MSG_MODIFY_KEY_CMD            = 0x04,
    MSG_REKEY_ACK                 = 0x07,
    MSG_ZEROIZE_CMD               = 0x0A,
    MSG_SESSION_CONTROL           = 0x0B,
    MSG_LOAD_CONFIG_CMD           = 0x0C,
    MSG_CHANGEOVER_CMD            = 0x0D,
    MSG_CHANGE_RSI_CMD            = 0x0E,
    
    // Responses (MR -> KFD)
    MSG_INVENTORY_RSP             = 0x01,
    MSG_NEGATIVE_ACK              = 0x08,
    MSG_ZEROIZE_RSP               = 0x0F,
    MSG_LOAD_CONFIG_RSP           = 0x10,
    MSG_CHANGEOVER_RSP            = 0x11,
    MSG_CHANGE_RSI_RSP            = 0x12,
};

// =============================================================================
// Inventory Types (from KFDtool InventoryType.cs)
// =============================================================================
enum InventoryType : uint8_t {
    INV_NULL                      = 0x00,
    INV_SEND_CURRENT_DATETIME     = 0x01,
    INV_LIST_ACTIVE_KSET_IDS      = 0x02,
    INV_LIST_INACTIVE_KSET_IDS    = 0x03,
    INV_LIST_ACTIVE_KEY_IDS       = 0x04,
    INV_LIST_INACTIVE_KEY_IDS     = 0x05,
    INV_LIST_ALL_KSET_TAGGING     = 0x06,
    INV_LIST_ALL_UNIQUE_KEY_INFO  = 0x07,
    INV_LIST_KSET_TAGGING         = 0xF9,
    INV_LIST_ACTIVE_KEYS          = 0xFD,  // This returns full key info
    INV_LIST_MNP                  = 0xFE,
    INV_LIST_KMF_RSI              = 0xFF,
};

// =============================================================================
// Operation Status Codes (TIA-102.AACD-A Table 7.4)
// =============================================================================
enum OperationStatus : uint8_t {
    STATUS_COMMAND_PERFORMED      = 0x00,
    STATUS_KEY_NOT_LOADED         = 0x01,
    STATUS_KEY_OVERWRITTEN        = 0x02,
    STATUS_KEY_STORAGE_FULL       = 0x03,
    STATUS_KEY_PREVIOUSLY_ERASED  = 0x04,
    STATUS_INVALID_MESSAGE_ID     = 0x05,
    STATUS_INVALID_MAC            = 0x06,
    STATUS_INVALID_CRYPTO_HEADER  = 0x07,
    STATUS_INVALID_KEY_ID         = 0x08,
    STATUS_INVALID_ALGO_ID        = 0x09,
    STATUS_INVALID_MN             = 0x0A,
    STATUS_INVALID_KEY_LENGTH     = 0x0B,
    STATUS_INVALID_KEYSET_ID      = 0x0C,
    STATUS_UNSUPPORTED_FEATURE    = 0x0D,
    STATUS_KEYSET_NOT_FOUND       = 0x0E,
    STATUS_ALGO_NOT_SUPPORTED     = 0x0F,
    STATUS_KEY_NOT_FOUND          = 0x10,
    STATUS_INTERNAL_ERROR         = 0xFF,
};

// =============================================================================
// Three-Wire Interface (TWI/3WI) Protocol Opcodes
// =============================================================================
enum TwiOpcode : uint8_t {
    TWI_READY_REQ           = 0xC0,  // KFD -> MR: Ready Request
    TWI_READY_MODE_MR       = 0xD0,  // MR -> KFD: Ready (General Mode, MR)
    TWI_READY_MODE_KVL      = 0xD1,  // MR -> KFD: Ready (General Mode, KVL)
    TWI_TRANSFER_DONE       = 0xC1,  // Bidirectional: Transfer Done
    TWI_KMM                 = 0xC2,  // KMM frame follows
    TWI_DISCONNECT          = 0x92,  // KFD -> MR: Disconnect
    TWI_DISCONNECT_ACK      = 0x90,  // MR -> KFD: Disconnect Acknowledge
};

// =============================================================================
// Key Types
// =============================================================================
enum KeyType : uint8_t {
    KEY_TYPE_TEK = 0,  // Traffic Encryption Key (SLN 0-61439)
    KEY_TYPE_KEK = 1,  // Key Encryption Key (SLN 61440-65535)
};

inline KeyType getKeyType(uint16_t sln) {
    return (sln >= 0xF000) ? KEY_TYPE_KEK : KEY_TYPE_TEK;
}

// =============================================================================
// Data Structures
// =============================================================================

// Key item for keyload operations
struct KeyItem {
    uint16_t keysetId;
    uint16_t sln;         // Storage Location Number (CKR)
    uint16_t keyId;       // Key ID
    uint8_t  algorithmId;
    std::vector<uint8_t> key;
    bool     isKek;       // true if KEK, false if TEK
    bool     erase;       // true for erase operation
    
    KeyItem() : keysetId(1), sln(0), keyId(0), algorithmId(ALGO_AES_256), 
                isKek(false), erase(false) {}
};

// Key status response
struct KeyStatus {
    uint16_t keyId;
    uint8_t  algorithmId;
    uint8_t  status;      // OperationStatus
};

// RSI (Radio System Identity) item
struct RsiItem {
    uint32_t rsi;         // 24-bit RSI
    uint16_t messageNumber;
};

// Keyset information
struct KeysetInfo {
    uint8_t  keysetId;
    std::string name;
    uint8_t  algorithmId;
    bool     active;
};

// MNP (Message Number Pair)
struct MnpInfo {
    uint32_t rsi;
    uint16_t mn;
};

// KMF RSI item
struct KmfRsiItem {
    uint32_t rsi;
    uint16_t mn;
};

// =============================================================================
// CRC-16 CCITT for KMM frames
// =============================================================================
uint16_t calculateCrc16(const uint8_t* data, size_t len);

// =============================================================================
// Validation Functions
// =============================================================================
bool validateKeysetId(int keysetId);
bool validateSln(int sln, bool isKek);
bool validateKeyId(int keyId);
bool validateAlgorithmId(int algoId);
bool validateKey(const std::vector<uint8_t>& key, uint8_t algoId);

} // namespace P25
