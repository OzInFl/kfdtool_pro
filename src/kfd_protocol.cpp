/**
 * @file kfd_protocol.cpp
 * @brief KFD Protocol Layer Implementation
 */

#include "kfd_protocol.h"
#include <Arduino.h>
#include <stdarg.h>

namespace P25 {
    // CRC16 table from TIA 102.AACD-A Annex A (same as KFDtool)
    static const uint16_t crc16Table[256] = {
        0x0000, 0x1189, 0x2312, 0x329B, 0x4624, 0x57AD, 0x6536, 0x74BF,
        0x8C48, 0x9DC1, 0xAF5A, 0xBED3, 0xCA6C, 0xDBE5, 0xE97E, 0xF8F7,
        0x1081, 0x0108, 0x3393, 0x221A, 0x56A5, 0x472C, 0x75B7, 0x643E,
        0x9CC9, 0x8D40, 0xBFDB, 0xAE52, 0xDAED, 0xCB64, 0xF9FF, 0xE876,
        0x2102, 0x308B, 0x0210, 0x1399, 0x6726, 0x76AF, 0x4434, 0x55BD,
        0xAD4A, 0xBCC3, 0x8E58, 0x9FD1, 0xEB6E, 0xFAE7, 0xC87C, 0xD9F5,
        0x3183, 0x200A, 0x1291, 0x0318, 0x77A7, 0x662E, 0x54B5, 0x453C,
        0xBDCB, 0xAC42, 0x9ED9, 0x8F50, 0xFBEF, 0xEA66, 0xD8FD, 0xC974,
        0x4204, 0x538D, 0x6116, 0x709F, 0x0420, 0x15A9, 0x2732, 0x36BB,
        0xCE4C, 0xDFC5, 0xED5E, 0xFCD7, 0x8868, 0x99E1, 0xAB7A, 0xBAF3,
        0x5285, 0x430C, 0x7197, 0x601E, 0x14A1, 0x0528, 0x37B3, 0x263A,
        0xDECD, 0xCF44, 0xFDDF, 0xEC56, 0x98E9, 0x8960, 0xBBFB, 0xAA72,
        0x6306, 0x728F, 0x4014, 0x519D, 0x2522, 0x34AB, 0x0630, 0x17B9,
        0xEF4E, 0xFEC7, 0xCC5C, 0xDDD5, 0xA96A, 0xB8E3, 0x8A78, 0x9BF1,
        0x7387, 0x620E, 0x5095, 0x411C, 0x35A3, 0x242A, 0x16B1, 0x0738,
        0xFFCF, 0xEE46, 0xDCDD, 0xCD54, 0xB9EB, 0xA862, 0x9AF9, 0x8B70,
        0x8408, 0x9581, 0xA71A, 0xB693, 0xC22C, 0xD3A5, 0xE13E, 0xF0B7,
        0x0840, 0x19C9, 0x2B52, 0x3ADB, 0x4E64, 0x5FED, 0x6D76, 0x7CFF,
        0x9489, 0x8500, 0xB79B, 0xA612, 0xD2AD, 0xC324, 0xF1BF, 0xE036,
        0x18C1, 0x0948, 0x3BD3, 0x2A5A, 0x5EE5, 0x4F6C, 0x7DF7, 0x6C7E,
        0xA50A, 0xB483, 0x8618, 0x9791, 0xE32E, 0xF2A7, 0xC03C, 0xD1B5,
        0x2942, 0x38CB, 0x0A50, 0x1BD9, 0x6F66, 0x7EEF, 0x4C74, 0x5DFD,
        0xB58B, 0xA402, 0x9699, 0x8710, 0xF3AF, 0xE226, 0xD0BD, 0xC134,
        0x39C3, 0x284A, 0x1AD1, 0x0B58, 0x7FE7, 0x6E6E, 0x5CF5, 0x4D7C,
        0xC60C, 0xD785, 0xE51E, 0xF497, 0x8028, 0x91A1, 0xA33A, 0xB2B3,
        0x4A44, 0x5BCD, 0x6956, 0x78DF, 0x0C60, 0x1DE9, 0x2F72, 0x3EFB,
        0xD68D, 0xC704, 0xF59F, 0xE416, 0x90A9, 0x8120, 0xB3BB, 0xA232,
        0x5AC5, 0x4B4C, 0x79D7, 0x685E, 0x1CE1, 0x0D68, 0x3FF3, 0x2E7A,
        0xE70E, 0xF687, 0xC41C, 0xD595, 0xA12A, 0xB0A3, 0x8238, 0x93B1,
        0x6B46, 0x7ACF, 0x4854, 0x59DD, 0x2D62, 0x3CEB, 0x0E70, 0x1FF9,
        0xF78F, 0xE606, 0xD49D, 0xC514, 0xB1AB, 0xA022, 0x92B9, 0x8330,
        0x7BC7, 0x6A4E, 0x58D5, 0x495C, 0x3DE3, 0x2C6A, 0x1EF1, 0x0F78
    };
    
