/**
 * @file container.cpp
 * @brief Container Manager Implementation
 */

#include "container.h"
#include "crypto.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

P25::KeyItem KeySlot::toKeyItem(uint16_t keysetId) const {
    P25::KeyItem item;
    item.keysetId = keysetId;
    item.sln = sln;
    item.keyId = keyId;
    item.algorithmId = algorithmId;
    item.isKek = P25::getKeyType(sln) == P25::KEY_TYPE_KEK;
    item.erase = false;
    int len = Crypto::hexToBytes(keyHex, nullptr, 0);
    if (len > 0) { item.key.resize(len); Crypto::hexToBytes(keyHex, item.key.data(), len); }
    return item;
}

bool KeySlot::isValid() const {
    size_t expected = P25::getKeyLength(algorithmId);
    return expected == 0 || keyHex.length() == expected * 2;
}

size_t KeySlot::getKeyLength() const { return keyHex.length() / 2; }

std::vector<KeySlot> KeyGroup::getSelectedKeys() const {
    std::vector<KeySlot> sel;
    for (const auto& k : keys) if (k.selected) sel.push_back(k);
    return sel;
}

size_t KeyGroup::selectedCount() const {
    size_t c = 0; for (const auto& k : keys) if (k.selected) c++;
    return c;
}

std::vector<KeySlot> Container::getAllKeys() const {
    std::vector<KeySlot> all;
    for (const auto& g : groups) for (const auto& k : g.keys) all.push_back(k);
    return all;
}

std::vector<KeySlot> Container::getSelectedKeys() const {
    std::vector<KeySlot> sel;
    for (const auto& g : groups) for (const auto& k : g.keys) if (k.selected) sel.push_back(k);
    return sel;
}

size_t Container::totalKeyCount() const {
    size_t c = 0; for (const auto& g : groups) c += g.keys.size();
    return c;
}

size_t Container::selectedKeyCount() const {
    size_t c = 0; for (const auto& g : groups) c += g.selectedCount();
    return c;
}

void Container::touch() { char buf[32]; snprintf(buf, sizeof(buf), "%lu", millis()/1000); modifiedDate = buf; }

ContainerManager& ContainerManager::instance() { static ContainerManager inst; return inst; }

ContainerManager::ContainerManager() : _activeIndex(-1), _storageReady(false), _dirty(false), _lastChangeMs(0), _lastSaveMs(0) {}

bool ContainerManager::init() {
    if (!LittleFS.begin(true)) return false;
    _storageReady = true;
    return true;
}

const Container& ContainerManager::getContainer(size_t idx) const {
    static Container empty; return idx < _containers.size() ? _containers[idx] : empty;
}

Container& ContainerManager::getMutableContainer(size_t idx) {
    static Container empty; return idx < _containers.size() ? _containers[idx] : empty;
}

int ContainerManager::addContainer(const Container& c) {
    _containers.push_back(c); _dirty = true; _lastChangeMs = millis();
    return _containers.size() - 1;
}

bool ContainerManager::updateContainer(size_t idx, const Container& c) {
    if (idx >= _containers.size()) return false;
    _containers[idx] = c; _containers[idx].touch(); _dirty = true; _lastChangeMs = millis();
    return true;
}

bool ContainerManager::deleteContainer(size_t idx) {
    if (idx >= _containers.size()) return false;
    _containers.erase(_containers.begin() + idx);
    if (_activeIndex == (int)idx) _activeIndex = -1;
    else if (_activeIndex > (int)idx) _activeIndex--;
    _dirty = true; _lastChangeMs = millis();
    return true;
}

bool ContainerManager::setActiveIndex(int idx) {
    if (idx < -1 || idx >= (int)_containers.size()) return false;
    _activeIndex = idx;
    return true;
}

const Container* ContainerManager::getActiveContainer() const {
    return (_activeIndex >= 0 && _activeIndex < (int)_containers.size()) ? &_containers[_activeIndex] : nullptr;
}

Container* ContainerManager::getActiveContainerMutable() {
    return (_activeIndex >= 0 && _activeIndex < (int)_containers.size()) ? &_containers[_activeIndex] : nullptr;
}

bool ContainerManager::addGroup(size_t ci, const KeyGroup& g) {
    if (ci >= _containers.size()) return false;
    _containers[ci].groups.push_back(g); _containers[ci].touch(); _dirty = true; _lastChangeMs = millis();
    return true;
}

bool ContainerManager::addKey(size_t ci, size_t gi, const KeySlot& k) {
    if (ci >= _containers.size() || gi >= _containers[ci].groups.size()) return false;
    _containers[ci].groups[gi].keys.push_back(k); _containers[ci].touch(); _dirty = true; _lastChangeMs = millis();
    return true;
}

bool ContainerManager::load() {
    if (!_storageReady && !init()) return false;
    File f = LittleFS.open("/containers.json", "r");
    if (!f) return false;
    String json = f.readString(); f.close();
    return deserializeContainers(json.c_str());
}

bool ContainerManager::save() { _dirty = true; _lastChangeMs = millis(); return true; }

bool ContainerManager::saveNow() {
    if (!_storageReady) return false;
    std::string json = serializeContainers();
    File f = LittleFS.open("/containers.json", "w");
    if (!f) return false;
    f.print(json.c_str()); f.close();
    _dirty = false; _lastSaveMs = millis();
    return true;
}

