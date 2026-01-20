#pragma once

/**
 * @file device_info.h
 * @brief Device Information, Settings, and User Management
 * 
 * Manages:
 * - Device serial number and identification
 * - User authentication (Operator/Admin roles)
 * - Device settings (pins, timing, display)
 * - Session timeout and auto-lock
 */

#include <stdint.h>
#include <string>

// =============================================================================
// User Roles
// =============================================================================
enum UserRole {
    ROLE_NONE = 0,      // Not logged in
    ROLE_OPERATOR,      // Can view and keyload
    ROLE_ADMIN,         // Full access including settings
};

// =============================================================================
// Device Information
// =============================================================================
struct DeviceInfo {
    // Hardware identification
    char serialNumber[17];      // 16-char serial + null
    char modelNumber[9];        // Model identifier
    char hardwareRev[5];        // Hardware revision
    char firmwareVer[17];       // Firmware version string
    uint32_t uniqueId;          // ESP32 chip ID (last 4 bytes of MAC)
    
    // Runtime info
    uint32_t bootCount;         // Number of boots
    uint32_t uptimeSeconds;     // Current uptime
    uint32_t lastKeyloadTime;   // Timestamp of last keyload
    uint32_t keyloadCount;      // Total keyloads performed
    
    // Initialize with defaults
    void init();
    
    // Generate serial number from chip ID
    void generateSerial();
    
    // Get formatted info string
    std::string getInfoString() const;
};

// =============================================================================
// Device Settings (persisted)
// =============================================================================
struct DeviceSettings {
    // TWI Pin Configuration
    int twiDataPin;         // DATA pin (default: 11)
    int twiSensePin;        // SENSE pin (default: 10)
    
    // TWI Timing
    uint8_t twiTxSpeed;     // TX speed in kbaud (default: 4)
    uint8_t twiRxSpeed;     // RX speed in kbaud (default: 4)
    uint32_t twiTimeout;    // Timeout in ms (default: 5000)
    
    // Display Settings
    uint8_t brightness;     // Display brightness 0-255
    bool autoOff;           // Auto display off
    uint16_t autoOffTimeout; // Timeout in seconds
    
    // Security Settings
    bool requireLogin;      // Require login on startup
    uint16_t sessionTimeout; // Auto-logout timeout (0 = never)
    bool lockOnSleep;       // Lock when display turns off
    uint8_t maxPinAttempts; // Max failed PIN attempts before lockout
    uint16_t lockoutTime;   // Lockout duration in seconds
    
    // User PINs (hashed)
    char operatorPinHash[65]; // SHA-256 hash of operator PIN
    char adminPinHash[65];    // SHA-256 hash of admin PIN
    
    // Container Settings
    bool autoSave;          // Auto-save containers
    uint16_t autoSaveDelay; // Delay before auto-save (seconds)
    bool confirmDelete;     // Require confirmation for deletes
    bool backupOnSave;      // Backup to SD on save
    
    // Initialize with defaults
    void setDefaults();
    
    // Validate settings
    bool validate() const;
};

// =============================================================================
// User Session
// =============================================================================
struct UserSession {
    UserRole role;
    std::string username;
    uint32_t loginTime;
    uint32_t lastActivityTime;
    uint8_t failedAttempts;
    bool lockedOut;
    uint32_t lockoutEndTime;
    
    UserSession() : role(ROLE_NONE), loginTime(0), lastActivityTime(0),
                    failedAttempts(0), lockedOut(false), lockoutEndTime(0) {}
    
    bool isLoggedIn() const { return role != ROLE_NONE; }
    bool isAdmin() const { return role == ROLE_ADMIN; }
    bool isOperator() const { return role == ROLE_OPERATOR; }
    
    // Update activity timestamp
    void touch();
    
    // Check if session has timed out
    bool isTimedOut(uint16_t timeoutSec) const;
};

// =============================================================================
// Device Manager Singleton
// =============================================================================
class DeviceManager {
public:
    static DeviceManager& instance();
    
    // Initialization
    bool init();
    
    // Device info access
    const DeviceInfo& getInfo() const { return _info; }
    DeviceInfo& getMutableInfo() { return _info; }
    
    // Settings access
    const DeviceSettings& getSettings() const { return _settings; }
    DeviceSettings& getMutableSettings() { return _settings; }
    bool saveSettings();
    bool loadSettings();
    void resetSettingsToDefaults();
    
    // User management
    const UserSession& getSession() const { return _session; }
    UserRole getCurrentRole() const { return _session.role; }
    bool isLoggedIn() const { return _session.isLoggedIn(); }
    bool isAdmin() const { return _session.isAdmin(); }
    
    // Authentication
    bool login(UserRole role, const std::string& pin);
    void logout();
    bool changePIN(UserRole role, const std::string& oldPin, const std::string& newPin);
    bool setPIN(UserRole role, const std::string& pin);  // Admin only
    bool verifyPIN(UserRole role, const std::string& pin);
    
    // Check access level
    bool checkAccess(bool requireAdmin, const char* action = nullptr);
    
    // Session management (call from loop)
    void service();
    
    // Activity tracking
    void recordActivity();
    void recordKeyload();
    
    // Factory reset
    bool factoryReset();
    
    // Get serial number for display
    const char* getSerialNumber() const { return _info.serialNumber; }
    
private:
    DeviceManager();
    DeviceManager(const DeviceManager&) = delete;
    DeviceManager& operator=(const DeviceManager&) = delete;
    
    DeviceInfo _info;
    DeviceSettings _settings;
    UserSession _session;
    
    bool _initialized;
    bool _settingsDirty;
    
    // PIN hashing
    std::string hashPIN(const std::string& pin);
    
    // Persistence
    bool saveInfoToStorage();
    bool loadInfoFromStorage();
};

// =============================================================================
// Default PINs (change on first login!)
// =============================================================================
#define DEFAULT_OPERATOR_PIN "1111"
#define DEFAULT_ADMIN_PIN "5000"
