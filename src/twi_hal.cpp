/**
 * @file twi_hal.cpp
 * @brief Three-Wire Interface Hardware Abstraction Layer Implementation
 * 
 * Based on KFDtool AVR firmware reference implementation.
 * Key differences from generic implementations:
 * - Line polarity: BUSY = LOW, IDLE = HIGH on the actual wire
 * - Key signature: 100ms BUSY, then 5ms IDLE (not alternating pulses!)
 * - Byte frame: start(0) + 8 data bits (LSB first, reversed) + parity + 4 stop bits
 * - All timing is based on 4kbaud default (250µs per bit)
 */

#include "twi_hal.h"
#include <Arduino.h>
#include <esp_timer.h>
#include <driver/gpio.h>

TWI_HAL* TWI_HAL::_instance = nullptr;

TWI_HAL::TWI_HAL() : _initialized(false), _debug(true), _useBusyStopBits(true),
                     _receiving(false), _rxByte(0), _rxComplete(false), 
                     _bitPeriodTx(250), _bitPeriodRx(250) {}

TWI_HAL::~TWI_HAL() { reset(); }

bool TWI_HAL::init(const Config& config) {
    _config = config;
    _instance = this;
    
    // Configure DATA pin with internal pullup
    pinMode(_config.dataPin, INPUT_PULLUP);
    
    // Configure SENSE pin as output
    // Like KFDtool reference: connect SENSE at init and keep it connected
    pinMode(_config.sensePin, OUTPUT);
    digitalWrite(_config.sensePin, LOW);  // CONNECTED at startup
    
    setTxSpeed(_config.txKilobaud);
    setRxSpeed(_config.rxKilobaud);
    
    _initialized = true;
    
    delay(50);
    
    if (_debug) {
        Serial.printf("\n[TWI] ========== TWI HAL Init ==========\n");
        Serial.printf("[TWI] DATA pin: GPIO%d (internal pullup)\n", _config.dataPin);
        Serial.printf("[TWI] SENSE pin: GPIO%d (CONNECTED)\n", _config.sensePin);
        Serial.printf("[TWI] TX speed: %d kbaud (%d us/bit)\n", _config.txKilobaud, _bitPeriodTx);
        Serial.printf("[TWI] RX speed: %d kbaud (%d us/bit)\n", _config.rxKilobaud, _bitPeriodRx);
        Serial.printf("[TWI] DATA line: %s\n", kfdRxIsIdle() ? "IDLE (HIGH) - OK" : "BUSY (LOW) - CHECK WIRING!");
        Serial.printf("[TWI] =====================================\n\n");
    }
    
    return true;
}

void TWI_HAL::setTxSpeed(uint8_t kilobaud) {
    if (kilobaud == 0) kilobaud = 4;
    _config.txKilobaud = kilobaud;
    _bitPeriodTx = 1000 / kilobaud;  // µs per bit
}

void TWI_HAL::setRxSpeed(uint8_t kilobaud) {
    if (kilobaud == 0) kilobaud = 4;
    _config.rxKilobaud = kilobaud;
    _bitPeriodRx = 1000 / kilobaud;  // µs per bit
}

uint8_t TWI_HAL::selfTest() {
    if (!_initialized) return 0xFF;
    
    if (_debug) Serial.println("[TWI] Running self-test...");
    
    // Save current state
    bool senseWasActive = !digitalRead(_config.sensePin);
    
    // Test 1: DATA line should be HIGH when idle
    kfdTxIdle();
    senTxDisc();
    delay(10);
    
    if (kfdRxIsBusy()) {
        if (_debug) Serial.println("[TWI] FAIL: DATA stuck low");
        return 0x01;
    }
    
    // Test 2: SENSE should read disconnected
    if (senRxIsConn()) {
        if (_debug) Serial.println("[TWI] FAIL: SENSE stuck low");
        return 0x02;
    }
    
    // Test 3: DATA should go LOW when driven busy
    kfdTxBusy();
    delay(10);
    
    if (kfdRxIsIdle()) {
        if (_debug) Serial.println("[TWI] FAIL: DATA stuck high");
        kfdTxIdle();
        return 0x03;
    }
    kfdTxIdle();
    
    // Test 4: SENSE should read connected when driven
    senTxConn();
    delay(10);
    
    if (senRxIsDisc()) {
        if (_debug) Serial.println("[TWI] FAIL: SENSE stuck high");
        senTxDisc();
        return 0x04;
    }
    
    // Restore state
    if (senseWasActive) senTxConn();
    else senTxDisc();
    
    if (_debug) Serial.println("[TWI] Self-test PASSED");
    return 0x00;
}

