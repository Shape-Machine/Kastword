// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AppController.h"

#include <KDBusService>
#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <KStatusNotifierItem>
#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  QApplication::setOrganizationDomain(QStringLiteral("shape_machine.github.io"));
  // Keep the internal component name identical to the executable name. KGlobalAccel uses this
  // value as its persistent identifier, while the separately configured display name remains
  // the user-facing, capitalized "Kastword".
  QApplication::setApplicationName(QStringLiteral("kastword"));
  QApplication::setApplicationDisplayName(QStringLiteral("Kastword"));
  QApplication::setQuitOnLastWindowClosed(false);
  KLocalizedString::setApplicationDomain("kastword");
  KDBusService dbusService(KDBusService::Unique);

  AppController controller;
  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty(QStringLiteral("appController"), &controller);
  KLocalization::setupLocalizedContext(&engine);
  engine.loadFromModule(QStringLiteral("io.github.shape_machine.Kastword"), QStringLiteral("Main"));
  if (engine.rootObjects().isEmpty())
    return 1;

  auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
  KStatusNotifierItem tray(QStringLiteral("kastword"));
  tray.setIconByName(QStringLiteral("audio-input-microphone"));
  tray.setTitle(QStringLiteral("Kastword"));
  tray.setToolTip(QStringLiteral("audio-input-microphone"), QStringLiteral("Kastword"),
                  QStringLiteral("Offline dictation — Meta+Shift+D"));

  QMenu trayMenu;
  QAction openAction(QStringLiteral("Open Kastword"), &trayMenu);
  QAction dictateAction(QStringLiteral("Start Dictation"), &trayMenu);
  QAction quitAction(QStringLiteral("Quit"), &trayMenu);
  trayMenu.addAction(&openAction);
  trayMenu.addAction(&dictateAction);
  trayMenu.addSeparator();
  trayMenu.addAction(&quitAction);
  tray.setContextMenu(&trayMenu);

  const auto toggleWindow = [window] {
    if (window->isVisible()) {
      window->hide();
    } else {
      window->show();
      window->raise();
      window->requestActivate();
    }
  };
  QObject::connect(&tray, &KStatusNotifierItem::activateRequested, window,
                   [toggleWindow](bool, const QPoint &) { toggleWindow(); });
  QObject::connect(&dbusService, &KDBusService::activateRequested, window,
                   [window](const QStringList &, const QString &) {
                     window->show();
                     window->raise();
                     window->requestActivate();
                   });
  QObject::connect(&openAction, &QAction::triggered, window, toggleWindow);
  QObject::connect(&dictateAction, &QAction::triggered, &controller, &AppController::toggle);
  QObject::connect(&quitAction, &QAction::triggered, &app, &QApplication::quit);

  QObject::connect(&controller, &AppController::stateChanged, &tray,
                   [&controller, &tray, &dictateAction] {
                     const QString state = controller.state();
                     if (state == QStringLiteral("recording")) {
                       tray.setIconByName(QStringLiteral("media-record"));
                       dictateAction.setText(QStringLiteral("Stop and Transcribe"));
                     } else if (state == QStringLiteral("transcribing")) {
                       tray.setIconByName(QStringLiteral("view-refresh"));
                       dictateAction.setText(QStringLiteral("Transcribing…"));
                     } else if (state == QStringLiteral("success")) {
                       tray.setIconByName(QStringLiteral("dialog-ok-apply"));
                       dictateAction.setText(QStringLiteral("Start Dictation"));
                     } else {
                       tray.setIconByName(QStringLiteral("audio-input-microphone"));
                       dictateAction.setText(QStringLiteral("Start Dictation"));
                     }
                     dictateAction.setEnabled(state != QStringLiteral("transcribing"));
                     tray.setToolTip(QStringLiteral("audio-input-microphone"),
                                     QStringLiteral("Kastword"), controller.status());
                   });

  return app.exec();
}
