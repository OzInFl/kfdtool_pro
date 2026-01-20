# KFDtool Pro - Debug Session Notes

## Current Issue
Radio responds with 0xC3 (unknown error code) to all KMM frames, regardless of content.
The 0xC3 response is always followed by: `01 80 00 01 84 83 FF`

**Key observation**: 0xC3 = 0xC2 with bit 0 flipped. This suggests a potential timing/sampling issue.

## Changes Made This Session

### 1. Added 10ms delay after receiving 0xD0/0xD1 ready response
File: `src/kfd_protocol.cpp`
- Radio may need time to transition from key signature mode to KMM mode

### 2. Made stop bit mode configurable
Files: `include/twi_hal.h`, `src/twi_hal.cpp`
- Added `setStopBitMode(bool useBusy)` method
- Default: `_useBusyStopBits = true` (KFDtool style)
- Can toggle to try standard IDLE stop bits

### 3. Changed from sendByte loop to sendBytesFast
Files: `src/kfd_protocol.cpp`
- All KMM frame transmission now uses sendBytesFast() instead of individual sendByte() calls
- Provides more consistent timing between bytes

### 4. Added serial debug commands
File: `src/main.cpp`
Commands available via serial console:
- `test` or `t` - Run inventory test
- `aes` or `a` - Test AES key at SLN 202  
- `stop0` - Use IDLE stop bits (standard async)
- `stop1` - Use BUSY stop bits (KFDtool style)
- `baud2` - Set 2 kbaud (slower)
- `baud4` - Set 4 kbaud (standard)
- `raw` - Send single 0xC2 opcode after session
- `send XX` - Send arbitrary hex byte XX
- `frame` - Send minimal KMM frame
- `status` or `s` - Show current status
- `selftest` - Run hardware self-test
- `help` or `h` - Show help

## Debugging Steps to Try

### 1. Test with new fast send mode
```
test
```

### 2. Try different stop bit modes
```
stop0
test
stop1
test
```

### 3. Try slower baud rate
```
baud2
test
baud4
test
```

### 4. Test raw opcodes
```
raw
send C2
frame
```

## Analysis

### Response pattern analysis
The 0xC3 response:
- `0xC3` = 0xC2 with bit 0 flipped (bit error?)
- `0x01 0x80` = Length field (384 bytes - invalid, suggests corruption)
- `0x00` = Control byte
- `0x01 0x84 0x83` = Dest RSI or error data
- `0xFF` = Padding/terminator

The `0x84` = AES-256 algorithm ID suggests the radio has some internal state about the last operation.

### Root cause hypotheses
1. **Timing jitter**: ESP32 delayMicroseconds may not be accurate enough
2. **GPIO mode**: Need true open-drain, not push-pull
3. **Voltage levels**: 3.3V ESP32 vs 5V radio logic
4. **Inter-byte timing**: Gap between bytes causing sync loss

## Verified Correct
- Frame construction: 100% matches KFDtool reference (verified with Python)
- CRC calculation: TIA-102.AACD-A, no final XOR, LOW-HIGH byte order
- KMM structure: Matches C# reference exactly
- Key signature timing: 100ms BUSY + 5ms IDLE
- Byte frame: start(0) + 8 data reversed + parity + 4 stop bits

## Hardware Investigation Needed
1. **Logic analyzer capture**: Compare actual waveform with working KFDtool
2. **Voltage measurement**: Verify HIGH = 5V (or acceptable), LOW = 0V
3. **Timing measurement**: Verify bit period = 250µs ± tolerance
4. **Pullup resistor**: Check value and voltage source
