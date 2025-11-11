/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UsbDeviceHelper.h"

#include "platform/bridge/CdcTransport.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTextStream>

namespace deskflow::gui {

namespace {

#ifdef Q_OS_LINUX
QString canonicalUsbDevicePath(const QString &devicePath)
{
  QFileInfo deviceInfo(devicePath);
  QString ttyName = deviceInfo.fileName();

  const QString ttyBasePath = QStringLiteral("/sys/class/tty/%1").arg(ttyName);
  QFileInfo ttyLink(ttyBasePath);
  if (!ttyLink.exists()) {
    qWarning() << "TTY entry does not exist for" << devicePath << ":" << ttyBasePath;
    return QString();
  }

  QFileInfo deviceLink(ttyBasePath + QStringLiteral("/device"));
  return deviceLink.canonicalFilePath();
}

QString readUsbAttribute(const QString &canonicalDevicePath, const QString &attribute)
{
  if (canonicalDevicePath.isEmpty())
    return QString();

  QDir currentDir(canonicalDevicePath);
  constexpr int kMaxTraversal = 6;
  for (int depth = 0; depth < kMaxTraversal; ++depth) {
    const QString candidate = currentDir.absoluteFilePath(attribute);
    QFile file(candidate);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QTextStream in(&file);
      const QString value = in.readLine().trimmed();
      file.close();
      if (!value.isEmpty())
        return value;
    }
    if (!currentDir.cdUp())
      break;
  }

  return QString();
}
#endif

} // namespace

QString UsbDeviceHelper::readSerialNumber(const QString &devicePath)
{
#if defined(Q_OS_LINUX) || defined(Q_OS_WIN)
  // Read serial number via CDC command from firmware
  // This ensures we only read when device is not opened by bridge client

  qDebug() << "Reading serial number via CDC for device:" << devicePath;

  try {
    // Create CDC transport instance
    deskflow::bridge::CdcTransport transport(devicePath);

    // Check if device is busy by attempting to open it
    // This will fail if the bridge client already has it open
    if (!transport.open()) {
      qDebug() << "Device is busy or not accessible (likely opened by bridge client):" << devicePath;
      return QString(); // Device is in use, don't block
    }

    // Read serial number via CDC command
    std::string serial;
    if (transport.fetchSerialNumber(serial)) {
      transport.close();
      QString qSerial = QString::fromStdString(serial);
      if (!qSerial.isEmpty()) {
        qDebug() << "Read serial number via CDC for" << devicePath << ":" << qSerial;
        return qSerial;
      } else {
        qDebug() << "Device returned empty serial number:" << devicePath;
        return QString();
      }
    } else {
      qWarning() << "Failed to read serial number via CDC for" << devicePath
                 << "error:" << QString::fromStdString(transport.lastError());
      transport.close();
      return QString();
    }

  } catch (const std::exception &e) {
    qWarning() << "Exception reading serial number via CDC for" << devicePath
               << "error:" << e.what();
    return QString();
  }

#elif defined(Q_OS_IOS)
  // TODO: Implement CDC serial number reading for iOS
  // iOS has limited access to CDC devices, may need different approach
  qWarning() << "Serial number reading not implemented for iOS platform";
  return QString();

#else
  qWarning() << "Serial number reading not implemented for this platform";
  return QString();
#endif
}

