# Pico-HID Firmware Summary

## Repository Location
`/home/locke/checkouts/pico-hid`

## Purpose
Hardware bridge firmware for Raspberry Pi Pico 2 W that converts USB CDC HID events to Bluetooth LE HID reports, enabling PC keyboard/mouse control of mobile devices (iPad/iPhone/Android).

---

## Architecture Overview

The firmware uses a 4-layer architecture:

```
┌─────────────────────────────────────────┐
│  USB CDC Transport Layer                │  ← Receives framed HID events from deskflow-core
├─────────────────────────────────────────┤
│  HID Protocol Layer                     │  ← Parses event types and payloads
├─────────────────────────────────────────┤
│  BLE HID Layer                          │  ← Generates HID reports (keyboard, mouse)
├─────────────────────────────────────────┤
│  FreeRTOS + BLE Stack                   │  ← Manages tasks and BLE communication
└─────────────────────────────────────────┘
```

---

## Directory Structure

```
/home/locke/checkouts/pico-hid/
├── app/                          # Application code
│   ├── hid_protocol/             # HID event type definitions and protocol
│   │   ├── hid_protocol.h        # Event types enum, event structures
│   │   └── hid_protocol.c        # (Implementation if exists)
│   ├── usb/                      # USB CDC transport layer
│   │   ├── usb_cdc_transport.h   # CDC interface declarations
│   │   └── usb_cdc_transport.c   # Frame parsing, compact frame handling
│   └── ble/                      # Bluetooth LE HID layer
│       ├── hid_combo.h           # HID combo device interface
│       ├── hid_combo.c           # Report generation, event processing
│       └── hid_descriptor.h      # HID descriptor definitions (likely)
├── lib/                          # Third-party libraries
│   └── FreeRTOS/                 # Real-time operating system
├── CMakeLists.txt                # Build configuration
└── README.md                     # Project documentation
```

---

## Layer 1: HID Protocol Layer

**File**: `app/hid_protocol/hid_protocol.h`

### Event Types (Current Implementation)
```c
typedef enum {
    HID_EVENT_KEYBOARD_PRESS = 0x01,
    HID_EVENT_KEYBOARD_RELEASE = 0x02,
    HID_EVENT_MOUSE_MOVE = 0x03,
    HID_EVENT_MOUSE_BUTTON_PRESS = 0x04,
    HID_EVENT_MOUSE_BUTTON_RELEASE = 0x05,
    HID_EVENT_MOUSE_SCROLL = 0x06,
    // Consumer control support NOT YET IMPLEMENTED
} hid_event_type_t;
```

### Event Structures
```c
typedef struct {
    uint8_t modifiers;  // Modifier bitmap (Ctrl, Shift, Alt, etc.)
    uint8_t keycode;    // HID keyboard scancode
} hid_keyboard_event_t;

typedef struct {
    int16_t dx;  // Relative X movement
    int16_t dy;  // Relative Y movement
} hid_mouse_move_event_t;

typedef struct {
    uint8_t button_mask;  // Button bitmap (bit 0 = left, bit 1 = right, etc.)
} hid_mouse_button_event_t;

typedef struct {
    int8_t delta;  // Scroll wheel delta
} hid_mouse_scroll_event_t;

typedef struct {
    hid_event_type_t type;
    union {
        hid_keyboard_event_t keyboard;
        hid_mouse_move_event_t mouse_move;
        hid_mouse_button_event_t mouse_button;
        hid_mouse_scroll_event_t mouse_scroll;
    } data;
} hid_event_t;
```

---

## Layer 2: USB CDC Transport Layer

**File**: `app/usb/usb_cdc_transport.c`

### Frame Format
```
┌────────┬────────┬────────┬────────┬──────────────┐
│ Header │  Type  │ Length │ Payload│   ...        │
│ 0xAA55 │ 1 byte │ 1 byte │ N bytes│              │
└────────┴────────┴────────┴────────┴──────────────┘
```

