/**
 * @file device_info.cpp
 * @brief Device Information and User Management Implementation
 */

#include "device_info.h"
#include "crypto.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

void DeviceInfo::init() {
    strncpy(modelNumber, "KFD-PRO", sizeof(modelNumber));
    strncpy(hardwareRev, "1.0", sizeof(hardwareRev));
    strncpy(firmwareVer, "1.0.0", sizeof(firmwareVer));
    bootCount = 0;
    uptimeSeconds = 0;
    lastKeyloadTime = 0;
    keyloadCount = 0;
    generateSerial();
}

void DeviceInfo::generateSerial() {
    uint64_t mac = ESP.getEfuseMac();
    uniqueId = (uint32_t)(mac >> 16);
    snprintf(serialNumber, sizeof(serialNumber), "KFD%08X", uniqueId);
}

std::string DeviceInfo::getInfoString() const {
    char buf[256];
    snprintf(buf, sizeof(buf), "S/N: %s\nModel: %s\nHW: %s\nFW: %s\nUID: %08X",
             serialNumber, modelNumber, hardwareRev, firmwareVer, uniqueId);
    return buf;
}

void DeviceSettings::setDefaults() {
    twiDataPin = 12;   // Safe GPIO for WT32-SC01-Plus
    twiSensePin = 13;  // Safe GPIO for WT32-SC01-Plus
    twiTxSpeed = 4;
    twiRxSpeed = 4;
    twiTimeout = 5000;
    brightness = 200;
    autoOff = false;
    autoOffTimeout = 300;
    requireLogin = false;
    sessionTimeout = 0;
    lockOnSleep = false;
    maxPinAttempts = 5;
    lockoutTime = 60;
    memset(operatorPinHash, 0, sizeof(operatorPinHash));
    memset(adminPinHash, 0, sizeof(adminPinHash));
    autoSave = true;
    autoSaveDelay = 3;
    confirmDelete = true;
    backupOnSave = false;
}

bool DeviceSettings::validate() const {
    if (twiDataPin < 0 || twiDataPin > 48) return false;
    if (twiSensePin < 0 || twiSensePin > 48) return false;
    if (twiTxSpeed < 1 || twiTxSpeed > 9) return false;
    if (twiRxSpeed < 1 || twiRxSpeed > 9) return false;
    return true;
}

void UserSession::touch() { lastActivityTime = millis() / 1000; }

bool UserSession::isTimedOut(uint16_t timeoutSec) const {
    if (timeoutSec == 0) return false;
    uint32_t now = millis() / 1000;
    return (now - lastActivityTime) > timeoutSec;
}

DeviceManager& DeviceManager::instance() {
    static DeviceManager inst;
    return inst;
}

DeviceManager::DeviceManager() : _initialized(false), _settingsDirty(false) {}

bool DeviceManager::init() {
    _info.init();
    _settings.setDefaults();
    
    if (!LittleFS.begin(true)) return false;
    
    loadSettings();
    loadInfoFromStorage();
    
    _info.bootCount++;
    saveInfoToStorage();
    
    _initialized = true;
    return true;
}

bool DeviceManager::saveSettings() {
    JsonDocument doc;
    doc["dataPin"] = _settings.twiDataPin;
    doc["sensePin"] = _settings.twiSensePin;
    doc["txSpeed"] = _settings.twiTxSpeed;
    doc["rxSpeed"] = _settings.twiRxSpeed;
    doc["brightness"] = _settings.brightness;
    doc["requireLogin"] = _settings.requireLogin;
    doc["sessionTimeout"] = _settings.sessionTimeout;
    doc["opHash"] = _settings.operatorPinHash;
    doc["admHash"] = _settings.adminPinHash;
    
    File f = LittleFS.open("/settings.json", "w");
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
    
    _settingsDirty = false;
    return true;
}

bool DeviceManager::loadSettings() {
    File f = LittleFS.open("/settings.json", "r");
    if (!f) return false;
    
    JsonDocument doc;
    if (deserializeJson(doc, f)) { f.close(); return false; }
    f.close();
    
    _settings.twiDataPin = doc["dataPin"] | 12;
    _settings.twiSensePin = doc["sensePin"] | 13;
    _settings.twiTxSpeed = doc["txSpeed"] | 4;
    _settings.twiRxSpeed = doc["rxSpeed"] | 4;
    _settings.brightness = doc["brightness"] | 200;
    _settings.requireLogin = doc["requireLogin"] | false;
    _settings.sessionTimeout = doc["sessionTimeout"] | 0;
    
    const char* opHash = doc["opHash"];
    const char* admHash = doc["admHash"];
    if (opHash) strncpy(_settings.operatorPinHash, opHash, sizeof(_settings.operatorPinHash));
    if (admHash) strncpy(_settings.adminPinHash, admHash, sizeof(_settings.adminPinHash));
    
    return true;
}

