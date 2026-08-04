// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAction>
#include <QList>
#include <QString>
#include <functional>
#include <memory>

class KNotification;

class DesktopIntegration {
public:
  enum class NotificationKind { Information, Error };

  virtual ~DesktopIntegration() = default;
  virtual void configureShortcut(QAction *action, const QList<QKeySequence> &shortcuts) = 0;
  virtual QList<QKeySequence> shortcuts(QAction *action) const = 0;
  virtual bool setShortcuts(QAction *action, const QList<QKeySequence> &shortcuts,
                            bool autoload) = 0;
  virtual void cleanShortcutComponent(const QString &component) = 0;
  virtual void showNotification(NotificationKind kind, const QString &title, const QString &text,
                                const QString &iconName = {}, bool persistent = false) = 0;
  virtual void closeStatusNotification() = 0;
};

std::unique_ptr<DesktopIntegration> createDesktopIntegration();

struct TrayPresentation {
  QString iconName;
  QString actionText;
  bool actionEnabled = false;
};

TrayPresentation trayPresentation(int state, bool modelReady);

struct WindowActivation {
  std::function<bool()> isVisible;
  std::function<void()> hide;
  std::function<void()> show;
  std::function<void()> raise;
  std::function<void()> requestActivate;
};

void activateWindow(const WindowActivation &window, bool toggle);
