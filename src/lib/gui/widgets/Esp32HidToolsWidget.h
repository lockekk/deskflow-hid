/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTextEdit>
#include <cstdint>
#include <vector>

namespace deskflow::gui {

class Esp32HidToolsWidget : public QDialog
{
  Q_OBJECT

public:
  explicit Esp32HidToolsWidget(const QString &devicePath, QWidget *parent = nullptr);
  ~Esp32HidToolsWidget() override = default;

private Q_SLOTS:
  void refreshPorts();
  void onBrowseFactory();
  void onFlashFactory();
  void onBrowseUpgrade();
  void onFlashUpgrade();
  void onCopyInfo();
  void onCopySerialClicked();
  void onActivateClicked();
  void onTabChanged(int index);

private:
  void refreshDeviceState();
  // Port Selection
  QComboBox *m_portCombo;
  QPushButton *m_refreshPortsBtn;

  // Factory Tab
  QLineEdit *m_factoryPathEdit;
  QPushButton *m_factoryBrowseBtn;
  QPushButton *m_factoryFlashBtn;
  QPushButton *m_copyInfoBtn;

  // Upgrade Tab
  QLineEdit *m_upgradePathEdit;
  QPushButton *m_upgradeBrowseBtn;
  QPushButton *m_upgradeFlashBtn;

  // Activation Tab
  QWidget *m_tabActivation;
  QLabel *m_labelActivationState;
  QLabel *m_lineSerial;
  QPushButton *m_btnCopySerial;
  QWidget *m_groupActivationInput; // Container to hide/show
  QLineEdit *m_lineActivationKey;
  QPushButton *m_btnActivate;

  // Common
  QTextEdit *m_logOutput;

  // State
  QString m_devicePath;
  bool m_isTaskRunning = false;

  void setupUI();
  void log(const QString &message);
  void setControlsEnabled(bool enabled);
  template <typename Function> void runBackgroundTask(Function func);
  std::vector<uint8_t> readFile(const QString &path);
  void reject() override;
};

} // namespace deskflow::gui