    uint16_t calculateCrc16(const uint8_t* data, size_t len) {
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < len; i++) {
            crc = (uint16_t)(crc16Table[data[i] ^ (crc & 0xFF)] ^ (crc >> 8));
        }
        // Note: KFDtool reference does NOT do final XOR, even though TIA spec says to
        // The self-test "123456789" = 0x6F91 (not 0x906E) matches KFDtool behavior
        return crc;
    }
    
    bool validateKeysetId(int id) { return id >= 1 && id <= 255; }
    bool validateSln(int sln, bool isKek) {
        if (isKek) return sln >= 0xF000 && sln <= 0xFFFF;
        return sln >= 0 && sln <= 0xEFFF;
    }
    bool validateKeyId(int id) { return id >= 0 && id <= 0xFFFF; }
    bool validateAlgorithmId(int id) { return id >= 0 && id <= 0xFF; }
    bool validateKey(const std::vector<uint8_t>& key, uint8_t algo) {
        size_t expected = getKeyLength(algo);
        return expected == 0 || key.size() == expected;
    }
}

KFDProtocol::KFDProtocol() : _hal(nullptr), _initialized(false), _debug(true),
                             _operationInProgress(false), _mrEmulatorRunning(false),
                             _abortRequested(false), _useFastSend(true), 
                             _postReadyDelayUs(0) {}

KFDProtocol::~KFDProtocol() { stopMrEmulator(); }

bool KFDProtocol::init(TWI_HAL* hal) {
    if (!hal) return false;
    _hal = hal;
    
    // CRC self-test - verify our algorithm matches KFDtool reference
    // Test vector: CRC of "123456789" = 0x6F91 (KFDtool) or 0x906E (TIA with complement)
    // KFDtool does NOT do final complement, so we expect 0x6F91
    const uint8_t testData[] = {'1','2','3','4','5','6','7','8','9'};
    uint16_t testCrc = P25::calculateCrc16(testData, 9);
    
    // Also test with our actual frame structure
    const uint8_t testFrame[] = {0x00, 0xFF, 0xFF, 0xFF};  // control + destRSI
    uint16_t frameCrc = P25::calculateCrc16(testFrame, 4);
    
    if (_debug) {
        Serial.printf("[KFD] CRC self-test: '123456789' = 0x%04X (expected 0x6F91 for KFDtool compat)\n", testCrc);
        Serial.printf("[KFD] CRC of [00 FF FF FF] = 0x%04X\n", frameCrc);
    }
    
    _initialized = true;
    return true;
}

bool KFDProtocol::isRadioConnected() { return _hal && _hal->isRadioConnected(); }
uint8_t KFDProtocol::selfTest() { return _hal ? _hal->selfTest() : 0xFF; }

bool KFDProtocol::detectMr(DeviceType* deviceType) {
    if (!_initialized || !_hal) {
        setError("Not initialized");
        return false;
    }
    
    _hal->sendKeySignature();
    
    std::vector<uint8_t> cmd = { P25::TWI_READY_REQ };
    _hal->sendBytes(cmd.data(), cmd.size());
    
    uint8_t response;
    if (!_hal->receiveByte(&response, 5000)) {
        setError("No response from radio");
        return false;
    }
    
    if (response == P25::TWI_READY_MODE_MR) {
        if (deviceType) *deviceType = DEVICE_MR;
        endSession();
        return true;
    } else if (response == P25::TWI_READY_MODE_KVL) {
        if (deviceType) *deviceType = DEVICE_KVL;
        endSession();
        return true;
    }
    
    setError("Unexpected response: 0x%02X", response);
    return false;
}

bool KFDProtocol::beginSession(DeviceType* deviceType) {
    if (!_initialized || !_hal) return false;
    
    // Try multiple times - timing can be tricky
    for (int attempt = 1; attempt <= 3; attempt++) {
        if (_debug) {
            if (attempt == 1) Serial.println("[KFD] Beginning session...");
            else Serial.printf("[KFD] Retry attempt %d...\n", attempt);
        }
        
        // Send key signature + READY_REQ
        _hal->sendKeySignatureAndReadyReq();
        
        // Don't print before receive - radio responds fast!
        uint8_t response;
        if (_hal->receiveByte(&response, 2000)) {
            // Don't print here - minimize delay!
            
            if (response == P25::TWI_READY_MODE_MR) {
                if (_debug) Serial.println("[KFD] Got 0xD0 - MR mode ready");
                // Small delay for radio to transition - tunable via setPostReadyDelay()
                if (_postReadyDelayUs > 0) {
                    delayMicroseconds(_postReadyDelayUs);
                }
                if (deviceType) *deviceType = DEVICE_MR;
                return true;
            } else if (response == P25::TWI_READY_MODE_KVL) {
                if (_debug) Serial.println("[KFD] Got 0xD1 - KVL mode ready");
                if (_postReadyDelayUs > 0) {
                    delayMicroseconds(_postReadyDelayUs);
                }
                if (deviceType) *deviceType = DEVICE_KVL;
                return true;
            }
            
            if (_debug) Serial.printf("[KFD] Unexpected response: 0x%02X (expected 0xD0 or 0xD1)\n", response);
        } else {
            if (_debug) Serial.println("[KFD] No response from radio!");
        }
        
        // Longer delay before retry
        delay(500);
    }
    
    return false;
}

