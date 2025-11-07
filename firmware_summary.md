# ESP32 HID Firmware Summary

## Repository Location
`/home/locke/checkouts/esp32-hid`

## Purpose
ESP32-based replacement for the original Pico firmware that speaks Deskflow’s USB framing on the wired side and emits BLE HID keyboard/mouse/consumer-control reports to phones and tablets, keeping the desktop changes minimal while solving the Pico 2 W radio issues (`PLAN.md:3-17` in this repo and `PLAN.md:3-12` in `/home/locke/checkouts/deskflow-lim`).

---

## Architecture Overview

```
Deskflow Client ─USB CDC frames─> usb_cdc_transport ─parsed hid_event_t─> hid_combo
     ↑                                                             │
     └────────────── HELLO/ACK, display info ─ device_display_config│
                                                                   ▼
                                                   ESP-IDF BLE stack → Mobile host
```

- **HID protocol layer** (`components/hid_protocol/include/hid_protocol.h:21-109`, `src/hid_protocol.c`) keeps the 0xAA55 framing, keyboard/mouse/consumer event structs, parser stats, and helper logging.
- **USB transport** (`components/usb_transport/src/usb_cdc_transport.c:87-200` and `325-420`) runs on the ESP32-C3 USB Serial/JTAG peripheral, owns the RX FreeRTOS task, handshake state, compact mouse/key paths, and sends ACKs that advertise firmware version plus the baked-in display size from `components/hid_protocol/include/device_display_config.h:1-19`.
- **BLE combo layer** (`components/ble_hid/src/hid_combo.c:1-455`) queues prepared reports, tracks modifier state, converts consumer usages to Espressif `consumer_cmd_t`, and pushes data through `hid_dev_send_report()`.
- **Application entry** (`main/ble_hidd_demo_main.c:1-194`) initializes NVS, BT controller, HID profile, GAP/GATTS callbacks, and advertising/back-off logic that temporarily switches to non-connectable advertising when the phone drops the link to avoid iOS auto-reconnect storms.

The code-flow and boot/data sequences are further documented in `CODE_FLOW_EXPLANATION.md` and `BLE_HOST_DETECTION_ANALYSIS.md` at the repo root.

---

## Directory Structure Highlights

```
components/
  hid_protocol/      # Parser + shared display config
  usb_transport/     # USB Serial/JTAG framing and HELLO/ACK handshake
  ble_hid/           # HID combo queue + host heuristics
main/
  ble_hidd_demo_main.c, esp_hidd_prf_api.c, hid_device_le_prf.c # ESP-IDF HID example sources
docs/ (root)         # *_EXPLANATION.md analysis notes
sdkconfig*           # IDF build configs per chip
```

`plan.md` tracks project status and work items; right now hardware validation is still pending even though the software layers compile and link (`plan.md:10-16`).

---

## Key Firmware Behaviors

- **Deskflow handshake compatibility**: The RX task blocks until the desktop sends `HELLO`, then answers with `ACK` that includes protocol version, BLE-connected bit, and fixed 1080×2424 screen metadata so Deskflow can size the virtual screen (`components/usb_transport/src/usb_cdc_transport.c:335-377` plus `device_display_config.h:6-19`). Any malformed frames yield `USB_LINK_CONTROL_ERROR`.
- **Compact frame fast path**: Mouse delta, keyboard, button, and scroll compact frames bypass the parser and map header bytes straight into `hid_event_t` structs to minimize latency (`components/usb_transport/src/usb_cdc_transport.c:403-454`).
- **BLE host heuristics**: Connection-interval and MTU callbacks categorize the remote OS as iOS vs Android so Deskflow can tune gestures later; detections are logged and exposed via `hid_combo_get_host_os()` (`components/ble_hid/src/hid_combo.c:417-455` and expanded rationale in `BLE_HOST_DETECTION_ANALYSIS.md`).
- **Reconnect back-off**: When the phone intentionally drops the BLE link (e.g., iOS switching devices), the GAP handler swaps to non-connectable advertising for ~5 s before resuming fast connectable mode, reducing thrash when multiple hosts are nearby (`main/ble_hidd_demo_main.c:43-194`).
- **Report coverage**: The firmware already handles keyboard, mouse, and consumer-control usage IDs, matching Deskflow’s event set (`components/hid_protocol/include/hid_protocol.h:27-35`, `components/ble_hid/src/hid_combo.c:330-409`).

---

## Build & Flash Workflow

1. Select the SoC target (`idf.py set-target esp32c3`) and ensure the right `sdkconfig.defaults.*` is copied/merged for your board (README instructions at `README.md:18-59` still apply).
2. Build/flash/monitor in one shot with `idf.py -p <PORT> flash monitor`.
3. Use the ESP-IDF serial monitor (`Ctrl-]` to exit) to watch USB handshake logs (`USB_CDC`, `DeskflowHID`) for HELLO/ACK, BLE pairing messages, and host OS detection cues.

> Tip: Because the transport uses USB Serial/JTAG, no external USB-UART bridge is required—just plug the ESP32-C3 SuperMini into the PC; enumeration proves the driver is alive (`plan.md:11-16`).

---

## Testing Status & Next Steps

- ✅ Layers compile and integrate; docs capture boot flow, host detection, and unused code analysis (`CODE_FLOW_EXPLANATION.md`, `UNUSED_CODE_ANALYSIS.md`).
- ⚠️ Pending: On-hardware validation of USB enumeration, Deskflow client handshake, and BLE input on iOS/Android as tracked in `plan.md:41-52`.
- 📌 Follow-ups: dynamic display configuration and OTA/dual-host ideas remain in the “future work” bullets of `plan.md:18-53`; keep these in mind when adding ESP32-specific requirements to Deskflow’s GUI.

---

## Cross-Repo Dependencies

- Deskflow GUI/client expects the ESP32 bridge to speak the same link protocol and now filters on the Espressif VID 0x303A (`PLAN.md` in `/home/locke/checkouts/deskflow-lim`); ensure firmware USB descriptors stay consistent with the Serial/JTAG peripheral’s default enumeration.
- The deckflow bridge client relies on the ACK display dimensions reported here; update `device_display_config.h` if the ESP32 firmware starts querying the phone for live display info, then keep Deskflow’s `PLAN.md` and launcher arguments in sync.

---

**Last Verified**: current ESP32 firmware tree as of this task (see timestamps in repo). Update this summary whenever protocol changes, new boards/targets come online, or the testing status in `plan.md` advances.
