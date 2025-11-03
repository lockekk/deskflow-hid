/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>

namespace deskflow::gui {

/**
 * @brief Helper functions for USB device operations
 */
class UsbDeviceHelper
{
public:
  /**
   * @brief Read serial number from USB CDC device
   * @param devicePath USB CDC device path (e.g., "/dev/ttyACM0")
   * @return Serial number string, or empty if not found
   *
   * On Linux, reads from sysfs: /sys/class/tty/ttyACM0/../../../../serial
   */
  static QString readSerialNumber(const QString &devicePath);
};

} // namespace deskflow::gui
