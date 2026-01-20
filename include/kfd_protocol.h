#pragma once

/**
 * @file kfd_protocol.h
 * @brief KFD Protocol Layer - KMM Framing and Session Management
 * 
 * Implements the protocol layer for P25 Manual Rekeying:
 * - Session initiation and termination
 * - KMM frame construction and parsing
 * - Key management operations (load, erase, view)
 * - RSI, MNP, and keyset operations
 * - MR (Mobile Radio) Emulator mode
 */

#include <stdint.h>
#include <vector>
#include <functional>
#include <string>
#include "p25_defs.h"
#include "twi_hal.h"

class KFDProtocol {
public:
    // Device type detected during session init
    enum DeviceType {
        DEVICE_NONE = 0,
        DEVICE_MR,      // Mobile Radio
        DEVICE_KVL,     // KVL (another KFD)
    };

    // Operation result
    struct Result {
        bool success;
        std::string message;
        P25::OperationStatus status;
        
        Result() : success(false), status(P25::STATUS_INTERNAL_ERROR) {}
        Result(bool ok, const std::string& msg = "") 
            : success(ok), message(msg), 
              status(ok ? P25::STATUS_COMMAND_PERFORMED : P25::STATUS_INTERNAL_ERROR) {}
    };

    // Progress callback for multi-key operations
    using ProgressCallback = std::function<void(int current, int total, const char* status)>;

    // MR Emulator callback - called when keys are received
    using MrKeyCallback = std::function<void(const P25::KeyItem& key)>;

    KFDProtocol();
    ~KFDProtocol();

    /**
     * @brief Initialize the protocol layer
     * @param hal TWI hardware abstraction layer
     * @return true if successful
     */
    bool init(TWI_HAL* hal);

    /**
     * @brief Check if connected to radio (SENSE line)
     */
    bool isRadioConnected();

    /**
     * @brief Perform adapter self-test
     * @return 0x00 if OK, error code otherwise
     */
    uint8_t selfTest();

    /**
     * @brief Detect MR connection by sending key signature
     * @param deviceType Output: detected device type
     * @return true if device detected
     */
    bool detectMr(DeviceType* deviceType = nullptr);

    // =========================================================================
    // KFD Operations (TIA-102.AACD-A Section 2.3)
    // =========================================================================

    /**
     * @brief Load a single key (2.3.1)
     */
    Result keyload(const P25::KeyItem& key);
    Result testInventory();  // Test with simple InventoryCommand
    Result testDESKey();     // Test with simple DES key (algorithm 0x81)
    
    /**
     * @brief Set send mode for debugging
     * @param fast true = use sendBytesFast(), false = use sendByte() loop
     */
    void setFastSendMode(bool fast) { _useFastSend = fast; }
    bool getFastSendMode() const { return _useFastSend; }
    
    /**
     * @brief Set delay after 0xD0 ready response (microseconds)
     */
    void setPostReadyDelay(uint32_t us) { _postReadyDelayUs = us; }
    uint32_t getPostReadyDelay() const { return _postReadyDelayUs; }

    /**
     * @brief Load multiple keys (2.3.1)
     */
    Result keyloadMultiple(const std::vector<P25::KeyItem>& keys,
                           ProgressCallback progress = nullptr);

    /**
     * @brief Erase a specific key (2.3.2)
     */
    Result eraseKey(uint16_t keysetId, uint16_t sln);

    /**
     * @brief Erase all keys (2.3.3)
     */
    Result eraseAllKeys();

    /**
     * @brief View key info (2.3.4)
     */
    Result viewKeyInfo(std::vector<P25::KeyStatus>& keys);

    /**
     * @brief View individual RSI (2.3.5)
     */
    Result viewRsi(std::vector<P25::RsiItem>& rsiItems);

    /**
     * @brief Load individual RSI (2.3.6)
     */
    Result loadRsi(uint32_t rsi, uint16_t mn = 0);

    /**
     * @brief View KMF RSI (2.3.7)
     */
    Result viewKmfRsi(std::vector<P25::KmfRsiItem>& kmfItems);