### Compact Frame Sizes (Current)
```c
#define COMPACT_KEYBOARD_PRESS_SIZE 3      // type + modifiers + keycode
#define COMPACT_KEYBOARD_RELEASE_SIZE 3
#define COMPACT_MOUSE_MOVE_SIZE 5          // type + dx_low + dx_high + dy_low + dy_high
#define COMPACT_MOUSE_BUTTON_PRESS_SIZE 2  // type + button_mask
#define COMPACT_MOUSE_BUTTON_RELEASE_SIZE 2
#define COMPACT_MOUSE_SCROLL_SIZE 2        // type + delta
```

### Frame Parsing Logic
Located in `parse_hid_frame()` function (around line 150):
- Validates frame header (0xAA55)
- Extracts event type and payload length
- Parses payload based on event type
- Handles little-endian multi-byte values

---

## Layer 3: BLE HID Layer

**File**: `app/ble/hid_combo.c`

### HID Report IDs (Current)
```c
#define HID_REPORT_ID_KEYBOARD 1  // Standard keyboard (8 bytes)
#define HID_REPORT_ID_MOUSE 2     // Standard mouse (4 bytes)
// Consumer control Report ID 3 NOT YET IMPLEMENTED
```

### Keyboard Report Format (Report ID 1)
```
Byte 0: Report ID (0x01)
Byte 1: Modifier bitmap
Byte 2: Reserved (0x00)
Byte 3-8: Up to 6 concurrent key scancodes
```

### Mouse Report Format (Report ID 2)
```
Byte 0: Report ID (0x02)
Byte 1: Button bitmap
Byte 2: X movement (signed 8-bit)
Byte 3: Y movement (signed 8-bit)
```

### State Management
```c
static struct {
    uint8_t keyboard_report[8];  // Current keyboard state
    uint8_t mouse_report[4];     // Current mouse state
} hid_state;
```

### Event Processing
Function: `hid_combo_process_event(const hid_event_t *event)`
- Switch on event type
- Update internal state
- Send HID report via `tud_hid_report()`

---

## Layer 4: FreeRTOS & BLE Stack

**Location**: `lib/FreeRTOS/` and platform BLE libraries

### Key Components
- **Task Management**: Separate tasks for USB CDC RX, HID event processing, BLE TX
- **Queue Management**: Event queues between layers
- **BLE GATT Profile**: HID-over-GATT service with Report characteristic
- **Pairing/Security**: BLE pairing state machine (implementation details TBD)

---

## Current Limitations

### Missing Features
1. **Consumer Control Support**: No Report ID 3 for media keys
   - No `HID_EVENT_CONSUMER_CONTROL_PRESS/RELEASE` event types
   - No consumer control descriptor in HID report map
   - No consumer control state tracking or report generation

2. **Advanced HID Features** (if needed):
   - Gamepad/joystick support
   - Touchscreen absolute positioning
   - Custom vendor-specific reports

### Protocol Version
- Current protocol appears to be **version 1.0**
- No explicit version negotiation in CDC handshake (yet)

---

## Integration with Deskflow Bridge Client

### Data Flow
```
BridgePlatformScreen (PC)
    ↓ sendConsumerControlEvent(type=0x07, usageCode=0x00E9)
    ↓ HidEventPacket::serialize()
    ↓ CdcTransport::send()
    ↓ [USB CDC to Pico 2 W]
    ↓ usb_cdc_transport.c::parse_hid_frame()
    ↓ hid_combo.c::hid_combo_process_event()
    ↓ tud_hid_report(REPORT_ID_3, [0x03, 0xE9, 0x00])
    ↓ [BLE HID to iPad/Android]
    ✓ Volume Up
```

### Protocol Alignment
- **Deskflow Client**: Defines event types 0x01-0x08 (including consumer control)
- **Pico Firmware**: Only implements 0x01-0x06 (keyboard + mouse)
- **Gap**: Event types 0x07 (ConsumerControlPress) and 0x08 (ConsumerControlRelease) need implementation

---

