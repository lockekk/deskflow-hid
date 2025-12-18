/*
 * Deskflow-hid -- created by locke.huang@gmail.com
 */

#include "BridgeClientWidget.h"

#include "common/Settings.h"

#include <QHBoxLayout>
#include <QMessageBox>
#include <QSettings>
#include <QSignalBlocker>

namespace {
constexpr auto kLandscapeIconPath = ":/bridge-client/client/orientation_landspace.png";
constexpr auto kPortraitIconPath = ":/bridge-client/client/orientation_portrait.png";
} // namespace

namespace deskflow::gui {

BridgeClientWidget::BridgeClientWidget(
    const QString &screenName, const QString &devicePath, const QString &configPath, QWidget *parent
)
    : QGroupBox(screenName, parent),
      m_screenName(screenName),
      m_devicePath(devicePath),
      m_configPath(configPath)
{
  setMinimumWidth(650);
  setMaximumWidth(16777215); // QWIDGETSIZE_MAX

  // Create horizontal layout for buttons
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);

  // Create Connect button (toggleable)
  m_btnConnect = new QPushButton(tr("Connect"), this);
  m_btnConnect->setCheckable(true);
  m_btnConnect->setChecked(false); // Default disconnected
  m_btnConnect->setMinimumSize(80, 32);
  m_btnConnect->setToolTip(tr("Connect/disconnect bridge client"));

  // Create Configure button
  m_btnConfigure = new QPushButton(tr("Configure"), this);
  m_btnConfigure->setMinimumSize(80, 32);
  m_btnConfigure->setToolTip(tr("Configure bridge client settings"));

  // Create Delete button
  m_btnDelete = new QPushButton(tr("Delete"), this);
  m_btnDelete->setMinimumSize(80, 32);
  m_btnDelete->setToolTip(tr("Delete this bridge client configuration"));

  m_deviceNameLabel = new QLabel(tr("--"), this);
  m_deviceNameLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  m_deviceNameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  m_deviceNameLabel->setMinimumWidth(140);

  m_activeHostnameLabel = new QLabel(this);
  m_activeHostnameLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  m_activeHostnameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  m_activeHostnameLabel->setStyleSheet(
      "color: gray;"
  ); // Make it distinct, maybe? Or just normal. user said "next to", didn't specify style.

  // Activation state and orientation labels
  m_activationStateLabel = new QLabel(tr("unknown"), this);
  m_activationStateLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  m_activationStateLabel->setMinimumWidth(90);

  m_activationStateLabel->setMinimumWidth(90);

  // Add buttons and label to layout
  layout->addWidget(m_btnConnect);
  layout->addWidget(m_btnConfigure);
  layout->addWidget(m_btnDelete);
  layout->addSpacing(12);
  layout->addWidget(m_deviceNameLabel);        // Removed stretch, will share with hostname
  layout->addWidget(m_activeHostnameLabel, 1); // Give stretch to hostname
  layout->addWidget(m_activationStateLabel);

  // Connect signals
  connect(m_btnConnect, &QPushButton::toggled, this, &BridgeClientWidget::onConnectToggled);
  connect(m_btnConfigure, &QPushButton::clicked, this, &BridgeClientWidget::onConfigureClicked);
  connect(m_btnDelete, &QPushButton::clicked, this, &BridgeClientWidget::onDeleteClicked);

  refreshDeviceNameLabel();
  refreshActivationStateLabel();
  refreshButtonStates();
}

void BridgeClientWidget::setConnected(bool connected)
{
  m_isConnected = connected;
  {
    const QSignalBlocker blocker(m_btnConnect);
    m_btnConnect->setChecked(connected);
  }
  m_btnConnect->setText(connected ? tr("Disconnect") : tr("Connect"));
  refreshButtonStates();
}

void BridgeClientWidget::updateConfig(const QString &screenName, const QString &configPath)
{
  m_screenName = screenName;
  m_configPath = configPath;
  setTitle(screenName); // Update the group box title
  refreshActivationStateLabel();
  refreshDeviceNameLabel();
}

void BridgeClientWidget::setActivationState(const QString &activationState)
{
  QString normalized = activationState.trimmed().toLower();
  if (normalized.isEmpty()) {
    normalized = Settings::defaultValue(Settings::Bridge::ActivationState).toString().toLower();
  }
  m_activationState = normalized;
  m_activationStateLabel->setText(normalized);
  m_activationStateLabel->setToolTip(normalized);
}

void BridgeClientWidget::setDeviceName(const QString &deviceName)
{
  const QString trimmed = deviceName.trimmed();
  m_deviceName = trimmed;
  const QString display = trimmed.isEmpty() ? tr("--") : trimmed;
  m_deviceNameLabel->setText(display);
}