void ContainerManager::service() {
    if (!_dirty) return;
    uint32_t now = millis();
    if (now - _lastChangeMs > 3000 && now - _lastSaveMs > 5000) saveNow();
}

std::string ContainerManager::serializeContainers() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (const auto& c : _containers) {
        JsonObject co = arr.add<JsonObject>();
        co["name"] = c.name; co["desc"] = c.description;
        JsonArray groups = co["groups"].to<JsonArray>();
        for (const auto& g : c.groups) {
            JsonObject go = groups.add<JsonObject>();
            go["name"] = g.name; go["keyset"] = g.keysetId;
            JsonArray keys = go["keys"].to<JsonArray>();
            for (const auto& k : g.keys) {
                JsonObject ko = keys.add<JsonObject>();
                ko["name"] = k.name; ko["algo"] = k.algorithmId;
                ko["keyId"] = k.keyId; ko["sln"] = k.sln;
                ko["key"] = k.keyHex; ko["sel"] = k.selected;
            }
        }
    }
    String out; serializeJson(doc, out);
    return out.c_str();
}

bool ContainerManager::deserializeContainers(const std::string& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) return false;
    _containers.clear();
    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject co : arr) {
        Container c; c.name = co["name"] | "Unnamed"; c.description = co["desc"] | "";
        JsonArray groups = co["groups"];
        for (JsonObject go : groups) {
            KeyGroup g; g.name = go["name"] | "Group"; g.keysetId = go["keyset"] | 1;
            JsonArray keys = go["keys"];
            for (JsonObject ko : keys) {
                KeySlot k; k.name = ko["name"] | "Key";
                k.algorithmId = ko["algo"] | P25::ALGO_AES_256;
                k.keyId = ko["keyId"] | 1; k.sln = ko["sln"] | 1;
                k.keyHex = ko["key"] | ""; k.selected = ko["sel"] | true;
                g.keys.push_back(k);
            }
            c.groups.push_back(g);
        }
        _containers.push_back(c);
    }
    return true;
}

void ContainerManager::loadDefaults() {
    _containers.clear();
    Container c; c.name = "Demo Container"; c.description = "Sample container";
    KeyGroup g; g.name = "Test Keys"; g.keysetId = 1;
    KeySlot k1; k1.name = "Test AES Key"; k1.algorithmId = P25::ALGO_AES_256;
    k1.keyId = 1; k1.sln = 1;
    k1.keyHex = "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF";
    k1.selected = true; g.keys.push_back(k1);
    c.groups.push_back(g); _containers.push_back(c); _activeIndex = 0;
}

bool ContainerManager::factoryReset() {
    _containers.clear(); _activeIndex = -1; _password.clear();
    if (_storageReady) LittleFS.remove("/containers.json");
    return true;
}

bool ContainerManager::setPassword(const std::string& p) { _password = p; return true; }
bool ContainerManager::verifyPassword(const std::string& p) const { return _password == p; }
bool ContainerManager::backupToSd(const char*) { return false; }
bool ContainerManager::restoreFromSd(const char*) { return false; }
bool ContainerManager::listSdBackups(std::vector<std::string>&) { return false; }
bool ContainerManager::exportToJson(const char*) { return false; }
bool ContainerManager::importFromJson(const char*) { return false; }
bool ContainerManager::updateGroup(size_t, size_t, const KeyGroup&) { return false; }
bool ContainerManager::deleteGroup(size_t, size_t) { return false; }
bool ContainerManager::updateKey(size_t, size_t, size_t, const KeySlot&) { return false; }
bool ContainerManager::deleteKey(size_t, size_t, size_t) { return false; }

namespace KeyGen {
    std::string generateKey(uint8_t algo) {
        size_t len = P25::getKeyLength(algo); if (len == 0) len = 32;
        uint8_t buf[32]; Crypto::generateRandom(buf, len);
        if (algo == P25::ALGO_DES_OFB || algo == P25::ALGO_2_KEY_3DES || algo == P25::ALGO_3_KEY_3DES)
            for (size_t i = 0; i < len; i += 8) Crypto::fixDesKeyParity(&buf[i]);
        return Crypto::bytesToHex(buf, len);
    }
    std::string generateAes256() { return generateKey(P25::ALGO_AES_256); }
    std::string generateDes() { return generateKey(P25::ALGO_DES_OFB); }
    std::string generate3Des2Key() { return generateKey(P25::ALGO_2_KEY_3DES); }
    std::string generate3Des3Key() { return generateKey(P25::ALGO_3_KEY_3DES); }
    bool validateKeyHex(const std::string& hex, uint8_t algo) {
        size_t exp = P25::getKeyLength(algo); return exp == 0 || hex.length() == exp * 2;
    }
    std::string fixDesParityHex(const std::string& hex) {
        size_t len = hex.length() / 2; if (len == 0) return hex;
        uint8_t buf[32]; Crypto::hexToBytes(hex, buf, sizeof(buf));
        for (size_t i = 0; i < len; i += 8) Crypto::fixDesKeyParity(&buf[i]);
        return Crypto::bytesToHex(buf, len);
    }
}
