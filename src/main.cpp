/**
 * @file main.cpp
 * @brief KFDtool Professional - Main Entry Point
 * 
 * ESP32-S3 based P25 Key Fill Device
 * TIA-102.AACD-A Compliant
 * 
 * Hardware: WT32-SC01-Plus (ESP32-S3 + 3.5" 320x480 LCD)
 */

#include <Arduino.h>
#ifndef LGFX_USE_V1
#define LGFX_USE_V1
#endif
#include <LovyanGFX.hpp>
#include <lvgl.h>

#include "device_info.h"
#include "container.h"
#include "kfd_protocol.h"
#include "ui.h"

// =============================================================================
// LovyanGFX Configuration for WT32-SC01-Plus
// =============================================================================
class LGFX : public lgfx::LGFX_Device {
public:
    lgfx::Panel_ST7796   _panel_instance;
    lgfx::Bus_Parallel8  _bus_instance;
    lgfx::Light_PWM      _light_instance;
    lgfx::Touch_FT5x06   _touch_instance;

    LGFX(void) {
        // 8-bit parallel bus configuration
        {
            auto cfg = _bus_instance.config();
            cfg.port = 0;
            cfg.freq_write = 40000000;
            cfg.pin_wr = 47;
            cfg.pin_rd = -1;
            cfg.pin_rs = 0;
            cfg.pin_d0 = 9;
            cfg.pin_d1 = 46;
            cfg.pin_d2 = 3;
            cfg.pin_d3 = 8;
            cfg.pin_d4 = 18;
            cfg.pin_d5 = 17;
            cfg.pin_d6 = 16;
            cfg.pin_d7 = 15;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        // Panel configuration
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs = -1;
            cfg.pin_rst = 4;
            cfg.pin_busy = -1;
            cfg.memory_width = 320;
            cfg.memory_height = 480;
            cfg.panel_width = 320;
            cfg.panel_height = 480;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits = 1;
            cfg.readable = true;
            cfg.invert = true;
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = false;
            _panel_instance.config(cfg);
        }

        // Backlight configuration
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = 45;
            cfg.invert = false;
            cfg.freq = 44100;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        // Touch configuration (FT5x06 / FT6336U)
        {
            auto cfg = _touch_instance.config();
            cfg.x_min = 0;
            cfg.x_max = 319;
            cfg.y_min = 0;
            cfg.y_max = 479;
            cfg.pin_int = -1;
            cfg.bus_shared = false;
            cfg.offset_rotation = 0;
            cfg.i2c_port = 1;
            cfg.i2c_addr = 0x38;
            cfg.pin_sda = 6;
            cfg.pin_scl = 5;
            cfg.freq = 400000;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }

        setPanel(&_panel_instance);
    }
};

static LGFX lcd;

// =============================================================================
// LVGL Integration
// =============================================================================
static lv_disp_draw_buf_t draw_buf;
static lv_color_t lv_buf1[320 * 40];

static void lvgl_flush_cb(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
    int32_t w = area->x2 - area->x1 + 1;
    int32_t h = area->y2 - area->y1 + 1;

    if (w <= 0 || h <= 0) {
        lv_disp_flush_ready(disp);
        return;
    }

    lcd.startWrite();
    lcd.setAddrWindow(area->x1, area->y1, w, h);
    lcd.pushPixels((lgfx::rgb565_t*)&color_p->full, w * h);
    lcd.endWrite();
    lv_disp_flush_ready(disp);
}

static bool last_pressed = false;
static int16_t last_x = 0, last_y = 0;

static void lvgl_touch_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data) {
    (void)indev_drv;
    uint16_t x, y;
    bool pressed = lcd.getTouch(&x, &y);

    if (pressed) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = x;
        data->point.y = y;
        last_x = x;
        last_y = y;
        last_pressed = true;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        last_pressed = false;
    }
}

static void setup_lvgl() {
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, lv_buf1, nullptr, 320 * 40);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 320;
    disp_drv.ver_res = 480;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_read;
    lv_indev_drv_register(&indev_drv);
}