bool TWI_HAL::isRadioConnected() {
    return senRxIsConn();
}

void TWI_HAL::setSenseLine(bool active) {
    if (active) senTxConn();
    else senTxDisc();
}

void TWI_HAL::enableInterface() {
    if (_debug) Serial.println("[TWI] Enabling interface (SENSE -> LOW)");
    senTxConn();
}

void TWI_HAL::disableInterface() {
    // Don't disconnect SENSE - keep connected like KFDtool reference
    // Radio stays in keyload mode
    if (_debug) Serial.println("[TWI] Session ended (SENSE stays connected)");
}

void TWI_HAL::sendKeySignature() {
    if (!_initialized) return;
    
    if (_debug) Serial.println("[TWI] === Sending key signature ===");
    
    // Enable interface (SENSE LOW) - radio should wake up
    senTxConn();
    
    if (_debug) Serial.printf("[TWI] DATA line before sig: %s\n", 
                              kfdRxIsIdle() ? "IDLE (HIGH)" : "BUSY (LOW)");
    
    // Key signature timing from KFDtool reference:
    // - BUSY for 100 periods (100ms) at 1ms/period  
    // - Then IDLE for 5 periods (5ms)
    // No delays - send immediately!
    
    portDISABLE_INTERRUPTS();
    
    // BUSY for 100ms (100 x 1ms periods)
    kfdTxBusy();
    for (int i = 0; i < 100; i++) {
        delayMicroseconds_accurate(1000);
    }
    
    // IDLE for 5ms (5 x 1ms periods) - release the line
    kfdTxIdle();
    for (int i = 0; i < 5; i++) {
        delayMicroseconds_accurate(1000);
    }
    
    portENABLE_INTERRUPTS();
    
    // Line is now released (INPUT mode)
    
    if (_debug) {
        Serial.println("[TWI] Key signature complete");
        Serial.printf("[TWI] DATA line after sig: %s\n", 
                      kfdRxIsIdle() ? "IDLE (HIGH)" : "BUSY (LOW)");
    }
    
    // NO DELAY - send READY_REQ immediately after key signature!
}

void TWI_HAL::sendKeySignatureAndReadyReq() {
    if (!_initialized) return;
    
    if (_debug) Serial.println("[TWI] === Sending key signature + READY_REQ ===");
    
    // SENSE is already connected from init (like KFDtool reference)
    // Just verify DATA line is IDLE
    if (_debug) Serial.printf("[TWI] DATA before keysig: %s\n", kfdRxIsIdle() ? "HIGH" : "LOW");
    
    portDISABLE_INTERRUPTS();
    
    // Key signature: BUSY for 100ms (pull line LOW)
    kfdTxBusy();
    for (int i = 0; i < 100; i++) {
        delayMicroseconds_accurate(1000);
    }
    
    // Key signature: IDLE for 5ms (release line)
    kfdTxIdle();
    for (int i = 0; i < 5; i++) {
        delayMicroseconds_accurate(1000);
    }
    
    portENABLE_INTERRUPTS();
    
    // Send READY_REQ (0xC0) immediately - suppress debug for speed
    bool debugWas = _debug;
    _debug = false;
    sendByte(0xC0);
    _debug = debugWas;
    
    // Release line - radio responds FAST so no delay!
    kfdTxIdle();
}

