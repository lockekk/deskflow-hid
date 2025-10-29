# USB-HID Bridge Device Plan

## Problem
Need to extend PC keyboard/mouse control to mobile devices (iPad/iPhone/Android) that don't support standard Deskflow client installation.

## Solution
Create a hardware bridge using Pico 2 W that converts Deskflow protocol to Bluetooth LE HID, allowing PC to control mobile devices wirelessly through a USB-connected bridge device.

The software portion runs entirely inside `deskflow-core` and integrates with the existing client/server stack. All new behavior must keep protocol compatibility with the upstream server.

## Requirements

### 1. deskflow (GUI) - Server Only
- GUI only launches server mode
- GUI blocks duplicate GUI instances (unchanged)
- Reason: GUI is for server configuration and USB device management only

### 2. deskflow (GUI) - USB Device Event Subscription
- Subscribe to USB device plug/unplug events via platform-specific APIs:
  - Windows: `WM_DEVICECHANGE` (already used in MSWindowsScreen.cpp)
  - macOS: IOKit notifications (IOKit already imported in platform code)
  - X11/Wayland: udev (needs implementation)
- On matched vendor USB CDC device plug:
  - Open Pico configuration window (fetch/configure arch, screen info)
  - GUI directly communicates with Pico via USB CDC
  - Configuration stored in Pico's flash memory
  - After config window closed, spawn `deskflow-core bridge-client --name <unique-name> --usb-cdc-device <device-path> localhost:<port>`
- On matched vendor USB CDC device unplug:
  - Kill corresponding bridge client process
- Reason: Automatically create/destroy clients based on physical device presence

Note: Pico configuration is a GUI feature (not part of deskflow-core), bridge client only reads existing config from Pico on startup

### 3. deskflow-core - Multiple Instance Support
- Allow multiple instances based on role + name
- Shared memory key strategy:
  - Server: `"deskflow-core-server"` (only 1 allowed)
  - Client: `"deskflow-core-client-<name>"` (multiple allowed, each unique)
    - Adjust the bootstrap in `deskflow-core.cpp` to create a per-client `QSharedMemory` key before parsing arguments.
    - Each bridge instance sets its own `QSettings` file (e.g. under `settings/<name>.conf`) immediately after parsing CLI so instances do not overwrite the default client/server configuration.
- Reason: Enable 1 server + N clients on same machine without instance blocking

### 4. Special USB-HID Bridge Client Implementation

#### Overview
A new client type that acts as a bridge: converts Deskflow events → HID reports → Pico 2 W (via USB CDC) → Bluetooth LE HID → Mobile device (iPad/iPhone/Android)

#### Arguments (passed from GUI)
1. `--name <unique-name>`: For multi-instance support
2. `--usb-cdc-device <device-path>`: USB CDC device for Pico 2 W communication

#### Architecture Flow
```
Deskflow Server (PC)
    ↓ (network)
USB-HID Bridge Client (PC)
    ↓ (convert events to HID reports)
    ↓ (pack as frames via USB CDC)
Pico 2 W (hardware bridge)
    ↓ (Bluetooth LE HID)
Mobile Device (iPad/iPhone/Android)
```

#### Client Initialization Sequence
Performed before the standard `ClientApp` starts:
1. Query Pico 2 W over CDC for **arch** (e.g., `bridge-ios`, `bridge-android`).
   - Map this to a new bridge-specific `IPlatformScreen` implementation variant (`BridgePlatformScreen`) that knows which HID layout to emit.
   - Instantiate a matching `BridgeClientApp` (derived from `ClientApp`) that overrides `createScreen()` to return the bridge screen.
2. Query Pico 2 W for **screen info**: resolution, rotation, physical size (inches), scale factor.
   - Feed this into `BridgePlatformScreen::getShape()` so Deskflow coordinates match the mobile display.
3. Configure TLS and remote host using the per-instance settings file so the bridge client connects with the user’s configured security settings.

#### Key Differences from Standard Client
- **No local event injection**: Doesn't fake mouse/keyboard events on PC
- **Bridge platform screen**: New `IPlatformScreen` implementation packages `fakeMouse*` and `fakeKey*` calls into HID reports sent over CDC
- **USB CDC transport**: Sends framed HID data to Pico 2 W
- **Dynamic platform adaptation**: Platform type determined by Pico's connected device, not PC platform
- **Screen info from Pico**: Uses mobile device screen properties, not PC screen
- **Clipboard handling**: Bridge client disables clipboard sync at handshake (respond with no clipboard data) and implements `setClipboard()` as a no-op with logging so the server doesn't stall
- **Independent settings file**: Each bridge client instance has its own QSettings file
  - Path: `~/.config/deskflow/bridge-client-<name>.conf` (Linux), similar for Windows/macOS
  - Settings managed independently by bridge client process
  - Not shared with main deskflow settings
  - Allows multiple bridge clients with separate configurations
- **TLS compatibility**: Bridge client continues to respect server TLS settings; when TLS is enabled, connect using the same security level as standard clients.

### 5. CLI / Argument Handling
- Extend `CoreArgParser` options with:
  - `--bridge` (boolean) to indicate bridge mode.
  - `--usb-cdc-device <dev>` (string) for the Pico CDC path.
  - Optional `--bridge-settings-file <path>` to override per-instance settings (defaults to `settings/<name>.conf`).
- Update `deskflow-core` `main()` to:
  1. Inspect arguments for bridge mode before the single-instance guard.
  2. Select the shared-memory key and settings file based on `--name`/`--bridge-settings-file`.
  3. Fetch Pico configuration, instantiate `BridgeClientApp`, and run it.

### 6. Transport / HID Framing
- Bridge client owns a CDC transport helper that:
  - Opens the CDC device and exchanges configuration frames (arch + screen info).
  - Provides `sendHidReport(const HidFrame &frame)` for the platform screen.
- `BridgePlatformScreen` maintains button/modifier state so Deskflow’s “release all keys” logic still works.
- Provide unit coverage or logging hooks to validate HID packets during development.