    /**
     * @brief Load KMF RSI (2.3.8)
     */
    Result loadKmfRsi(uint32_t rsi, uint16_t mn);

    /**
     * @brief View MNP (2.3.9)
     */
    Result viewMnp(std::vector<P25::MnpInfo>& mnpItems);

    /**
     * @brief Load MNP (2.3.10)
     */
    Result loadMnp(uint32_t rsi, uint16_t mn);

    /**
     * @brief View keyset info (2.3.11)
     */
    Result viewKeysetInfo(std::vector<P25::KeysetInfo>& keysets);

    /**
     * @brief Activate keyset (2.3.12)
     */
    Result activateKeyset(uint8_t keysetId);

    // =========================================================================
    // MR Emulator Mode
    // =========================================================================

    /**
     * @brief Start MR emulator mode
     * Listens for key loads from another KFD
     */
    Result startMrEmulator(MrKeyCallback callback);

    /**
     * @brief Stop MR emulator mode
     */
    void stopMrEmulator();

    /**
     * @brief Check if MR emulator is running
     */
    bool isMrEmulatorRunning() const { return _mrEmulatorRunning; }

    // =========================================================================
    // Session Management
    // =========================================================================

    /**
     * @brief Abort current operation
     */
    void abort();

    /**
     * @brief Check if operation is in progress
     */
    bool isOperationInProgress() const { return _operationInProgress; }

    /**
     * @brief Get last error message
     */
    const char* getLastError() const { return _lastError.c_str(); }

    /**
     * @brief Set debug mode
     */
    void setDebug(bool enable) { _debug = enable; }

private:
    TWI_HAL* _hal;
    bool _initialized;
    bool _debug;
    bool _operationInProgress;
    bool _mrEmulatorRunning;
    bool _abortRequested;
    bool _useFastSend;  // true = sendBytesFast(), false = sendByte() loop
    uint32_t _postReadyDelayUs;  // Delay after 0xD0/0xD1 before sending KMM
    std::string _lastError;
    MrKeyCallback _mrCallback;

    // Session management
    bool beginSession(DeviceType* deviceType = nullptr);
    void endSession();
    bool sendKmm(const std::vector<uint8_t>& kmm);
    bool receiveKmm(std::vector<uint8_t>& kmm, uint32_t timeoutMs = 5000);

    // KMM Frame construction
    std::vector<uint8_t> buildKmmFrame(const std::vector<uint8_t>& kmmBody);
    std::vector<uint8_t> buildKmmFrame(const std::vector<uint8_t>& kmmBody, uint8_t messageId, uint8_t responseKind);
    bool parseKmmFrame(const std::vector<uint8_t>& frame, std::vector<uint8_t>& kmmBody);

    // KMM Message builders
    std::vector<uint8_t> buildModifyKeyCommand(const P25::KeyItem& key);
    std::vector<uint8_t> buildModifyKeyCommand(const std::vector<P25::KeyItem>& keys);
    std::vector<uint8_t> buildZeroizeCommand();
    std::vector<uint8_t> buildInventoryCommand(P25::InventoryType type);
    std::vector<uint8_t> buildLoadConfigCommand(P25::InventoryType type, 
                                                 const std::vector<uint8_t>& data);
    std::vector<uint8_t> buildChangeRsiCommand(uint32_t rsi, uint16_t mn);
    std::vector<uint8_t> buildChangeoverCommand(uint8_t keysetId);

    // KMM Response parsers
    bool parseRekeyAck(const std::vector<uint8_t>& kmm, 
                       std::vector<P25::KeyStatus>& status);
    bool parseInventoryResponse(const std::vector<uint8_t>& kmm,
                                P25::InventoryType expectedType,
                                std::vector<uint8_t>& data);
    bool parseNegativeAck(const std::vector<uint8_t>& kmm, 
                          P25::OperationStatus& status);
    bool parseZeroizeResponse(const std::vector<uint8_t>& kmm);

    // Error handling
    void setError(const char* fmt, ...);
};