void KFDProtocol::endSession() {
    if (!_hal) return;
    _hal->sendByte(P25::TWI_TRANSFER_DONE);
    
    uint8_t response;
    if (_hal->receiveByte(&response, 1000)) {
        if (response == P25::TWI_TRANSFER_DONE) {
            _hal->sendByte(P25::TWI_DISCONNECT);
            _hal->receiveByte(&response, 1000);
        }
    }
    
    // Disable the interface (release SENSE line)
    _hal->disableInterface();
}

bool KFDProtocol::sendKmm(const std::vector<uint8_t>& kmm) {
    if (!_hal) return false;
    auto frame = buildKmmFrame(kmm);
    
    // Debug: print entire frame
    if (_debug) {
        Serial.printf("[KFD] KMM frame hex (%d bytes): ", frame.size());
        for (size_t i = 0; i < frame.size(); i++) {
            Serial.printf("%02X ", frame[i]);
        }
        Serial.println();
    }
    
    // Suppress debug during transmission for consistent timing
    bool debugWas = _hal->isDebugEnabled();
    _hal->enableDebug(false);
    
    // Send frame using fast mode for consistent timing
    _hal->sendBytesFast(frame.data(), frame.size());
    
    _hal->enableDebug(debugWas);
    
    if (_debug) Serial.printf("[TWI] Sent %d bytes (normal mode)\n", frame.size());
    
    // Check line state immediately after send
    if (_debug) {
        bool isIdle = _hal->isLineIdle();
        Serial.printf("[KFD] Line after TX: %s\n", isIdle ? "HIGH (idle)" : "LOW (radio responding?)");
        
        // Poll line state for 100ms to see if it changes
        uint32_t start = millis();
        while ((millis() - start) < 100) {
            if (_hal->isLineBusy()) {
                Serial.printf("[KFD] Line went LOW at +%dms!\n", (int)(millis() - start));
                break;
            }
            delayMicroseconds(100);
        }
    }
    
    return true;
}