QMap<QString, QString> UsbDeviceHelper::getConnectedDevices()
{
  QMap<QString, QString> devices;

#ifdef Q_OS_LINUX
  // Scan /dev for ttyACM* devices
  QDir devDir("/dev");
  QStringList filters;
  filters << "ttyACM*";
  QStringList deviceFiles = devDir.entryList(filters, QDir::System);

  for (const QString &deviceFile : deviceFiles) {
    QString devicePath = QStringLiteral("/dev/%1").arg(deviceFile);

    if (!isSupportedBridgeDevice(devicePath)) {
      qDebug() << "Skipping non-bridge CDC device" << devicePath;
      continue;
    }

    QString serialNumber = readSerialNumber(devicePath);

    if (!serialNumber.isEmpty()) {
      devices[devicePath] = serialNumber;
    }
  }

  qDebug() << "Found" << devices.size() << "connected USB CDC devices";
#elif defined(Q_OS_WIN)
  // On Windows, enumerate COM ports
  // Scan registry for available COM ports
  for (int i = 1; i <= 256; i++) {
    QString devicePath = QString("\\\\.\\COM%1").arg(i);

    if (isSupportedBridgeDevice(devicePath)) {
      QString serialNumber = readSerialNumber(devicePath);
      devices[devicePath] = serialNumber.isEmpty() ? QString("COM%1").arg(i) : serialNumber;
      qDebug() << "Found bridge device at" << devicePath;
    }
  }

  qDebug() << "Found" << devices.size() << "connected USB CDC devices on Windows";
#endif

  return devices;
}

bool UsbDeviceHelper::isSupportedBridgeDevice(const QString &devicePath)
{
#ifdef Q_OS_LINUX
  const QString canonicalPath = canonicalUsbDevicePath(devicePath);
  const QString vendorId = readUsbAttribute(canonicalPath, QStringLiteral("idVendor")).toLower();
  const QString productId = readUsbAttribute(canonicalPath, QStringLiteral("idProduct")).toLower();
  if (vendorId.isEmpty())
    return false;

  if (vendorId == kEspressifVendorId) {
    if (productId.isEmpty() || productId == kEspressifProductId) {
      return true;
    }
    qDebug() << "Device" << devicePath << "has Espressif vendor id but unexpected product id" << productId;
    return true;
  }

  qDebug() << "Device" << devicePath << "has unsupported vendor id" << vendorId;
  return false;
#elif defined(Q_OS_WIN)
  // On Windows, try to open and handshake with the device
  // This is simpler than querying hardware IDs from the registry
  deskflow::bridge::CdcTransport transport(devicePath);
  if (!transport.open()) {
    return false;
  }

  bool isSupported = transport.hasDeviceConfig();
  transport.close();
  return isSupported;
#else
  Q_UNUSED(devicePath);
  return false;
#endif
}

bool UsbDeviceHelper::verifyBridgeHandshake(
    const QString &devicePath, deskflow::bridge::FirmwareConfig *configOut, int timeoutMs
)
{
#if defined(Q_OS_LINUX) || defined(Q_OS_WIN)
  Q_UNUSED(timeoutMs);

  deskflow::bridge::CdcTransport transport(devicePath);
  if (!transport.open()) {
    qWarning() << "Bridge handshake failed for" << devicePath
               << ":" << QString::fromStdString(transport.lastError());
    return false;
  }

  if (transport.hasDeviceConfig()) {
    const auto &cfg = transport.deviceConfig();
    if (configOut != nullptr) {
      *configOut = cfg;
    }

    std::string deviceName;
    if (transport.fetchDeviceName(deviceName)) {
      if (configOut != nullptr) {
        configOut->deviceName = deviceName;
      }
    } else {
      qWarning() << "Unable to fetch device name:" << QString::fromStdString(transport.lastError());
    }

    const QString nameForLog = QString::fromStdString(cfg.deviceName);

    qInfo() << "Bridge handshake successful with" << devicePath
            << "proto:" << cfg.protocolVersion
            << "hid:" << (cfg.hidConnected ? 1 : 0)
            << "host_os:" << cfg.hostOsString()
            << "ble_interval_ms:" << cfg.bleIntervalMs
            << "activated:" << (cfg.productionActivated ? 1 : 0)
            << "fw_bcd:" << cfg.firmwareVersionBcd
            << "hw_bcd:" << cfg.hardwareVersionBcd
            << "name:" << nameForLog;
  } else {
    qInfo() << "Bridge handshake successful with" << devicePath << "(no config payload)";
  }

  transport.close();
  return true;
#else
  Q_UNUSED(devicePath)
  Q_UNUSED(timeoutMs)
  Q_UNUSED(configOut)
  return false;
#endif
}

} // namespace deskflow::gui