// =============================================================================
// Global Objects
// =============================================================================
static TWI_HAL g_twiHal;
static KFDProtocol g_kfd;

TWI_HAL& getTwiHal() { return g_twiHal; }
KFDProtocol& getKfdProtocol() { return g_kfd; }

// =============================================================================
// Splash Screen
// =============================================================================
static void show_splash_screen() {
    // Get device info
    DeviceManager& dm = DeviceManager::instance();
    const DeviceInfo& info = dm.getInfo();
    
    // Clear screen with dark background
    lcd.fillScreen(TFT_BLACK);
    
    // Draw title
    lcd.setTextColor(0x07FF, TFT_BLACK);  // Cyan
    lcd.setTextDatum(MC_DATUM);
    lcd.setTextSize(1);
    
    // KFDtool logo area (top)
    lcd.drawRect(20, 40, 280, 100, 0x07FF);
    lcd.drawRect(22, 42, 276, 96, 0x07FF);
    
    lcd.setTextSize(3);
    lcd.drawString("KFDtool", 160, 70);
    lcd.setTextSize(1);
    lcd.setTextColor(0xFFFF, TFT_BLACK);  // White
    lcd.drawString("PROFESSIONAL", 160, 105);
    
    // Subtitle
    lcd.setTextColor(0x8410, TFT_BLACK);  // Gray
    lcd.drawString("P25 Key Fill Device", 160, 160);
    lcd.drawString("TIA-102.AACD-A Compliant", 160, 180);
    
    // Device info box
    lcd.drawRect(20, 210, 280, 140, 0x4208);  // Dark gray border
    lcd.fillRect(21, 211, 278, 138, 0x10A2);  // Dark blue background
    
    lcd.setTextColor(0x07FF, 0x10A2);
    lcd.setTextDatum(TL_DATUM);
    lcd.drawString("DEVICE INFORMATION", 30, 220);
    
    lcd.setTextColor(0xFFFF, 0x10A2);
    char buf[64];
    
    lcd.drawString("Serial:", 30, 250);
    lcd.setTextColor(0x07E0, 0x10A2);  // Green
    lcd.drawString(info.serialNumber, 120, 250);
    
    lcd.setTextColor(0xFFFF, 0x10A2);
    lcd.drawString("Model:", 30, 275);
    lcd.setTextColor(0x07E0, 0x10A2);
    lcd.drawString(info.modelNumber, 120, 275);
    
    lcd.setTextColor(0xFFFF, 0x10A2);
    lcd.drawString("Firmware:", 30, 300);
    lcd.setTextColor(0x07E0, 0x10A2);
    lcd.drawString(info.firmwareVer, 120, 300);
    
    lcd.setTextColor(0xFFFF, 0x10A2);
    lcd.drawString("UID:", 30, 325);
    lcd.setTextColor(0x07E0, 0x10A2);
    snprintf(buf, sizeof(buf), "%08X", info.uniqueId);
    lcd.drawString(buf, 120, 325);
    
    // Copyright
    lcd.setTextDatum(MC_DATUM);
    lcd.setTextColor(0x4208, TFT_BLACK);
    lcd.drawString("ESP32-S3 / WT32-SC01-Plus", 160, 390);
    lcd.drawString("Open Source P25 KFD", 160, 410);
    
    // Loading indicator
    lcd.setTextColor(0xFFFF, TFT_BLACK);
    lcd.drawString("Initializing...", 160, 450);
    
    // Progress bar
    lcd.drawRect(60, 460, 200, 10, 0x4208);
    for (int i = 0; i < 180; i += 10) {
        lcd.fillRect(61 + i, 461, 8, 8, 0x07FF);
        delay(20);
    }
}