bool KFDProtocol::receiveKmm(std::vector<uint8_t>& kmm, uint32_t timeoutMs) {
    if (!_hal) return false;
    
    if (_debug) Serial.printf("[KFD] Waiting for KMM response (timeout=%dms)...\n", timeoutMs);
    
    uint8_t opcode;
    if (!_hal->receiveByte(&opcode, timeoutMs)) {
        if (_debug) Serial.println("[KFD] No KMM opcode received");
        return false;
    }
    
    if (_debug) Serial.printf("[KFD] Got opcode: 0x%02X\n", opcode);
    
    // If opcode is 0xC3 (unknown/error), just dump all bytes we can receive
    if (opcode == 0xC3) {
        if (_debug) Serial.println("[KFD] Got 0xC3 (possible CRC error response) - dumping raw response...");
        std::vector<uint8_t> raw;
        raw.push_back(opcode);
        
        // Keep reading bytes until timeout (with short timeout between bytes)
        uint8_t b;
        while (_hal->receiveByte(&b, 500)) {  // 500ms timeout between bytes
            raw.push_back(b);
            if (raw.size() > 100) break;  // Safety limit
        }
        
        Serial.printf("[KFD] 0xC3 response (%d bytes): ", raw.size());
        for (size_t i = 0; i < raw.size(); i++) {
            Serial.printf("%02X ", raw[i]);
        }
        Serial.println();
        
        // Try to interpret the response
        if (raw.size() >= 8) {
            Serial.println("[KFD] Interpreting 0xC3 response:");
            Serial.printf("  Bytes 1-2: 0x%02X%02X (possibly length or error code)\n", raw[1], raw[2]);
            Serial.printf("  Bytes 3-4: 0x%02X%02X (possibly error details)\n", raw[3], raw[4]);
            Serial.printf("  Bytes 5-6: 0x%02X%02X (0x84=AES256, 0x83=unknown)\n", raw[5], raw[6]);
            Serial.printf("  Byte 7: 0x%02X\n", raw[7]);
        }
        
        // Return the raw response so caller can analyze
        kmm = raw;
        return true;  // We got a response, just not what we expected
    }
    
    // Standard KMM (0xC2) handling
    if (opcode != P25::TWI_KMM) {
        if (_debug) Serial.printf("[KFD] Unexpected opcode (expected 0xC2)\n");
        // Try to read more bytes to see what the radio is sending
        if (_debug) {
            Serial.print("[KFD] Debug - reading more bytes: ");
            for (int i = 0; i < 10; i++) {
                uint8_t b;
                if (_hal->receiveByte(&b, 100)) {
                    Serial.printf("%02X ", b);
                } else {
                    break;
                }
            }
            Serial.println();
        }
        return false;
    }
    
    uint8_t lenHi, lenLo;
    if (!_hal->receiveByte(&lenHi, timeoutMs)) {
        if (_debug) Serial.println("[KFD] Timeout reading length high byte");
        return false;
    }
    if (!_hal->receiveByte(&lenLo, timeoutMs)) {
        if (_debug) Serial.println("[KFD] Timeout reading length low byte");
        return false;
    }
    uint16_t len = ((uint16_t)lenHi << 8) | lenLo;
    
    if (_debug) Serial.printf("[KFD] Frame length: %d bytes\n", len);
    
    if (len < 6 || len > 512) {
        if (_debug) Serial.printf("[KFD] Invalid length, reading raw bytes...\n");
        Serial.print("[KFD] Raw bytes: ");
        Serial.printf("%02X %02X ", lenHi, lenLo);
        for (int i = 0; i < 20; i++) {
            uint8_t b;
            if (_hal->receiveByte(&b, 100)) {
                Serial.printf("%02X ", b);
            } else {
                break;
            }
        }
        Serial.println();
        return false;
    }
    
    std::vector<uint8_t> body(len);
    for (size_t i = 0; i < len; i++) {
        if (!_hal->receiveByte(&body[i], timeoutMs)) {
            if (_debug) Serial.printf("[KFD] Timeout at byte %d of %d\n", i, len);
            return false;
        }
    }
    
    if (_debug) {
        Serial.printf("[KFD] Raw frame (%d bytes): ", len);
        for (size_t i = 0; i < len && i < 20; i++) {
            Serial.printf("%02X ", body[i]);
        }
        if (len > 20) Serial.print("...");
        Serial.println();
    }
    
    // Skip control and dest RSI (4 bytes), extract KMM body (excluding 2-byte CRC)
    if (len > 6) {
        kmm.assign(body.begin() + 4, body.end() - 2);
    } else {
        kmm.assign(body.begin(), body.end());
    }
    
    if (_debug) {
        Serial.printf("[KFD] Received KMM body (%d bytes): ", kmm.size());
        for (size_t i = 0; i < kmm.size() && i < 16; i++) {
            Serial.printf("%02X ", kmm[i]);
        }
        if (kmm.size() > 16) Serial.print("...");
        Serial.println();
    }
    
    return true;
}

std::vector<uint8_t> KFDProtocol::buildModifyKeyCommand(const P25::KeyItem& key) {
    // Build the ModifyKeyCommand body per TIA 102.AACA-A
    // This is just the body - KmmFrame header comes later
    std::vector<uint8_t> body;
    
    // Decryption instruction format (1 byte) - bit 6 = MI flag
    body.push_back(0x00);
    
    // Extended decryption instruction format (1 byte)
    body.push_back(0x00);
    
    // KEK Algorithm ID (1 byte) - 0x80 = clear (no encryption)
    body.push_back(0x80);
    
    // KEK Key ID (2 bytes) - 0x0000 for clear
    body.push_back(0x00);
    body.push_back(0x00);
    
    // Keyset ID (1 byte)
    body.push_back(key.keysetId & 0xFF);
    
    // Algorithm ID (1 byte) - algorithm for the TEK
    body.push_back(key.algorithmId);
    
    // Key Length (1 byte)
    body.push_back(key.key.size() & 0xFF);
    
    // Number of keys (1 byte)
    body.push_back(0x01);
    
    // Key Item:
    // Key format (1 byte) - bit 7 = KEK flag, bit 5 = erase flag
    body.push_back(key.erase ? 0x20 : 0x00);
    
    // SLN (2 bytes)
    body.push_back((key.sln >> 8) & 0xFF);
    body.push_back(key.sln & 0xFF);
    
    // Key ID (2 bytes)
    body.push_back((key.keyId >> 8) & 0xFF);
    body.push_back(key.keyId & 0xFF);
    
    // Key data
    body.insert(body.end(), key.key.begin(), key.key.end());
    
    return body;
}