## Implementation Plan for Consumer Control

See detailed plan in previous response. Summary:

### Phase 1: Protocol Layer
- Add `HID_EVENT_CONSUMER_CONTROL_PRESS = 0x07`
- Add `HID_EVENT_CONSUMER_CONTROL_RELEASE = 0x08`
- Add `hid_consumer_control_event_t` structure with `uint16_t usage_code`

### Phase 2: USB Transport
- Add compact frame parsing for 2-byte consumer control payload (little-endian)

### Phase 3: BLE HID Layer
- Add Report ID 3 for consumer control
- Implement `send_consumer_control_report(uint16_t usage_code)`
- Update `hid_combo_process_event()` with new cases

### Phase 4: HID Descriptor
- Append Consumer Devices (0x0C) collection to HID descriptor
- 16-bit usage code, single report count

---

## Key Files Reference

| File | Lines of Interest | Purpose |
|------|-------------------|---------|
| `app/hid_protocol/hid_protocol.h` | 20-27 | Event type enum |
| `app/hid_protocol/hid_protocol.h` | 60-70 | Event union structure |
| `app/usb/usb_cdc_transport.c` | ~45 | Compact frame size definitions |
| `app/usb/usb_cdc_transport.c` | ~150 | Frame parsing switch statement |
| `app/ble/hid_combo.c` | ~15 | Report ID definitions |
| `app/ble/hid_combo.c` | ~30 | State tracking structure |
| `app/ble/hid_combo.c` | ~50-120 | HID descriptor array |
| `app/ble/hid_combo.c` | ~180 | Event processing switch statement |

---

## Testing Strategy

### Unit Tests (if available)
- Protocol layer: Event structure serialization/deserialization
- Transport layer: Frame parsing edge cases

### Integration Tests
1. **USB CDC Loopback**: Send test frames, verify parsed events
2. **BLE HID Sniffer**: Capture BLE packets, validate report format
3. **End-to-End**: deskflow-core → Pico → iPad, verify input works

### Test Devices
- **Keyboard**: Volume Up/Down, Play/Pause, Brightness
- **Mouse**: Movement, buttons, scroll wheel
- **Consumer Control** (after implementation): Media keys

---

## Build Notes

### CMake Configuration
- Target: `pico2_w` (RP2350 with wireless)
- SDK: Pico SDK 2.x
- Dependencies: FreeRTOS, TinyUSB, BTstack (or pico-sdk BLE)

### Build Commands
```bash
cd /home/locke/checkouts/pico-hid
mkdir -p build && cd build
cmake ..
make -j$(nproc)
# Output: pico-hid.uf2
```

### Flashing
1. Hold BOOTSEL button on Pico 2 W
2. Connect USB
3. Copy `pico-hid.uf2` to mounted drive
4. Device reboots and runs firmware

---

## Debugging

### Logging
- USB CDC transport likely has debug UART output
- Check for `printf()` or custom logging macros

### Common Issues
1. **Frame parsing errors**: Check magic header 0xAA55
2. **Report not sending**: Verify BLE connection state
3. **Wrong HID codes**: Validate mapping tables in deskflow client

---

## Future Enhancements

1. **Configuration Protocol**: Allow runtime config (screen resolution, arch, pairing mode)
2. **OTA Updates**: Firmware update over BLE
3. **Multi-Device Pairing**: Switch between multiple paired devices
4. **Latency Optimization**: Reduce USB → BLE latency
5. **Power Management**: Sleep modes when idle

---

## Protocol Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | Initial | Keyboard + Mouse support (events 0x01-0x06) |
| 1.1 | TBD | Consumer Control support (events 0x07-0x08) |

---

## Contact & Maintenance

- **Repository Owner**: User `locke`
- **Bridge Client Repo**: `/home/locke/checkouts/deskflow-lim`
- **Related Docs**: See `PLAN.md` in deskflow-lim repo

---

**Last Updated**: 2025-11-02
**Firmware Status**: Consumer control implementation pending