void TWI_HAL::sendByte(uint8_t byte) {
    if (!_initialized) return;
    
    uint8_t reversed = reverseBits(byte);
    uint16_t frame = reversed;
    
    bool needParity = !isEvenParity(byte);
    if (needParity) {
        frame |= 0x100;
    }
    
    frame = frame << 1;
    
    portDISABLE_INTERRUPTS();
    
    // Send 10 bits LSB first
    for (int i = 0; i < 10; i++) {
        if (frame & 0x01) {
            kfdTxIdle();
        } else {
            kfdTxBusy();
        }
        delayMicroseconds_accurate(_bitPeriodTx);
        frame >>= 1;
    }
    
    // Stop bits - configurable via setStopBitMode()
    if (_useBusyStopBits) {
        // KFDtool sends 4 stop bits as BUSY (LOW), then returns to IDLE
        kfdTxBusy();
        for (int i = 0; i < 4; i++) {
            delayMicroseconds_accurate(_bitPeriodTx);
        }
        kfdTxIdle();
    } else {
        // Standard async serial: stop bits are IDLE (HIGH)
        kfdTxIdle();
        for (int i = 0; i < 4; i++) {
            delayMicroseconds_accurate(_bitPeriodTx);
        }
    }
    
    portENABLE_INTERRUPTS();
    
    // CRITICAL: The radio needs significant IDLE time between bytes to detect
    // the next start bit. Add 2 bit periods of IDLE time for better reliability.
    delayMicroseconds(_bitPeriodTx * 2);
    
    if (_debug) Serial.printf("[TWI] TX: 0x%02X (rev=0x%02X, par=%d, frame=0x%03X)\n",
                              byte, reversed, needParity ? 1 : 0, (uint16_t)(reversed << 1) | (needParity ? 0x200 : 0));
}

void TWI_HAL::sendBytesFast(const uint8_t* data, size_t len) {
    // Send multiple bytes with minimal overhead - no debug output until done
    if (!_initialized || !data || len == 0) return;
    
    for (size_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        uint8_t reversed = reverseBits(byte);
        uint16_t frame = reversed;
        
        if (!isEvenParity(byte)) {
            frame |= 0x100;
        }
        frame = frame << 1;
        
        portDISABLE_INTERRUPTS();
        
        // Send 10 bits LSB first
        for (int j = 0; j < 10; j++) {
            if (frame & 0x01) {
                kfdTxIdle();
            } else {
                kfdTxBusy();
            }
            delayMicroseconds_accurate(_bitPeriodTx);
            frame >>= 1;
        }
        
        // 4 stop bits - use member variable
        if (_useBusyStopBits) {
            kfdTxBusy();
            for (int j = 0; j < 4; j++) {
                delayMicroseconds_accurate(_bitPeriodTx);
            }
            kfdTxIdle();
        } else {
            kfdTxIdle();
            for (int j = 0; j < 4; j++) {
                delayMicroseconds_accurate(_bitPeriodTx);
            }
        }
        
        portENABLE_INTERRUPTS();
        
        // Inter-byte gap - give radio time to sync for next start bit
        delayMicroseconds(_bitPeriodTx * 2);
    }
    
    // Make sure line is released at end
    kfdTxIdle();
    
    // Delay to let radio process frame before we look for response
    delayMicroseconds(1000);
    
    if (_debug) Serial.printf("[TWI] Sent %d bytes (fast mode)\n", len);
}

void TWI_HAL::sendBytes(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        sendByte(data[i]);
    }
}