std::vector<uint8_t> KFDProtocol::buildKmmFrame(const std::vector<uint8_t>& kmmBody, uint8_t messageId, uint8_t responseKind) {
    // Build complete KMM frame for TWI transmission
    // Reference: ThreeWireProtocol.CreateKmmFrame() and KmmFrame.ToBytesWithPreamble()
    
    // First, build the inner KMM Frame (KmmFrame.ToBytes())
    std::vector<uint8_t> kmmFrame;
    
    // Message ID
    kmmFrame.push_back(messageId);
    
    // Message Length (7 + body length)
    int messageLength = 7 + kmmBody.size();
    kmmFrame.push_back((messageLength >> 8) & 0xFF);
    kmmFrame.push_back(messageLength & 0xFF);
    
    // Message Format - bits 7,6 = ResponseKind (0b11 = Immediate, 0b10 = Delayed)
    kmmFrame.push_back(responseKind);
    
    // Destination RSI (3 bytes)
    kmmFrame.push_back(0xFF);
    kmmFrame.push_back(0xFF);
    kmmFrame.push_back(0xFF);
    
    // Source RSI (3 bytes)
    kmmFrame.push_back(0xFF);
    kmmFrame.push_back(0xFF);
    kmmFrame.push_back(0xFF);
    
    // KMM Body
    kmmFrame.insert(kmmFrame.end(), kmmBody.begin(), kmmBody.end());
    
    // CRITICAL: For THREE-WIRE protocol, KFDtool does NOT use preamble!
    // Preamble is only for DLI (Data Link Independent) over UDP/IP
    // Reference: ManualRekeyApplication.cs - WithPreamble = false for ThreeWireProtocol
    bool usePreamble = false;
    
    std::vector<uint8_t> fullKmm;
    
    if (usePreamble) {
        // Build the preamble (ToBytesWithPreamble)
        std::vector<uint8_t> preamble;
        preamble.push_back(0x00);  // Version
        preamble.push_back(0x00);  // MFID
        preamble.push_back(0x80);  // AlgID (clear)
        preamble.push_back(0x00);  // Key ID high
        preamble.push_back(0x00);  // Key ID low
        // MI (Message Indicator) - 9 bytes of zeros
        for (int i = 0; i < 9; i++) {
            preamble.push_back(0x00);
        }
        
        // Combine preamble + KMM frame
        fullKmm.insert(fullKmm.end(), preamble.begin(), preamble.end());
        fullKmm.insert(fullKmm.end(), kmmFrame.begin(), kmmFrame.end());
        
        if (_debug) Serial.println("[KFD] WITH preamble (14 bytes) - DLI mode");
    } else {
        // No preamble - standard for THREE-WIRE protocol
        fullKmm = kmmFrame;
        if (_debug) Serial.println("[KFD] NO preamble (THREE-WIRE mode)");
    }
    
    // Now build the TWI frame (CreateKmmFrame)
    std::vector<uint8_t> twiBody;
    twiBody.push_back(0x00);  // Control
    twiBody.push_back(0xFF);  // Dest RSI high
    twiBody.push_back(0xFF);  // Dest RSI mid
    twiBody.push_back(0xFF);  // Dest RSI low
    twiBody.insert(twiBody.end(), fullKmm.begin(), fullKmm.end());
    
    // Calculate CRC over the body (control + destRSI + kmm)
    uint16_t crc = P25::calculateCrc16(twiBody.data(), twiBody.size());
    
    // Debug: show CRC calculation details
    if (_debug) {
        Serial.printf("[KFD] CRC over %d bytes: ", twiBody.size());
        for (size_t i = 0; i < twiBody.size() && i < 16; i++) {
            Serial.printf("%02X ", twiBody[i]);
        }
        if (twiBody.size() > 16) Serial.print("...");
        Serial.printf("\n[KFD] CRC = 0x%04X, sending LOW-HIGH order: %02X %02X\n", 
                      crc, crc & 0xFF, (crc >> 8) & 0xFF);
    }
    
    // Build final frame
    std::vector<uint8_t> frame;
    frame.push_back(P25::TWI_KMM);  // Opcode 0xC2
    
    // Length = body + CRC (2 bytes)
    int length = twiBody.size() + 2;
    frame.push_back((length >> 8) & 0xFF);
    frame.push_back(length & 0xFF);
    
    // Body
    frame.insert(frame.end(), twiBody.begin(), twiBody.end());
    
    // CRC byte order: KFDtool CalculateCrc() returns [low, high] and CreateKmmFrame adds them in that order
    // So we send LOW byte first, then HIGH byte
    frame.push_back(crc & 0xFF);          // CRC low byte first
    frame.push_back((crc >> 8) & 0xFF);   // CRC high byte second
    
    if (_debug) {
        Serial.printf("[KFD] KMM frame: msgID=0x%02X, len=%d, CRC=0x%04X (sent as %02X %02X)\n", 
                      messageId, length, crc, crc & 0xFF, (crc >> 8) & 0xFF);
    }
    
    return frame;
}

