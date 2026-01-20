# KFDtool Professional - ESP32 Edition

A full-featured P25 Key Fill Device implementation for the ESP32-S3 platform (WT32-SC01-Plus), inspired by and compatible with the [KFDtool](https://github.com/omahacommsys/KFDtool) desktop application.

## Features

### TIA-102.AACD-A Compliant Operations
- **Keyload** - Load single or multiple encryption keys
- **Key Erase** - Erase specific keys from radio
- **Erase All Keys** - Zeroize all keys in radio
- **View Key Info** - Query key information from radio
- **View Keyset Info** - Query keyset information
- **RSI Configuration** - View/Load Individual RSI
- **KMF RSI** - View/Load KMF RSI
- **MNP Configuration** - View/Load Message Number Pair
- **Keyset Activation** - Activate specific keyset
- **MR Emulator** - Receive keys from another KFD (coming soon)

### Security Features
- **User Authentication** - Operator and Admin roles with PIN-based login
- **AES-256 Encrypted Storage** - FIPS-compliant container encryption
- **Session Timeout** - Automatic logout after inactivity
- **PIN Lockout** - Protection against brute force attacks
- **Secure Key Generation** - Cryptographically random key generation

### Key Container Management
- **Multiple Containers** - Organize keys into separate containers
- **Key Groups** - Group related keys within containers
- **Algorithm Support**: AES-256, DES-OFB, 2-key 3DES, 3-key 3DES, ARC4, ADP
- **SD Card Backup/Restore** - Backup containers to SD card

### User Interface
- **Splash Screen** - Device serial number display on startup
- **Professional Dark Theme** - Modern, clean interface
- **Touch-Optimized** - Full touchscreen navigation
- **Progress Indicators** - Real-time operation status
- **Confirmation Dialogs** - Prevent accidental key deletion

## Hardware Requirements

### Target Platform
- **WT32-SC01-Plus** (ESP32-S3 + 3.5" 320x480 TFT LCD)
  - ESP32-S3 Dual-core @ 240MHz
  - 8MB PSRAM
  - 16MB Flash
  - 3.5" 320x480 Capacitive Touch LCD (ST7796 + FT5x06)

### TWI Connection (Default GPIO)
| Pin | Function | Description |
|-----|----------|-------------|
| GPIO 11 | DATA | Bidirectional data (open-drain with 4.7kΩ pull-up to 5V) |
| GPIO 10 | SENSE | Radio connection detection |
| GND | GND | Ground reference |

### Radio Cable Pinout (3.5mm TRS)
- **Tip** → DATA
- **Ring** → SENSE
- **Shield** → GND

## Building

### Prerequisites
- [PlatformIO](https://platformio.org/) (VSCode extension or CLI)
- USB-C cable for programming

### Build Steps
```bash
# Clone the project
cd kfdtool_pro

# Build
pio run

# Upload to device
pio run --target upload

# Monitor serial output
pio device monitor
```

## Configuration

### Default PINs
- **Operator PIN**: 1111
- **Admin PIN**: 5000

**Change these on first login for security!**

### Settings (Admin Only)
- TWI Pin Configuration
- Transfer Speed (1-9 kbaud)
- Display Brightness
- Session Timeout
- Auto-save Preferences

## File Structure

```
kfdtool_pro/
├── include/
│   ├── container.h      # Key container management
│   ├── crypto.h         # AES-256 encryption
│   ├── device_info.h    # Device info & user management
│   ├── kfd_protocol.h   # P25 KMM protocol layer
│   ├── lv_conf.h        # LVGL configuration
│   ├── p25_defs.h       # P25 protocol definitions
│   ├── twi_hal.h        # Three-Wire Interface HAL
│   └── ui.h             # User interface
├── src/
│   ├── main.cpp         # Entry point & initialization
│   ├── container.cpp    # Container implementation
│   ├── crypto.cpp       # Crypto implementation
│   ├── device_info.cpp  # Device/user implementation
│   ├── kfd_protocol.cpp # Protocol implementation
│   ├── twi_hal.cpp      # TWI HAL implementation
│   └── ui.cpp           # UI implementation
├── data/                # LittleFS data files
└── platformio.ini       # PlatformIO configuration
```

## Protocol Support

### Supported Radios
- Motorola ASTRO 25 Series (APX, XTS, XTL)
- Harris XG/XL Series
- EF Johnson VP Series
- Kenwood NEXEDGE (P25 mode)
- Other TIA-102.AACD-A compliant radios

### Supported Algorithms
| ID | Algorithm | Key Length |
|----|-----------|------------|
| 0x81 | DES-OFB | 8 bytes (64 bits) |
| 0x82 | 2-key 3DES | 16 bytes (128 bits) |
| 0x83 | 3-key 3DES | 24 bytes (192 bits) |
| 0x84 | AES-256 | 32 bytes (256 bits) |
| 0x85 | AES-128 | 16 bytes (128 bits) |
| 0x9F | ARC4 | 13 bytes (104 bits) |
| 0xAA | ADP | 5 bytes (40 bits) |

## License

This project is open source. Based on the KFDtool project by omahacommsys.

## Acknowledgments

- [KFDtool](https://github.com/omahacommsys/KFDtool) - Original KFDtool project
- [LVGL](https://lvgl.io/) - Graphics library
- [LovyanGFX](https://github.com/lovyan03/LovyanGFX) - Display driver
- [ArduinoJson](https://arduinojson.org/) - JSON parsing

## Safety Notice

**This device handles encryption keys.** Always:
- Change default PINs immediately
- Use in secure environments only
- Follow your organization's key management policies
- Never share encryption keys over unsecured channels
- Properly dispose of old keys according to policy

## Version

- Firmware: 1.0.0
- Protocol: TIA-102.AACD-A
- Hardware: WT32-SC01-Plus