bool TWI_HAL::receiveByte(uint8_t* byte, uint32_t timeoutMs) {
    if (!_initialized || !byte) return false;
    
    // Ensure line is released
    kfdTxIdle();
    
    uint32_t startMs = millis();
    
    // Check if line is already LOW (radio responding fast!)
    bool alreadyLow = kfdRxIsBusy();
    
    if (alreadyLow) {
        // Radio already started responding! Don't wait.
        if (_debug) Serial.println("[TWI] RX: Line already LOW - fast response!");
    } else {
        // Wait for start bit (falling edge)
        if (_debug) Serial.println("[TWI] RX: Waiting for start bit...");
        while (kfdRxIsIdle()) {
            if (timeoutMs > 0 && (millis() - startMs) > timeoutMs) {
                if (_debug) Serial.println("[TWI] RX timeout waiting for start bit");
                return false;
            }
            // Tight polling
        }
    }
    
    // Got the start bit (line is now LOW)
    portDISABLE_INTERRUPTS();
    
    // Wait T/2 to center on start bit (if we caught it fresh)
    // If alreadyLow, we might be late - but still try
    if (!alreadyLow) {
        delayMicroseconds_accurate(_bitPeriodRx / 2);
    }
    
    // Sample 10 bits using KFDtool algorithm
    uint16_t rxByte = 0;
    
    for (int bitsLeft = 10; bitsLeft > 0; bitsLeft--) {
        if (kfdRxIsIdle()) {
            rxByte |= 0x400;
        }
        rxByte = rxByte >> 1;
        if (bitsLeft > 1) {
            delayMicroseconds_accurate(_bitPeriodRx);
        }
    }
    
    portENABLE_INTERRUPTS();
    
    // Wait for stop bits
    startMs = millis();
    while (kfdRxIsBusy()) {
        if ((millis() - startMs) > 50) break;
        delayMicroseconds(10);
    }
    
    // Process received byte
    rxByte = rxByte >> 1;  // Remove start bit
    uint8_t rawByte = rxByte & 0xFF;
    *byte = reverseBits(rawByte);
    
    if (_debug) Serial.printf("[TWI] RX: 0x%02X (raw=0x%02X)\n", *byte, rawByte);
    
    return true;
}

bool TWI_HAL::receiveBytes(uint8_t* buffer, size_t maxLen, size_t* received, uint32_t timeoutMs) {
    if (!received) return false;
    *received = 0;
    
    while (*received < maxLen) {
        uint8_t b;
        if (!receiveByte(&b, timeoutMs)) break;
        buffer[(*received)++] = b;
    }
    
    return *received > 0;
}

// Low-level line control functions (matching KFDtool naming)
void TWI_HAL::kfdTxBusy() {
    // Drive LOW - active output
    gpio_set_direction((gpio_num_t)_config.dataPin, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)_config.dataPin, 0);  // LOW = BUSY
}

void TWI_HAL::kfdTxIdle() {
    // Release the line - high impedance, let external pullup pull HIGH
    gpio_set_direction((gpio_num_t)_config.dataPin, GPIO_MODE_INPUT);
    gpio_pullup_en((gpio_num_t)_config.dataPin);
}

bool TWI_HAL::kfdRxIsBusy() {
    return digitalRead(_config.dataPin) == LOW;
}

bool TWI_HAL::kfdRxIsIdle() {
    return digitalRead(_config.dataPin) == HIGH;
}

void TWI_HAL::senTxConn() {
    digitalWrite(_config.sensePin, LOW);  // Connected = drive LOW
}

void TWI_HAL::senTxDisc() {
    digitalWrite(_config.sensePin, HIGH); // Disconnected = drive HIGH  
}

bool TWI_HAL::senRxIsConn() {
    return digitalRead(_config.sensePin) == LOW;
}

bool TWI_HAL::senRxIsDisc() {
    return digitalRead(_config.sensePin) == HIGH;
}

// Legacy interface functions (for compatibility)
void TWI_HAL::setDataLine(bool busy) {
    if (busy) kfdTxBusy();
    else kfdTxIdle();
}

bool TWI_HAL::readDataLine() {
    return kfdRxIsIdle();  // Returns true if IDLE (HIGH)
}

bool TWI_HAL::readSenseLine() {
    return senRxIsConn();  // Returns true if connected (LOW)
}