// Convenience wrapper for ModifyKeyCommand
std::vector<uint8_t> KFDProtocol::buildKmmFrame(const std::vector<uint8_t>& kmmBody) {
    return buildKmmFrame(kmmBody, P25::MSG_MODIFY_KEY_CMD, 0xC0);
}

std::vector<uint8_t> KFDProtocol::buildModifyKeyCommand(const std::vector<P25::KeyItem>& keys) {
    if (keys.empty()) return {};
    
    // Build the ModifyKeyCommand body per TIA 102.AACA-A
    std::vector<uint8_t> body;
    
    // Decryption instruction format (1 byte)
    body.push_back(0x00);
    
    // Extended decryption instruction format (1 byte)
    body.push_back(0x00);
    
    // KEK Algorithm ID (0x80 = clear)
    body.push_back(0x80);
    
    // KEK Key ID (2 bytes)
    body.push_back(0x00);
    body.push_back(0x00);
    
    // Keyset ID
    body.push_back(keys[0].keysetId & 0xFF);
    
    // Algorithm ID
    body.push_back(keys[0].algorithmId);
    
    // Key Length
    body.push_back(keys[0].key.size() & 0xFF);
    
    // Number of keys
    body.push_back(keys.size() & 0xFF);
    
    // Key items
    for (const auto& key : keys) {
        body.push_back(key.erase ? 0x20 : 0x00);  // Key format (bit 5 = erase)
        body.push_back((key.sln >> 8) & 0xFF);
        body.push_back(key.sln & 0xFF);
        body.push_back((key.keyId >> 8) & 0xFF);
        body.push_back(key.keyId & 0xFF);
        body.insert(body.end(), key.key.begin(), key.key.end());
    }
    
    return body;
}

std::vector<uint8_t> KFDProtocol::buildZeroizeCommand() {
    return { P25::MSG_ZEROIZE_CMD };
}

KFDProtocol::Result KFDProtocol::testInventory() {
    if (!_initialized) return Result(false, "Not initialized");
    
    if (_debug) Serial.println("[KFD] === Testing with InventoryCommand ===");
    
    if (!beginSession()) {
        return Result(false, "Failed to connect");
    }
    
    // CRITICAL: Send immediately after session start - no delay!
    
    // Build simple InventoryCommand (ListActiveKsetIds)
    std::vector<uint8_t> inventoryBody;
    inventoryBody.push_back(P25::INV_LIST_ACTIVE_KSET_IDS);  // = 0x02
    
    // Build frame with InventoryCommand message ID (0x00) and Immediate response (0xC0)
    auto frame = buildKmmFrame(inventoryBody, P25::MSG_INVENTORY_CMD, 0xC0);
    
    // Print COMPLETE frame hex before sending
    if (_debug) {
        Serial.printf("[KFD] Inventory frame (%d bytes): ", frame.size());
        for (size_t i = 0; i < frame.size(); i++) {
            Serial.printf("%02X ", frame[i]);
        }
        Serial.println();
    }
    
    // Send immediately - no debug prints before sending!
    _hal->enableDebug(false);
    if (_useFastSend) {
        _hal->sendBytesFast(frame.data(), frame.size());
    } else {
        for (size_t i = 0; i < frame.size(); i++) {
            _hal->sendByte(frame[i]);
        }
    }
    _hal->enableDebug(true);
    
    if (_debug) Serial.printf("[TWI] Sent inventory (%d bytes)\n", frame.size());
    
    if (_debug) {
        bool isIdle = _hal->isLineIdle();
        Serial.printf("[KFD] Line after TX: %s\n", isIdle ? "HIGH (idle)" : "LOW (radio responding?)");
    }
    
    // Wait for response
    std::vector<uint8_t> response;
    if (!receiveKmm(response, 5000)) {
        if (_debug) Serial.println("[KFD] No inventory response");
        endSession();
        return Result(false, "No response to inventory");
    }
    
    if (_debug) {
        Serial.printf("[KFD] Got inventory response: %d bytes, msgID=0x%02X\n", 
                      response.size(), response.empty() ? 0 : response[0]);
    }
    
    endSession();
    return Result(true, "Inventory succeeded");
}