void BridgeClientWidget::setActiveHostname(const QString &hostname)
{
  m_activeHostname = hostname.trimmed();
  m_activeHostnameLabel->setText(m_activeHostname);

  if (m_isBleConnected) {
    m_activeHostnameLabel->setStyleSheet(QStringLiteral("color: red; font-weight: normal;"));
  } else {
    m_activeHostnameLabel->setStyleSheet(QStringLiteral("color: gray; font-weight: normal;"));
  }
}

void BridgeClientWidget::setBleConnected(bool connected)
{
  if (m_isBleConnected == connected)
    return;

  m_isBleConnected = connected;
  setActiveHostname(m_activeHostname);
}

void BridgeClientWidget::refreshActivationStateLabel()
{
  QString activationState = Settings::defaultValue(Settings::Bridge::ActivationState).toString();
  if (!m_configPath.isEmpty()) {
    QSettings config(m_configPath, QSettings::IniFormat);
    activationState = config.value(Settings::Bridge::ActivationState, activationState).toString();
  }
  setActivationState(activationState);
}

void BridgeClientWidget::refreshDeviceNameLabel()
{
  QString name = Settings::defaultValue(Settings::Bridge::DeviceName).toString();
  QString hostname = Settings::defaultValue(Settings::Bridge::ActiveProfileHostname).toString();

  if (!m_configPath.isEmpty()) {
    QSettings config(m_configPath, QSettings::IniFormat);
    name = config.value(Settings::Bridge::DeviceName, name).toString();
    hostname = config.value(Settings::Bridge::ActiveProfileHostname, hostname).toString();
  }
  setDeviceName(name);
  setActiveHostname(hostname);
}

void BridgeClientWidget::setDeviceAvailable(const QString &devicePath, bool available)
{
  m_deviceAvailable = available;
  m_devicePath = available ? devicePath : QString();

  refreshButtonStates();

  if (!available) {
    setStyleSheet("QGroupBox { color: gray; }");
  } else {
    setStyleSheet("");
  }
}

void BridgeClientWidget::setGroupLocked(bool locked, const QString &reason)
{
  m_groupLocked = locked;
  m_groupLockReason = reason;
  refreshButtonStates();
}

void BridgeClientWidget::refreshButtonStates()
{
  const bool connectEnabled = m_deviceAvailable && (!m_groupLocked || m_isConnected);
  m_btnConnect->setEnabled(connectEnabled);

  QString connectTooltip;
  if (!m_deviceAvailable) {
    connectTooltip = tr("Device not connected");
  } else if (m_groupLocked && !m_isConnected) {
    connectTooltip =
        m_groupLockReason.isEmpty() ? tr("Another profile for this device is already connected") : m_groupLockReason;
  } else {
    connectTooltip = tr("Connect/disconnect bridge client");
  }
  m_btnConnect->setToolTip(connectTooltip);

  const bool configureEnabled = !m_isConnected && !m_groupLocked;
  m_btnConfigure->setEnabled(configureEnabled);

  QString configureTooltip;
  if (m_isConnected) {
    configureTooltip = tr("Disconnect before configuring");
  } else if (m_groupLocked) {
    configureTooltip =
        m_groupLockReason.isEmpty() ? tr("This profile is locked because another one is connected") : m_groupLockReason;
  } else {
    configureTooltip = tr("Configure bridge client settings");
  }
  m_btnConfigure->setToolTip(configureTooltip);

  const bool deleteEnabled = !m_isConnected && !m_groupLocked;
  m_btnDelete->setEnabled(deleteEnabled);

  QString deleteTooltip;
  if (m_isConnected) {
    deleteTooltip = tr("Disconnect before deleting");
  } else if (m_groupLocked) {
    deleteTooltip =
        m_groupLockReason.isEmpty() ? tr("This profile is locked because another one is connected") : m_groupLockReason;
  } else {
    deleteTooltip = tr("Delete this bridge client configuration");
  }
  m_btnDelete->setToolTip(deleteTooltip);
}

void BridgeClientWidget::onConnectToggled(bool checked)
{
  m_isConnected = checked;
  m_btnConnect->setText(checked ? tr("Disconnect") : tr("Connect"));
  refreshButtonStates();

  Q_EMIT connectToggled(m_devicePath, m_configPath, checked);
}

void BridgeClientWidget::onConfigureClicked()
{
  Q_EMIT configureClicked(m_devicePath, m_configPath);
}

void BridgeClientWidget::onDeleteClicked()
{
  // Show warning and confirmation dialog
  QMessageBox::StandardButton reply = QMessageBox::question(
      this, tr("Delete Configuration"),
      tr("Are you sure you want to delete this bridge client configuration?\n\nThis action cannot be undone."),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No
  );

  if (reply == QMessageBox::Yes) {
    Q_EMIT deleteClicked(m_devicePath, m_configPath);
  }
}

} // namespace deskflow::gui