void TWI_HAL::reset() {
    if (_initialized) {
        kfdTxIdle();
        senTxDisc();
    }
}

uint8_t TWI_HAL::reverseBits(uint8_t byte) {
    static const uint8_t table[] = {
        0x00,0x80,0x40,0xC0,0x20,0xA0,0x60,0xE0,0x10,0x90,0x50,0xD0,0x30,0xB0,0x70,0xF0,
        0x08,0x88,0x48,0xC8,0x28,0xA8,0x68,0xE8,0x18,0x98,0x58,0xD8,0x38,0xB8,0x78,0xF8,
        0x04,0x84,0x44,0xC4,0x24,0xA4,0x64,0xE4,0x14,0x94,0x54,0xD4,0x34,0xB4,0x74,0xF4,
        0x0C,0x8C,0x4C,0xCC,0x2C,0xAC,0x6C,0xEC,0x1C,0x9C,0x5C,0xDC,0x3C,0xBC,0x7C,0xFC,
        0x02,0x82,0x42,0xC2,0x22,0xA2,0x62,0xE2,0x12,0x92,0x52,0xD2,0x32,0xB2,0x72,0xF2,
        0x0A,0x8A,0x4A,0xCA,0x2A,0xAA,0x6A,0xEA,0x1A,0x9A,0x5A,0xDA,0x3A,0xBA,0x7A,0xFA,
        0x06,0x86,0x46,0xC6,0x26,0xA6,0x66,0xE6,0x16,0x96,0x56,0xD6,0x36,0xB6,0x76,0xF6,
        0x0E,0x8E,0x4E,0xCE,0x2E,0xAE,0x6E,0xEE,0x1E,0x9E,0x5E,0xDE,0x3E,0xBE,0x7E,0xFE,
        0x01,0x81,0x41,0xC1,0x21,0xA1,0x61,0xE1,0x11,0x91,0x51,0xD1,0x31,0xB1,0x71,0xF1,
        0x09,0x89,0x49,0xC9,0x29,0xA9,0x69,0xE9,0x19,0x99,0x59,0xD9,0x39,0xB9,0x79,0xF9,
        0x05,0x85,0x45,0xC5,0x25,0xA5,0x65,0xE5,0x15,0x95,0x55,0xD5,0x35,0xB5,0x75,0xF5,
        0x0D,0x8D,0x4D,0xCD,0x2D,0xAD,0x6D,0xED,0x1D,0x9D,0x5D,0xDD,0x3D,0xBD,0x7D,0xFD,
        0x03,0x83,0x43,0xC3,0x23,0xA3,0x63,0xE3,0x13,0x93,0x53,0xD3,0x33,0xB3,0x73,0xF3,
        0x0B,0x8B,0x4B,0xCB,0x2B,0xAB,0x6B,0xEB,0x1B,0x9B,0x5B,0xDB,0x3B,0xBB,0x7B,0xFB,
        0x07,0x87,0x47,0xC7,0x27,0xA7,0x67,0xE7,0x17,0x97,0x57,0xD7,0x37,0xB7,0x77,0xF7,
        0x0F,0x8F,0x4F,0xCF,0x2F,0xAF,0x6F,0xEF,0x1F,0x9F,0x5F,0xDF,0x3F,0xBF,0x7F,0xFF
    };
    return table[byte];
}

bool TWI_HAL::isEvenParity(uint8_t byte) {
    int ones = 0;
    for (int i = 0; i < 8; i++) {
        if (byte & (1 << i)) ones++;
    }
    return (ones % 2) == 0;
}

void TWI_HAL::delayMicroseconds_accurate(uint32_t us) {
    uint64_t start = esp_timer_get_time();
    while ((esp_timer_get_time() - start) < us) {
        // Busy wait for accurate timing
    }
}

void IRAM_ATTR TWI_HAL::dataFallingISR() {
    if (_instance) _instance->handleDataFalling();
}

void TWI_HAL::handleDataFalling() {
    _receiving = true;
}