void DeviceManager::resetSettingsToDefaults() {
    _settings.setDefaults();
    saveSettings();
}

bool DeviceManager::login(UserRole role, const std::string& pin) {
    if (_session.lockedOut) {
        uint32_t now = millis() / 1000;
        if (now < _session.lockoutEndTime) return false;
        _session.lockedOut = false;
        _session.failedAttempts = 0;
    }
    
    if (!verifyPIN(role, pin)) {
        _session.failedAttempts++;
        if (_session.failedAttempts >= _settings.maxPinAttempts) {
            _session.lockedOut = true;
            _session.lockoutEndTime = (millis() / 1000) + _settings.lockoutTime;
        }
        return false;
    }
    
    _session.role = role;
    _session.username = (role == ROLE_ADMIN) ? "Admin" : "Operator";
    _session.loginTime = millis() / 1000;
    _session.lastActivityTime = _session.loginTime;
    _session.failedAttempts = 0;
    
    return true;
}

void DeviceManager::logout() {
    _session.role = ROLE_NONE;
    _session.username.clear();
}

bool DeviceManager::changePIN(UserRole role, const std::string& oldPin, const std::string& newPin) {
    if (!verifyPIN(role, oldPin)) return false;
    return setPIN(role, newPin);
}

bool DeviceManager::setPIN(UserRole role, const std::string& pin) {
    std::string hash = hashPIN(pin);
    if (role == ROLE_OPERATOR) {
        strncpy(_settings.operatorPinHash, hash.c_str(), sizeof(_settings.operatorPinHash));
    } else if (role == ROLE_ADMIN) {
        strncpy(_settings.adminPinHash, hash.c_str(), sizeof(_settings.adminPinHash));
    } else {
        return false;
    }
    return saveSettings();
}

bool DeviceManager::verifyPIN(UserRole role, const std::string& pin) {
    std::string hash = hashPIN(pin);
    const char* stored = nullptr;
    const char* defaultPin = nullptr;
    
    if (role == ROLE_OPERATOR) {
        stored = _settings.operatorPinHash;
        defaultPin = DEFAULT_OPERATOR_PIN;
    } else if (role == ROLE_ADMIN) {
        stored = _settings.adminPinHash;
        defaultPin = DEFAULT_ADMIN_PIN;
    } else {
        return false;
    }
    
    // Check stored hash
    if (stored && stored[0] != '\0') {
        return hash == stored;
    }
    
    // Check default PIN
    return pin == defaultPin;
}

bool DeviceManager::checkAccess(bool requireAdmin, const char* action) {
    if (!_session.isLoggedIn()) return false;
    if (requireAdmin && !_session.isAdmin()) return false;
    return true;
}

void DeviceManager::service() {
    if (!_initialized) return;
    
    _info.uptimeSeconds = millis() / 1000;
    
    if (_session.isLoggedIn() && _settings.sessionTimeout > 0) {
        if (_session.isTimedOut(_settings.sessionTimeout)) {
            logout();
        }
    }
}

void DeviceManager::recordActivity() {
    _session.touch();
}

void DeviceManager::recordKeyload() {
    _info.lastKeyloadTime = millis() / 1000;
    _info.keyloadCount++;
    saveInfoToStorage();
}

bool DeviceManager::factoryReset() {
    _settings.setDefaults();
    _info.init();
    _session = UserSession();
    
    LittleFS.remove("/settings.json");
    LittleFS.remove("/device.json");
    
    return true;
}

std::string DeviceManager::hashPIN(const std::string& pin) {
    uint8_t hash[32];
    Crypto::sha256((const uint8_t*)pin.c_str(), pin.length(), hash);
    return Crypto::bytesToHex(hash, 32);
}

bool DeviceManager::saveInfoToStorage() {
    JsonDocument doc;
    doc["bootCount"] = _info.bootCount;
    doc["keyloadCount"] = _info.keyloadCount;
    doc["lastKeyload"] = _info.lastKeyloadTime;
    
    File f = LittleFS.open("/device.json", "w");
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
    return true;
}

bool DeviceManager::loadInfoFromStorage() {
    File f = LittleFS.open("/device.json", "r");
    if (!f) return false;
    
    JsonDocument doc;
    if (deserializeJson(doc, f)) { f.close(); return false; }
    f.close();
    
    _info.bootCount = doc["bootCount"] | 0;
    _info.keyloadCount = doc["keyloadCount"] | 0;
    _info.lastKeyloadTime = doc["lastKeyload"] | 0;
    
    return true;
}