KFDProtocol::Result KFDProtocol::testDESKey() {
    // Try loading an AES-256 key to SLN 202 (known working slot from user's radio)
    if (!_initialized) return Result(false, "Not initialized");
    
    if (_debug) Serial.println("[KFD] === Testing AES key at SLN 202 (known working slot) ===");
    
    if (!beginSession()) {
        return Result(false, "Failed to connect");
    }
    
    // Build ModifyKeyCommand for AES-256 at SLN 202
    std::vector<uint8_t> body;
    body.push_back(0x00);  // Decryption instruction format
    body.push_back(0x00);  // Extended decryption instruction
    body.push_back(0x80);  // KEK algo ID (clear)
    body.push_back(0x00);  // KEK key ID high
    body.push_back(0x00);  // KEK key ID low
    body.push_back(0x01);  // Keyset ID = 1
    body.push_back(0x84);  // Algorithm ID = AES-256 (0x84)
    body.push_back(0x20);  // Key length = 32 bytes
    body.push_back(0x01);  // Number of keys = 1
    
    // Key item - SLN 202 (0x00CA), Key ID 202 (0x00CA)
    body.push_back(0x00);  // Key format (clear, no erase)
    body.push_back(0x00);  // SLN high
    body.push_back(0xCA);  // SLN low = 202
    body.push_back(0x00);  // Key ID high
    body.push_back(0xCA);  // Key ID low = 202
    
    // 32-byte AES-256 test key
    for (int i = 0; i < 32; i++) {
        body.push_back(0x11 + i);  // Simple test pattern
    }
    
    auto frame = buildKmmFrame(body, P25::MSG_MODIFY_KEY_CMD, 0xC0);
    
    if (_debug) {
        Serial.printf("[KFD] AES@SLN202 frame hex (%d bytes): ", frame.size());
        for (size_t i = 0; i < frame.size(); i++) {
            Serial.printf("%02X ", frame[i]);
        }
        Serial.println();
    }
    
    _hal->enableDebug(false);
    if (_useFastSend) {
        _hal->sendBytesFast(frame.data(), frame.size());
    } else {
        for (size_t i = 0; i < frame.size(); i++) {
            _hal->sendByte(frame[i]);
        }
    }
    _hal->enableDebug(true);
    
    if (_debug) Serial.printf("[TWI] Sent AES@SLN202 frame (%d bytes)\n", frame.size());
    
    std::vector<uint8_t> response;
    if (!receiveKmm(response, 5000)) {
        if (_debug) Serial.println("[KFD] No response");
        endSession();
        return Result(false, "No response");
    }
    
    if (_debug) {
        Serial.printf("[KFD] Response: %d bytes\n", response.size());
    }
    
    endSession();
    return Result(true, "Test complete");
}

KFDProtocol::Result KFDProtocol::keyload(const P25::KeyItem& key) {
    if (!_initialized) return Result(false, "Not initialized");
    if (_operationInProgress) return Result(false, "Operation in progress");
    
    _operationInProgress = true;
    _abortRequested = false;
    
    if (_debug) {
        Serial.println("[KFD] === Starting keyload ===");
        Serial.printf("[KFD] Keyset: %d, SLN: %d, KeyID: %d, Algo: 0x%02X\n",
                      key.keysetId, key.sln, key.keyId, key.algorithmId);
    }
    
    if (!beginSession()) {
        _operationInProgress = false;
        return Result(false, "Failed to connect to radio");
    }
    
    // CRITICAL: Minimize delay between getting 0xD0 and sending KMM!
    // Radio may have timeout for expecting next command
    
    // Disable HAL debug for faster TX (less serial output)
    _hal->enableDebug(false);
    
    // Build and send KMM immediately - don't print anything in between
    auto kmm = buildModifyKeyCommand(key);
    
    // Print frame info BEFORE sending (debug happens after TX starts would be too late)
    auto frame = buildKmmFrame(kmm);
    if (_debug) {
        Serial.printf("[KFD] Sending KMM: %d bytes, CRC computed\n", frame.size());
    }
    
    // Now send the pre-built frame directly using fast mode
    _hal->sendBytesFast(frame.data(), frame.size());
    
    _hal->enableDebug(true);
    if (_debug) Serial.println("[KFD] KMM sent, waiting for response...");
    
    // Wait for radio to process and respond (10 seconds max)
    std::vector<uint8_t> response;
    if (!receiveKmm(response, 10000)) {
        if (_debug) Serial.println("[KFD] No KMM response received");
        
        endSession();
        _operationInProgress = false;
        return Result(false, "No response from radio after keyload");
    }
    
    endSession();
    _operationInProgress = false;
    
    if (response.empty()) return Result(false, "Empty response");
    
    if (_debug) Serial.printf("[KFD] Response message ID: 0x%02X\n", response[0]);
    
    if (response[0] == P25::MSG_REKEY_ACK) {
        if (_debug) Serial.println("[KFD] === KEY LOADED SUCCESSFULLY ===");
        return Result(true, "Key loaded successfully");
    } else if (response[0] == P25::MSG_NEGATIVE_ACK) {
        P25::OperationStatus status = (response.size() > 2) ? (P25::OperationStatus)response[2] : P25::STATUS_INVALID_MN;
        char buf[64];
        snprintf(buf, sizeof(buf), "NAK received: status 0x%02X", status);
        if (_debug) Serial.printf("[KFD] %s\n", buf);
        Result r(false, buf);
        r.status = status;
        return r;
    }
    
    if (_debug) Serial.printf("[KFD] Unknown response: 0x%02X\n", response[0]);
    return Result(false, "Unexpected response");
}

