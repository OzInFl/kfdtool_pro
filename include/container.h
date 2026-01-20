#pragma once

/**
 * @file container.h
 * @brief Encrypted Key Container Model
 * 
 * Manages encrypted key containers with:
 * - AES-256 encrypted storage (FIPS-compliant)
 * - Multiple containers with groups and keys
 * - SD card backup/restore
 * - JSON serialization compatible with KFDtool desktop
 * - Password-protected containers
 */

#include <stdint.h>
#include <string>
#include <vector>
#include "p25_defs.h"

// =============================================================================
// Key Slot - Individual key within a group
// =============================================================================
struct KeySlot {
    std::string name;           // Key name/label
    std::string description;    // Optional description
    uint8_t     algorithmId;    // P25::AlgorithmId
    uint16_t    keyId;          // Key ID
    uint16_t    sln;            // Storage Location Number (CKR)
    std::string keyHex;         // Key material as hex string
    bool        selected;       // Selected for keyload
    
    KeySlot() : algorithmId(P25::ALGO_AES_256), keyId(1), sln(1), selected(true) {}
    
    // Convert to P25::KeyItem for protocol operations
    P25::KeyItem toKeyItem(uint16_t keysetId = 1) const;
    
    // Validate key for algorithm
    bool isValid() const;
    
    // Get key length in bytes
    size_t getKeyLength() const;
};

// =============================================================================
// Key Group - Collection of related keys
// =============================================================================
struct KeyGroup {
    std::string name;           // Group name
    std::string description;    // Optional description
    uint16_t    keysetId;       // Keyset ID for this group
    bool        useActiveKeyset; // Use active keyset instead of keysetId
    std::vector<KeySlot> keys;  // Keys in this group
    bool        expanded;       // UI state: expanded in tree
    
    KeyGroup() : keysetId(1), useActiveKeyset(true), expanded(true) {}
    
    // Get all selected keys
    std::vector<KeySlot> getSelectedKeys() const;
    
    // Count selected keys
    size_t selectedCount() const;
};

// =============================================================================
// Container - Complete key container with metadata
// =============================================================================
struct Container {
    // Metadata
    std::string name;           // Container name
    std::string description;    // Optional description
    std::string agency;         // Agency/organization
    std::string system;         // Radio system name
    std::string createdDate;    // Creation timestamp
    std::string modifiedDate;   // Last modified timestamp
    
    // Groups and keys
    std::vector<KeyGroup> groups;
    
    // Security settings
    bool        isLocked;       // Container is locked (no edits)
    bool        isEncrypted;    // Container is encrypted
    std::string passwordHash;   // SHA-256 hash of password (for verification)
    
    Container() : isLocked(false), isEncrypted(false) {}
    
    // Get all keys (flattened)
    std::vector<KeySlot> getAllKeys() const;
    
    // Get all selected keys
    std::vector<KeySlot> getSelectedKeys() const;
    
    // Count total keys
    size_t totalKeyCount() const;
    
    // Count selected keys
    size_t selectedKeyCount() const;
    
    // Update modified timestamp
    void touch();
};

// =============================================================================
// Container Manager - Singleton for container operations
// =============================================================================
class ContainerManager {
public:
    static ContainerManager& instance();
    
    // Initialization
    bool init();
    
    // Container CRUD
    size_t getContainerCount() const { return _containers.size(); }
    const Container& getContainer(size_t index) const;
    Container& getMutableContainer(size_t index);
    int addContainer(const Container& c);
    bool updateContainer(size_t index, const Container& c);
    bool deleteContainer(size_t index);
    
    // Active container
    int getActiveIndex() const { return _activeIndex; }
    bool setActiveIndex(int index);
    const Container* getActiveContainer() const;
    Container* getActiveContainerMutable();
    
    // Group operations
    bool addGroup(size_t containerIdx, const KeyGroup& group);
    bool updateGroup(size_t containerIdx, size_t groupIdx, const KeyGroup& group);
    bool deleteGroup(size_t containerIdx, size_t groupIdx);
    
    // Key operations
    bool addKey(size_t containerIdx, size_t groupIdx, const KeySlot& key);
    bool updateKey(size_t containerIdx, size_t groupIdx, size_t keyIdx, const KeySlot& key);
    bool deleteKey(size_t containerIdx, size_t groupIdx, size_t keyIdx);
    
    // Persistence (encrypted storage)
    bool load();                              // Load from internal storage
    bool save();                              // Save to internal storage
    bool saveNow();                           // Immediate save
    
    // Password protection
    bool setPassword(const std::string& password);
    bool verifyPassword(const std::string& password) const;
    bool isPasswordSet() const { return !_password.empty(); }
    void clearPassword() { _password.clear(); }
    
    // SD Card backup/restore
    bool backupToSd(const char* filename);
    bool restoreFromSd(const char* filename);
    bool listSdBackups(std::vector<std::string>& files);
    
    // Export/Import (unencrypted JSON for compatibility)
    bool exportToJson(const char* filename);
    bool importFromJson(const char* filename);
    
    // Factory reset
    bool factoryReset();
    void loadDefaults();
    
    // Autosave service (call from loop)
    void service();
    
private:
    ContainerManager();
    ContainerManager(const ContainerManager&) = delete;
    ContainerManager& operator=(const ContainerManager&) = delete;
    
    std::vector<Container> _containers;
    int _activeIndex;
    std::string _password;  // Current session password (not persisted)
    
    // Persistence state
    bool _storageReady;
    bool _dirty;
    uint32_t _lastChangeMs;
    uint32_t _lastSaveMs;
    
    // Internal methods
    bool ensureStorage();
    bool loadFromStorage();
    bool saveToStorage();
    std::string serializeContainers();
    bool deserializeContainers(const std::string& json);
    bool encryptAndSave(const std::string& plaintext);
    bool loadAndDecrypt(std::string& plaintext);
};

// =============================================================================
// Key Generation Utilities
// =============================================================================
namespace KeyGen {
    // Generate random key for algorithm
    std::string generateKey(uint8_t algorithmId);
    
    // Generate random AES-256 key
    std::string generateAes256();
    
    // Generate random DES key (with proper parity)
    std::string generateDes();
    
    // Generate random 3DES key (2-key)
    std::string generate3Des2Key();
    
    // Generate random 3DES key (3-key)
    std::string generate3Des3Key();
    
    // Validate key hex string
    bool validateKeyHex(const std::string& hex, uint8_t algorithmId);
    
    // Fix DES parity in hex string
    std::string fixDesParityHex(const std::string& hex);
}
