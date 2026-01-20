#pragma once

/**
 * @file twi_hal.h
 * @brief Three-Wire Interface Hardware Abstraction Layer
 * 
 * Implements the physical layer for P25 TWI/3WI communication:
 * - Bit-level transmit/receive
 * - Key signature generation
 * - Timing control
 * - Self-test functionality
 * 
 * Pin assignments for WT32-SC01-Plus:
 * - DATA_TX/RX: GPIO 11 (bidirectional data)
 * - SENSE: GPIO 10 (radio connected detection)
 * - Additional optional pins for diagnostics
 */

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

class TWI_HAL {
public:
    // Configuration structure
    struct Config {
        int dataPin = 11;      // DATA pin (bidirectional, open-drain)
        int sensePin = 10;     // SENSE pin (directly active KFD detection - directly drive LOW to enable radio)
        int enPin = -1;        // Optional: separate enable pin if hardware supports it
        
        // Timing parameters (microseconds)
        uint32_t bitPeriodUs = 250;     // 4 kbaud default
        uint32_t keySignaturePeriodUs = 1000;  // 1ms period for key sig
        uint32_t keySignatureCount = 105;      // Number of pulses
        
        // Transfer speeds
        uint8_t txKilobaud = 4;  // TX speed in kbaud (standard)
        uint8_t rxKilobaud = 4;  // RX speed in kbaud (standard)
    };

    TWI_HAL();
    ~TWI_HAL();

    /**
     * @brief Initialize the TWI hardware
     * @param config Configuration parameters
     * @return true if successful
     */
    bool init(const Config& config);

    /**
     * @brief Get current configuration
     */
    Config getConfig() const { return _config; }

    /**
     * @brief Set transmit speed
     * @param kilobaud Speed in kilobaud (1-9 typical)
     */
    void setTxSpeed(uint8_t kilobaud);

    /**
     * @brief Set receive speed
     * @param kilobaud Speed in kilobaud (1-9 typical)
     */
    void setRxSpeed(uint8_t kilobaud);

    /**
     * @brief Perform self-test
     * @return 0x00 if OK, error code otherwise:
     *         0x01 = DATA stuck low
     *         0x02 = SENSE stuck low
     *         0x03 = DATA stuck high
     *         0x04 = SENSE stuck high
     */
    uint8_t selfTest();

    /**
     * @brief Check if radio is connected (SENSE line)
     * @return true if radio connected
     */
    bool isRadioConnected();
    bool isLineIdle() { return kfdRxIsIdle(); }
    bool isLineBusy() { return kfdRxIsBusy(); }

    /**
     * @brief Send key signature (initiates session)
     * This sends the characteristic pulse pattern that tells
     * the radio a KFD is present.
     */
    void sendKeySignature();

    /**
     * @brief Send a single byte
     * @param byte Byte to send
     */
    void sendByte(uint8_t byte);

    /**
     * @brief Send multiple bytes
     * @param data Data buffer
     * @param len Number of bytes
     */
    void sendBytes(const uint8_t* data, size_t len);

    /**
     * @brief Send multiple bytes with minimal overhead (for KMM frames)
     * @param data Data buffer
     * @param len Number of bytes
     */
    void sendBytesFast(const uint8_t* data, size_t len);

    /**
     * @brief Receive a byte with timeout
     * @param byte Output byte
     * @param timeoutMs Timeout in milliseconds (0 = no timeout)
     * @return true if byte received
     */
    bool receiveByte(uint8_t* byte, uint32_t timeoutMs = 5000);

    /**
     * @brief Receive bytes with timeout
     * @param buffer Output buffer
     * @param maxLen Maximum bytes to receive
     * @param received Output: actual bytes received
     * @param timeoutMs Timeout in milliseconds
     * @return true if any bytes received
     */
    bool receiveBytes(uint8_t* buffer, size_t maxLen, size_t* received, 
                      uint32_t timeoutMs = 5000);

    /**
     * @brief Set DATA line state (for diagnostics)
     * @param busy true = pull low (busy), false = release (idle/high)
     */
    void setDataLine(bool busy);

    /**
     * @brief Read DATA line state
     * @return true if high (idle), false if low (busy)
     */
    bool readDataLine();

    /**
     * @brief Read SENSE line state
     * @return true if connected (low), false if disconnected (high)
     */
    bool readSenseLine();

    /**
     * @brief Set SENSE line state (active low - pull low to wake radio)
     * @param active true = pull low (radio enabled), false = release (high-Z)
     */
    void setSenseLine(bool active);

    /**
     * @brief Enable the KFD interface (pull SENSE low to wake radio)
     */
    void enableInterface();

    /**
     * @brief Disable the KFD interface (release SENSE line)
     */
    void disableInterface();

    /**
     * @brief Send combined key signature and READY_REQ atomically
     * This is faster and more reliable than separate operations
     */
    void sendKeySignatureAndReadyReq();

    /**
     * @brief Enable debug output to Serial
     */
    void enableDebug(bool enable) { _debug = enable; }
    bool isDebugEnabled() const { return _debug; }

    /**
     * @brief Set stop bit mode for testing
     * @param useBusy true = use BUSY (LOW) stop bits (KFDtool style), 
     *                false = use IDLE (HIGH) stop bits (standard async)
     */
    void setStopBitMode(bool useBusy) { _useBusyStopBits = useBusy; }
    bool getStopBitMode() const { return _useBusyStopBits; }

    /**
     * @brief Reset to idle state
     */
    void reset();

private:
    Config _config;
    bool _initialized;
    bool _debug;
    bool _useBusyStopBits;  // true = BUSY (KFDtool), false = IDLE (standard)
    
    volatile bool _receiving;
    volatile uint8_t _rxByte;
    volatile bool _rxComplete;
    
    // Timing
    uint32_t _bitPeriodTx;
    uint32_t _bitPeriodRx;
    
    // Internal methods
    void sendBit(bool bit);
    bool receiveBit();
    uint8_t reverseBits(uint8_t byte);
    bool isEvenParity(uint8_t byte);
    void delayMicroseconds_accurate(uint32_t us);
    
    // Low-level line control (matching KFDtool naming conventions)
    void kfdTxBusy();      // Drive DATA LOW (busy)
    void kfdTxIdle();      // Drive DATA HIGH (idle)
    bool kfdRxIsBusy();    // Check if DATA is LOW
    bool kfdRxIsIdle();    // Check if DATA is HIGH
    void senTxConn();      // Drive SENSE LOW (connected)
    void senTxDisc();      // Drive SENSE HIGH (disconnected)
    bool senRxIsConn();    // Check if SENSE is LOW
    bool senRxIsDisc();    // Check if SENSE is HIGH
    
    // Interrupt handling
    static TWI_HAL* _instance;
    static void IRAM_ATTR dataFallingISR();
    void handleDataFalling();
};