KFDProtocol::Result KFDProtocol::keyloadMultiple(const std::vector<P25::KeyItem>& keys,
                                                  ProgressCallback progress) {
    if (!_initialized) return Result(false, "Not initialized");
    if (_operationInProgress) return Result(false, "Operation in progress");
    if (keys.empty()) return Result(false, "No keys to load");
    
    _operationInProgress = true;
    _abortRequested = false;
    
    if (progress) progress(0, keys.size(), "Connecting to radio...");
    
    if (!beginSession()) {
        _operationInProgress = false;
        return Result(false, "Failed to connect to radio");
    }
    
    int loaded = 0;
    for (size_t i = 0; i < keys.size(); i++) {
        if (_abortRequested) {
            endSession();
            _operationInProgress = false;
            return Result(false, "Aborted by user");
        }
        
        char status[64];
        snprintf(status, sizeof(status), "Loading key %d of %d...", (int)(i+1), (int)keys.size());
        if (progress) progress(i, keys.size(), status);
        
        auto kmm = buildModifyKeyCommand(keys[i]);
        if (!sendKmm(kmm)) {
            endSession();
            _operationInProgress = false;
            return Result(false, "Failed to send key");
        }
        
        std::vector<uint8_t> response;
        if (!receiveKmm(response, 5000)) {
            endSession();
            _operationInProgress = false;
            return Result(false, "No response from radio");
        }
        
        if (response.empty() || response[0] != P25::MSG_REKEY_ACK) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Key %d failed", (int)(i+1));
            endSession();
            _operationInProgress = false;
            return Result(false, buf);
        }
        
        loaded++;
    }
    
    if (progress) progress(keys.size(), keys.size(), "Complete!");
    
    endSession();
    _operationInProgress = false;
    
    char buf[64];
    snprintf(buf, sizeof(buf), "%d keys loaded successfully", loaded);
    return Result(true, buf);
}

KFDProtocol::Result KFDProtocol::eraseKey(uint16_t keysetId, uint16_t sln) {
    P25::KeyItem key;
    key.keysetId = keysetId;
    key.sln = sln;
    key.erase = true;
    return keyload(key);
}

KFDProtocol::Result KFDProtocol::eraseAllKeys() {
    if (!_initialized) return Result(false, "Not initialized");
    if (_operationInProgress) return Result(false, "Operation in progress");
    
    _operationInProgress = true;
    
    if (!beginSession()) {
        _operationInProgress = false;
        return Result(false, "Failed to connect to radio");
    }
    
    auto kmm = buildZeroizeCommand();
    if (!sendKmm(kmm)) {
        endSession();
        _operationInProgress = false;
        return Result(false, "Failed to send zeroize command");
    }
    
    std::vector<uint8_t> response;
    if (!receiveKmm(response, 10000)) {
        endSession();
        _operationInProgress = false;
        return Result(false, "No response");
    }
    
    endSession();
    _operationInProgress = false;
    
    return Result(true, "All keys erased");
}

KFDProtocol::Result KFDProtocol::viewKeyInfo(std::vector<P25::KeyStatus>& keys) {
    // Simplified implementation
    return Result(false, "Not implemented");
}

KFDProtocol::Result KFDProtocol::viewRsi(std::vector<P25::RsiItem>& rsiItems) {
    return Result(false, "Not implemented");
}

KFDProtocol::Result KFDProtocol::loadRsi(uint32_t rsi, uint16_t mn) {
    return Result(false, "Not implemented");
}

KFDProtocol::Result KFDProtocol::viewKmfRsi(std::vector<P25::KmfRsiItem>& kmfItems) {
    return Result(false, "Not implemented");
}

KFDProtocol::Result KFDProtocol::loadKmfRsi(uint32_t rsi, uint16_t mn) {
    return Result(false, "Not implemented");
}

KFDProtocol::Result KFDProtocol::viewMnp(std::vector<P25::MnpInfo>& mnpItems) {
    return Result(false, "Not implemented");
}

KFDProtocol::Result KFDProtocol::loadMnp(uint32_t rsi, uint16_t mn) {
    return Result(false, "Not implemented");
}

KFDProtocol::Result KFDProtocol::viewKeysetInfo(std::vector<P25::KeysetInfo>& keysets) {
    return Result(false, "Not implemented");
}

KFDProtocol::Result KFDProtocol::activateKeyset(uint8_t keysetId) {
    return Result(false, "Not implemented");
}

KFDProtocol::Result KFDProtocol::startMrEmulator(MrKeyCallback callback) {
    return Result(false, "Not implemented");
}

void KFDProtocol::stopMrEmulator() { _mrEmulatorRunning = false; }
void KFDProtocol::abort() { _abortRequested = true; }

void KFDProtocol::setError(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    _lastError = buf;
}
