// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PlatformIntegration.h"

#include "AppController.h"
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KNotification>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGuiApplication>
#include <QPointer>
#include <QUrl>

namespace {
class KdeDesktopIntegration final : public DesktopIntegration {
public:
  void configureShortcut(QAction *action, const QList<QKeySequence> &shortcuts) override {
    KGlobalAccel::self()->setDefaultShortcut(action, shortcuts);
    KGlobalAccel::self()->setShortcut(action, shortcuts);
  }

  QList<QKeySequence> shortcuts(QAction *action) const override {
    return KGlobalAccel::self()->shortcut(action);
  }

  bool setShortcuts(QAction *action, const QList<QKeySequence> &shortcuts, bool autoload) override {
    return KGlobalAccel::self()->setShortcut(
        action, shortcuts, autoload ? KGlobalAccel::Autoloading : KGlobalAccel::NoAutoloading);
  }

  void watchShortcutChanges(QAction *action,
                            std::function<void(const QKeySequence &)> handler) override {
    QObject::connect(KGlobalAccel::self(), &KGlobalAccel::globalShortcutChanged, action,
                     [action, handler = std::move(handler)](QAction *changedAction,
                                                            const QKeySequence &shortcut) {
                       if (changedAction == action)
                         handler(shortcut);
                     });
  }

  void cleanShortcutComponent(const QString &component) override {
    KGlobalAccel::cleanComponent(component);
  }

  void showNotification(NotificationKind kind, const QString &title, const QString &text,
                        const QString &iconName, bool persistent) override {
    const auto event =
        kind == NotificationKind::Error ? KNotification::Error : KNotification::Notification;
    const auto flags = persistent ? KNotification::Persistent : KNotification::CloseOnTimeout;
    KNotification *notification = KNotification::event(event, title, text, iconName, flags);
    if (persistent)
      m_statusNotification = notification;
  }

  void closeStatusNotification() override {
    if (m_statusNotification)
      m_statusNotification->close();
    m_statusNotification.clear();
  }

  void revealFile(const QString &path, OpenCallback callback) override {
    const QFileInfo file(path);
    const QUrl parentUrl = QUrl::fromLocalFile(file.absolutePath());
    if (!file.exists()) {
      const bool opened = QDesktopServices::openUrl(parentUrl);
      if (callback)
        callback(opened);
      return;
    }
    const QUrl url = QUrl::fromLocalFile(path);
    QDBusInterface fileManager(QStringLiteral("org.freedesktop.FileManager1"),
                               QStringLiteral("/org/freedesktop/FileManager1"),
                               QStringLiteral("org.freedesktop.FileManager1"),
                               QDBusConnection::sessionBus());
    if (fileManager.isValid()) {
      auto *watcher =
          new QDBusPendingCallWatcher(fileManager.asyncCall(QStringLiteral("ShowItems"),
                                                            QStringList{url.toString()}, QString()),
                                      QGuiApplication::instance());
      QObject::connect(watcher, &QDBusPendingCallWatcher::finished, watcher,
                       [watcher, parentUrl, callback = std::move(callback)] {
                         const QDBusPendingReply<> reply = *watcher;
                         const bool opened =
                             !reply.isError() || QDesktopServices::openUrl(parentUrl);
                         if (callback)
                           callback(opened);
                         watcher->deleteLater();
                       });
      return;
    }
    const bool opened = QDesktopServices::openUrl(parentUrl);
    if (callback)
      callback(opened);
  }

  void openDirectory(const QString &path, OpenCallback callback) override {
    const bool opened = QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    if (callback)
      callback(opened);
  }

private:
  QPointer<KNotification> m_statusNotification;
};
} // namespace

std::unique_ptr<DesktopIntegration> createDesktopIntegration() {
  return std::make_unique<KdeDesktopIntegration>();
}

TrayPresentation trayPresentation(int stateValue, bool actionEnabled) {
  const auto state = static_cast<AppController::State>(stateValue);
  if (state == AppController::State::Recording)
    return {QStringLiteral("media-record"), i18n("Stop and Transcribe"), actionEnabled};
  if (state == AppController::State::Transcribing)
    return {QStringLiteral("view-refresh"), i18n("Transcribing…"), false};
  if (state == AppController::State::Success)
    return {QStringLiteral("dialog-information"), i18n("Start Dictation"), actionEnabled};
  return {QStringLiteral("audio-input-microphone"), i18n("Start Dictation"), actionEnabled};
}

void activateWindow(const WindowActivation &window, bool toggle) {
  if (toggle && window.isVisible()) {
    window.hide();
    return;
  }
  window.show();
  window.raise();
  window.requestActivate();
}