// =============================================================================
// Arduino Setup
// =============================================================================
void setup() {
    Serial.begin(115200);
    delay(100);
    
    Serial.println("\n========================================");
    Serial.println("KFDtool Professional - ESP32-S3");
    Serial.println("P25 Key Fill Device");
    Serial.println("TIA-102.AACD-A Compliant");
    Serial.println("========================================\n");
    
    // Initialize LCD
    Serial.println("[INIT] Initializing LCD...");
    lcd.init();
    lcd.setColorDepth(16);
    lcd.setRotation(0);  // Portrait 320x480
    lcd.setBrightness(200);
    Serial.println("[INIT] LCD initialized");
    
    // Initialize device manager (loads settings, generates serial)
    Serial.println("[INIT] Initializing device manager...");
    DeviceManager::instance().init();
    Serial.printf("[INIT] Serial: %s\n", DeviceManager::instance().getSerialNumber());
    
    // Show splash screen
    show_splash_screen();
    delay(500);
    
    // Initialize TWI hardware
    Serial.println("[INIT] Initializing TWI hardware...");
    TWI_HAL::Config twiConfig;
    twiConfig.dataPin = DeviceManager::instance().getSettings().twiDataPin;
    twiConfig.sensePin = DeviceManager::instance().getSettings().twiSensePin;
    twiConfig.txKilobaud = DeviceManager::instance().getSettings().twiTxSpeed;
    twiConfig.rxKilobaud = DeviceManager::instance().getSettings().twiRxSpeed;
    
    if (g_twiHal.init(twiConfig)) {
        Serial.println("[INIT] TWI hardware initialized");
        Serial.printf("[INIT]   DATA=%d, SENSE=%d\n", twiConfig.dataPin, twiConfig.sensePin);
        Serial.printf("[INIT]   TX=%d kbaud, RX=%d kbaud\n", twiConfig.txKilobaud, twiConfig.rxKilobaud);
    } else {
        Serial.println("[INIT] WARNING: TWI hardware init failed!");
    }
    
    // Initialize KFD protocol
    Serial.println("[INIT] Initializing KFD protocol...");
    if (g_kfd.init(&g_twiHal)) {
        Serial.println("[INIT] KFD protocol initialized");
    } else {
        Serial.println("[INIT] WARNING: KFD protocol init failed!");
    }
    
    // Load containers
    Serial.println("[INIT] Loading container data...");
    ContainerManager::instance().init();
    if (!ContainerManager::instance().load()) {
        Serial.println("[INIT] No saved containers, loading defaults");
        ContainerManager::instance().loadDefaults();
    }
    Serial.printf("[INIT] %d containers loaded\n", ContainerManager::instance().getContainerCount());
    
    // Initialize LVGL
    Serial.println("[INIT] Initializing LVGL...");
    setup_lvgl();
    Serial.println("[INIT] LVGL initialized");
    
    // Initialize UI
    Serial.println("[INIT] Initializing UI...");
    ui_init();
    Serial.println("[INIT] UI initialized");
    
    Serial.println("\n========================================");
    Serial.println("Initialization complete!");
    Serial.println("========================================");
    Serial.println("\nHARDWARE SETUP:");
    Serial.println("\n=== TWI Pins ===");
    Serial.printf("  DATA: GPIO %d (using internal pullup - no external resistor needed!)\n", twiConfig.dataPin);
    Serial.printf("  SENSE: GPIO %d\n", twiConfig.sensePin);
    Serial.println("");
    Serial.println("  Wiring: DATA -> Radio DATA, SENSE -> Radio SENSE, GND -> Radio GND");
    Serial.println("  No external resistors required when using internal pullup.");
    Serial.println("\nCABLE PINOUT (3.5mm TRS):");
    Serial.println("  Tip    → DATA");
    Serial.println("  Ring   → SENSE");
    Serial.println("  Shield → GND");
    Serial.println("\nReady for operation.\n");
}

