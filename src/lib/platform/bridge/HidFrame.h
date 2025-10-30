/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstdint>
#include <vector>

namespace deskflow::bridge {

enum class HidEventType : uint8_t {
  KeyboardPress = 0x01,
  KeyboardRelease = 0x02,
  MouseMove = 0x03,
  MouseButtonPress = 0x04,
  MouseButtonRelease = 0x05,
  MouseScroll = 0x06,
};

enum class HidReportType : uint8_t {
  Keyboard = 0x01,
  Mouse = 0x02,
  MouseWheel = 0x03,
};

/**
 * @brief HID Keyboard Report
 */
struct KeyboardReport {
  uint8_t modifiers = 0;
  uint8_t reserved = 0;
  uint8_t keycodes[6] = {0};

  std::vector<uint8_t> toPayload() const {
    return std::vector<uint8_t>{modifiers, reserved, keycodes[0], keycodes[1], keycodes[2], keycodes[3], keycodes[4], keycodes[5]};
  }
};

/**
 * @brief HID Mouse Report
 */
struct MouseReport {
  uint8_t buttons = 0;
  int8_t x = 0;
  int8_t y = 0;

  std::vector<uint8_t> toPayload() const {
    return std::vector<uint8_t>{buttons, static_cast<uint8_t>(x), static_cast<uint8_t>(y)};
  }
};

/**
 * @brief HID Mouse Wheel Report
 */
struct MouseWheelReport {
  int8_t wheel = 0;
  int8_t hwheel = 0;

  std::vector<uint8_t> toPayload() const {
    return std::vector<uint8_t>{static_cast<uint8_t>(wheel), static_cast<uint8_t>(hwheel)};
  }
};

/**
 * @brief HID Frame
 */
struct HidFrame {
  HidReportType type;
  std::vector<uint8_t> payload;
};

/**
 * @brief HID protocol packet wrapper
 *
 * Format (little endian):
 *   header (0xAA55) | type (1 byte) | length (1 byte) | payload (N bytes)
 */
struct HidEventPacket {
  static constexpr uint16_t MAGIC = 0xAA55;

  HidEventType type;
  std::vector<uint8_t> payload;

  std::vector<uint8_t> serialize() const;
};

} // namespace deskflow::bridge