// =============================================================================
// Arduino Loop
// =============================================================================
void loop() {
    // Service LVGL
    lv_timer_handler();
    
    // Service device manager (session timeout, etc)
    DeviceManager::instance().service();
    
    // Service container manager (autosave)
    ContainerManager::instance().service();
    
    // Serial command handler for debugging
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        
        if (cmd == "test" || cmd == "t") {
            Serial.println("\n[TEST] Running inventory test...");
            auto result = g_kfd.testInventory();
            Serial.printf("[TEST] Result: %s - %s\n", result.success ? "SUCCESS" : "FAILED", result.message.c_str());
        }
        else if (cmd == "aes" || cmd == "a") {
            Serial.println("\n[TEST] Testing AES key at SLN 202...");
            auto result = g_kfd.testDESKey();  // Actually tests AES despite the name
            Serial.printf("[TEST] Result: %s - %s\n", result.success ? "SUCCESS" : "FAILED", result.message.c_str());
        }
        else if (cmd == "stop0") {
            g_twiHal.setStopBitMode(false);
            Serial.println("[CONFIG] Stop bits set to IDLE (standard async)");
        }
        else if (cmd == "stop1") {
            g_twiHal.setStopBitMode(true);
            Serial.println("[CONFIG] Stop bits set to BUSY (KFDtool style)");
        }
        else if (cmd == "status" || cmd == "s") {
            Serial.println("\n[STATUS]");
            Serial.printf("  Stop bit mode: %s\n", g_twiHal.getStopBitMode() ? "BUSY (KFDtool)" : "IDLE (standard)");
            Serial.printf("  Send mode: %s\n", g_kfd.getFastSendMode() ? "FAST" : "SLOW (byte-by-byte)");
            Serial.printf("  Post-0xD0 delay: %u µs\n", g_kfd.getPostReadyDelay());
            Serial.printf("  DATA line: %s\n", g_twiHal.isLineIdle() ? "HIGH (idle)" : "LOW (busy)");
            Serial.printf("  SENSE line: %s\n", g_twiHal.readSenseLine() ? "LOW (connected)" : "HIGH (disconnected)");
        }
        else if (cmd == "selftest") {
            Serial.println("\n[SELFTEST] Running hardware self-test...");
            uint8_t result = g_twiHal.selfTest();
            if (result == 0) {
                Serial.println("[SELFTEST] PASSED - All hardware OK");
            } else {
                Serial.printf("[SELFTEST] FAILED - Error code: 0x%02X\n", result);
            }
        }
        else if (cmd == "sniff") {
            // Sniffer mode - capture traffic from another KFD
            Serial.println("\n[SNIFF] === TWI Sniffer Mode ===");
            Serial.println("[SNIFF] Connect KFDNano DATA to GPIO12, GND to GND");
            Serial.println("[SNIFF] Press any key to exit...");
            Serial.println("[SNIFF] Waiting for traffic...\n");
            
            uint32_t lastChangeTime = 0;
            bool lastState = true;  // Assume IDLE (HIGH)
            
            // Clear any pending serial
            while (Serial.available()) Serial.read();
            
            uint32_t startTime = micros();
            while (!Serial.available()) {
                bool currentState = g_twiHal.isLineIdle();
                uint32_t now = micros();
                
                if (currentState != lastState) {
                    uint32_t duration = now - lastChangeTime;
                    
                    // Log state changes
                    if (duration > 50) {  // Filter noise < 50us
                        if (!lastState) {  // Was LOW, now HIGH
                            Serial.printf("[%8u] LOW  %5u µs\n", lastChangeTime - startTime, duration);
                        } else {  // Was HIGH, now LOW
                            Serial.printf("[%8u] HIGH %5u µs\n", lastChangeTime - startTime, duration);
                        }
                    }
                    
                    lastChangeTime = now;
                    lastState = currentState;
                }
                
                delayMicroseconds(5);  // Sample at ~200kHz for better resolution
            }
            Serial.println("\n[SNIFF] Exited sniffer mode");
        }
        else if (cmd == "measure") {
            // Measure actual bit timing from KFDNano
            Serial.println("\n[MEASURE] === Bit Timing Measurement ===");
            Serial.println("[MEASURE] Connect KFDNano DATA to GPIO12");
            Serial.println("[MEASURE] Run a command on KFDNano, will measure bit periods");
            Serial.println("[MEASURE] Press any key to exit...\n");
            
            while (Serial.available()) Serial.read();
            
            // Wait for key signature first
            Serial.println("[MEASURE] Waiting for key signature...");
            while (!Serial.available()) {
                if (g_twiHal.isLineBusy()) {
                    uint32_t lowStart = micros();
                    while (g_twiHal.isLineBusy() && !Serial.available());
                    uint32_t lowDuration = micros() - lowStart;
                    if (lowDuration > 50000) {  // Key sig is ~100ms
                        Serial.printf("[MEASURE] Key signature: %u µs\n", lowDuration);
                        break;
                    }
                }
            }
            
            if (Serial.available()) { 
                Serial.println("[MEASURE] Exited"); 
            } else {
                // Wait for idle period
                uint32_t idleStart = micros();
                while (g_twiHal.isLineIdle() && !Serial.available() && (micros() - idleStart) < 20000);
                Serial.printf("[MEASURE] Idle period: %u µs\n", micros() - idleStart);
                
                // Now measure individual bit transitions during READY_REQ byte
                Serial.println("[MEASURE] Capturing bit transitions...");
                
                uint32_t transitions[100];
                int transCount = 0;
                bool lastState = g_twiHal.isLineIdle();
                uint32_t lastTime = micros();
                uint32_t measureStart = micros();
                
                // Capture transitions for ~50ms (enough for several bytes)
                while (transCount < 100 && (micros() - measureStart) < 50000 && !Serial.available()) {
                    bool state = g_twiHal.isLineIdle();
                    if (state != lastState) {
                        uint32_t now = micros();
                        transitions[transCount++] = now - lastTime;
                        lastTime = now;
                        lastState = state;
                    }
                }
                
                Serial.printf("\n[MEASURE] Captured %d transitions:\n", transCount);
                uint32_t totalPeriod = 0;
                int periodCount = 0;
                for (int i = 0; i < transCount && i < 50; i++) {
                    Serial.printf("  %3d: %4u µs\n", i, transitions[i]);
                    // Look for bit-period sized transitions (150-350µs range)
                    if (transitions[i] >= 150 && transitions[i] <= 350) {
                        totalPeriod += transitions[i];
                        periodCount++;
                    }
                }
                
                if (periodCount > 0) {
                    uint32_t avgPeriod = totalPeriod / periodCount;
                    uint32_t baudRate = 1000000 / avgPeriod;
                    Serial.printf("\n[MEASURE] Estimated bit period: %u µs\n", avgPeriod);
                    Serial.printf("[MEASURE] Estimated baud rate: %u baud\n", baudRate);
                    Serial.printf("[MEASURE] Our setting: 250 µs = 4000 baud\n");
                    if (avgPeriod < 240) {
                        Serial.println("[MEASURE] KFDNano is FASTER - try baud5 or baud6");
                    } else if (avgPeriod > 260) {
                        Serial.println("[MEASURE] KFDNano is SLOWER - try baud3");
                    } else {
                        Serial.println("[MEASURE] Timing looks close to 4 kbaud");
                    }
                }
                
                Serial.println("[MEASURE] Done");
            }
        }
        else if (cmd == "sniffbytes") {
            // Byte-level sniffer - try to decode actual bytes
            Serial.println("\n[SNIFF] === Byte Sniffer Mode ===");
            Serial.println("[SNIFF] Connect KFDNano DATA to GPIO12");
            Serial.println("[SNIFF] Press any key to exit...\n");
            
            // Clear serial
            while (Serial.available()) Serial.read();
            
            while (!Serial.available()) {
                uint8_t b;
                if (g_twiHal.receiveByte(&b, 100)) {
                    Serial.printf("[SNIFF] Byte: 0x%02X\n", b);
                }
            }
            Serial.println("[SNIFF] Exited");
        }
        else if (cmd == "emulate" || cmd == "emu") {
            // Radio emulator mode - pretend to be a radio, capture what KFD sends
            Serial.println("\n[EMU] === Radio Emulator Mode ===");
            Serial.println("[EMU] Connect KFDNano to ESP32:");
            Serial.println("[EMU]   KFDNano DATA  -> GPIO12");
            Serial.println("[EMU]   KFDNano SENSE -> GPIO13");
            Serial.println("[EMU]   KFDNano GND   -> GND");
            Serial.println("[EMU] Then run inventory/keyload on KFDNano software");
            Serial.println("[EMU] Press any key to exit...\n");
            
            // Clear serial
            while (Serial.available()) Serial.read();
            
            // Release lines - let KFDNano drive them
            g_twiHal.reset();
            
            while (!Serial.available()) {
                // Wait for key signature (line goes LOW for extended period)
                Serial.println("[EMU] Waiting for key signature...");
                
                uint32_t lowStart = 0;
                bool gotKeySig = false;
                
                // Wait for line to go LOW
                uint32_t waitStart = millis();
                while (!Serial.available() && (millis() - waitStart) < 30000) {
                    if (g_twiHal.isLineBusy()) {
                        lowStart = millis();
                        // Wait for it to stay LOW for >50ms (key signature)
                        while (g_twiHal.isLineBusy() && !Serial.available()) {
                            if ((millis() - lowStart) > 50) {
                                gotKeySig = true;
                                break;
                            }
                        }
                        if (gotKeySig) break;
                    }
                    delay(1);
                }
                
                if (Serial.available()) break;
                if (!gotKeySig) continue;
                
                uint32_t keySigDuration = millis() - lowStart;
                
                // Wait for line to go HIGH (end of key signature)
                while (g_twiHal.isLineBusy() && !Serial.available()) {
                    delay(1);
                }
                uint32_t totalLow = millis() - lowStart;
                Serial.printf("[EMU] Key signature: LOW for %u ms\n", totalLow);
                
                // Wait for IDLE period
                uint32_t idleStart = millis();
                while (g_twiHal.isLineIdle() && !Serial.available() && (millis() - idleStart) < 100) {
                    delayMicroseconds(100);
                }
                uint32_t idleDuration = millis() - idleStart;
                Serial.printf("[EMU] Idle period: %u ms\n", idleDuration);
                
                if (Serial.available()) break;
                
                // Now receive READY_REQ (0xC0)
                uint8_t readyReq;
                if (g_twiHal.receiveByte(&readyReq, 1000)) {
                    Serial.printf("[EMU] Got READY_REQ: 0x%02X\n", readyReq);
                    
                    // Send 0xD0 response (MR mode ready)
                    delay(1);  // Small delay before response
                    g_twiHal.sendByte(0xD0);
                    Serial.println("[EMU] Sent 0xD0 (MR ready)");
                    
                    // Now capture the KMM frame
                    Serial.println("[EMU] Waiting for KMM frame...");
                    
                    uint8_t kmmBuffer[256];
                    int kmmLen = 0;
                    uint32_t lastByteTime = millis();
                    
                    while (kmmLen < 256 && (millis() - lastByteTime) < 2000) {
                        uint8_t b;
                        if (g_twiHal.receiveByte(&b, 500)) {
                            kmmBuffer[kmmLen++] = b;
                            lastByteTime = millis();
                            Serial.printf("[EMU] Byte %d: 0x%02X\n", kmmLen, b);
                        }
                    }
                    
                    if (kmmLen > 0) {
                        Serial.printf("\n[EMU] === Captured KMM Frame (%d bytes) ===\n", kmmLen);
                        Serial.print("[EMU] HEX: ");
                        for (int i = 0; i < kmmLen; i++) {
                            Serial.printf("%02X ", kmmBuffer[i]);
                        }
                        Serial.println();
                        
                        // Parse the frame
                        if (kmmLen >= 3 && kmmBuffer[0] == 0xC2) {
                            int len = (kmmBuffer[1] << 8) | kmmBuffer[2];
                            Serial.printf("[EMU] Opcode: 0x%02X (KMM)\n", kmmBuffer[0]);
                            Serial.printf("[EMU] Length: %d bytes\n", len);
                            if (kmmLen >= 7) {
                                Serial.printf("[EMU] Control: 0x%02X\n", kmmBuffer[3]);
                                Serial.printf("[EMU] Dest RSI: %02X %02X %02X\n", 
                                              kmmBuffer[4], kmmBuffer[5], kmmBuffer[6]);
                            }
                            if (kmmLen >= 2) {
                                Serial.printf("[EMU] CRC bytes: %02X %02X\n", 
                                              kmmBuffer[kmmLen-2], kmmBuffer[kmmLen-1]);
                            }
                        }
                        
                        // Send a dummy response (NegativeAck to avoid confusing the KFD)
                        Serial.println("[EMU] Sending NAK response...");
                        delay(5);
                        // Simple NAK: C2 00 06 00 FF FF FF 2C 00 [CRC]
                        uint8_t nak[] = {0xC2, 0x00, 0x08, 0x00, 0xFF, 0xFF, 0xFF, 0x2C, 0x00, 0x00};
                        // Add simple CRC (just for completeness)
                        for (int i = 0; i < 10; i++) {
                            g_twiHal.sendByte(nak[i]);
                        }
                        Serial.println("[EMU] Done - waiting for disconnect...");
                    } else {
                        Serial.println("[EMU] No KMM bytes received");
                    }
                } else {
                    Serial.println("[EMU] No READY_REQ received");
                }
                
                // Wait a bit before next cycle
                delay(1000);
            }
            Serial.println("[EMU] Exited emulator mode");
        }
        else if (cmd == "help" || cmd == "h" || cmd == "?") {
            Serial.println("\n=== KFDtool Debug Commands ===");
            Serial.println("  test, t     - Run inventory test");
            Serial.println("  aes, a      - Test AES key at SLN 202");
            Serial.println("  stop0       - Use IDLE stop bits (standard)");
            Serial.println("  stop1       - Use BUSY stop bits (KFDtool)");
            Serial.println("  baud2-9     - Set baud rate (2/3/4/5/6/9 kbaud)");
            Serial.println("  fast        - Use fast send mode");
            Serial.println("  slow        - Use byte-by-byte send mode");
            Serial.println("  delay0/1/5  - Set delay after 0xD0 (0/1/5 ms)");
            Serial.println("  emulate     - Act as radio, capture KFDNano traffic");
            Serial.println("  measure     - Measure KFDNano bit timing");
            Serial.println("  sniff       - Capture raw DATA line transitions");
            Serial.println("  status, s   - Show current status");
            Serial.println("  help, h     - Show this help");
        }
        else if (cmd == "baud2") {
            g_twiHal.setTxSpeed(2);
            g_twiHal.setRxSpeed(2);
            Serial.println("[CONFIG] Baud rate set to 2 kbaud (500µs/bit)");
        }
        else if (cmd == "baud3") {
            g_twiHal.setTxSpeed(3);
            g_twiHal.setRxSpeed(3);
            Serial.println("[CONFIG] Baud rate set to 3 kbaud (333µs/bit)");
        }
        else if (cmd == "baud4") {
            g_twiHal.setTxSpeed(4);
            g_twiHal.setRxSpeed(4);
            Serial.println("[CONFIG] Baud rate set to 4 kbaud (250µs/bit)");
        }
        else if (cmd == "baud5") {
            g_twiHal.setTxSpeed(5);
            g_twiHal.setRxSpeed(5);
            Serial.println("[CONFIG] Baud rate set to 5 kbaud (200µs/bit)");
        }
        else if (cmd == "baud6") {
            g_twiHal.setTxSpeed(6);
            g_twiHal.setRxSpeed(6);
            Serial.println("[CONFIG] Baud rate set to 6 kbaud (167µs/bit)");
        }
        else if (cmd == "baud9") {
            g_twiHal.setTxSpeed(9);
            g_twiHal.setRxSpeed(9);
            Serial.println("[CONFIG] Baud rate set to 9.6 kbaud (104µs/bit)");
        }
        else if (cmd == "fast") {
            g_kfd.setFastSendMode(true);
            Serial.println("[CONFIG] Send mode: FAST (sendBytesFast)");
        }
        else if (cmd == "slow") {
            g_kfd.setFastSendMode(false);
            Serial.println("[CONFIG] Send mode: SLOW (byte-by-byte)");
        }
        else if (cmd.startsWith("delay ")) {
            String delayStr = cmd.substring(6);
            uint32_t delayUs = delayStr.toInt();
            g_kfd.setPostReadyDelay(delayUs);
            Serial.printf("[CONFIG] Post-0xD0 delay set to %u µs\n", delayUs);
        }
        else if (cmd == "delay0") {
            g_kfd.setPostReadyDelay(0);
            Serial.println("[CONFIG] Post-0xD0 delay disabled");
        }
        else if (cmd == "delay1") {
            g_kfd.setPostReadyDelay(1000);  // 1ms
            Serial.println("[CONFIG] Post-0xD0 delay set to 1000 µs (1ms)");
        }
        else if (cmd == "delay5") {
            g_kfd.setPostReadyDelay(5000);  // 5ms
            Serial.println("[CONFIG] Post-0xD0 delay set to 5000 µs (5ms)");
        }
        else if (cmd == "raw") {
            // Send a raw 0xC2 KMM opcode and see response
            Serial.println("\n[RAW] Sending single 0xC2 opcode...");
            g_twiHal.sendByte(0xC2);
            uint8_t resp;
            if (g_twiHal.receiveByte(&resp, 2000)) {
                Serial.printf("[RAW] Response: 0x%02X\n", resp);
            } else {
                Serial.println("[RAW] No response (timeout)");
            }
        }
        else if (cmd.startsWith("send ")) {
            // Send arbitrary hex byte
            String hexStr = cmd.substring(5);
            uint8_t byte = (uint8_t)strtol(hexStr.c_str(), NULL, 16);
            Serial.printf("[SEND] Sending 0x%02X...\n", byte);
            g_twiHal.sendByte(byte);
            uint8_t resp;
            if (g_twiHal.receiveByte(&resp, 2000)) {
                Serial.printf("[SEND] Response: 0x%02X\n", resp);
            } else {
                Serial.println("[SEND] No response (timeout)");
            }
        }
        else if (cmd == "frame") {
            // Send minimal TWI frame (just opcode + length + CRC)
            Serial.println("\n[FRAME] Sending minimal KMM frame...");
            uint8_t frame[] = {0xC2, 0x00, 0x04, 0x00};  // Opcode, len=4, control=0
            // Add CRC for [0x00] (control only)
            // CRC of [0x00] = ?
            for (int i = 0; i < 4; i++) {
                g_twiHal.sendByte(frame[i]);
            }
            // Wait and receive response
            delay(10);
            uint8_t resp[16];
            int count = 0;
            while (count < 16) {
                uint8_t b;
                if (!g_twiHal.receiveByte(&b, 500)) break;
                resp[count++] = b;
            }
            if (count > 0) {
                Serial.printf("[FRAME] Response (%d bytes): ", count);
                for (int i = 0; i < count; i++) Serial.printf("%02X ", resp[i]);
                Serial.println();
            } else {
                Serial.println("[FRAME] No response");
            }
        }
        else if (cmd.length() > 0) {
            Serial.printf("[CMD] Unknown command: '%s' (type 'help' for commands)\n", cmd.c_str());
        }
    }
    
    // Basic timing
    static uint32_t lastMs = 0;
    uint32_t now = millis();
    if (now - lastMs > 5) {
        lastMs = now;
    }
    
    // Touch debouncing
    static int32_t x, y;
    if (lcd.getTouch(&x, &y)) {
        delay(50);
    } else {
        delay(5);
    }
}
