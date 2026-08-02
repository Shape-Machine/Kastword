// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AppController.h"
#include "RuntimeSecurity.h"

#include <KDBusService>
#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <KNotification>
#include <KStatusNotifierItem>
#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <cstdio>
#include <cstring>
#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

int main(int argc, char **argv) {
  bool smokeTest = false;
  for (int i = 1; i < argc; ++i)
    smokeTest = smokeTest || std::strcmp(argv[i], "--smoke-test") == 0;
#ifdef Q_OS_UNIX
  if (!smokeTest && shouldRefuseElevatedExecution(getuid(), geteuid())) {
    std::fputs("Kastword refuses to run with elevated privileges.\n", stderr);
    return 1;
  }
#endif
  QApplication app(argc, argv);
  QApplication::setOrganizationDomain(QStringLiteral("shape_machine.github.io"));
  // Keep the internal component name identical to the executable name. KGlobalAccel uses this
  // value as its persistent identifier, while the separately configured display name remains
  // the user-facing, capitalized "Kastword".
  QApplication::setApplicationName(QStringLiteral("kastword"));
  QApplication::setApplicationDisplayName(QStringLiteral("Kastword"));
  QApplication::setQuitOnLastWindowClosed(false);
  KLocalizedString::setApplicationDomain("kastword");

  AppController controller;
  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty(QStringLiteral("appController"), &controller);
  KLocalization::setupLocalizedContext(&engine);
  engine.loadFromModule(QStringLiteral("io.github.shape_machine.Kastword"), QStringLiteral("Main"));
  if (engine.rootObjects().isEmpty())
    return 1;
  if (smokeTest)
    return 0;

  KDBusService dbusService(KDBusService::Unique);
  auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
  KStatusNotifierItem tray(QStringLiteral("kastword"));
  tray.setIconByName(QStringLiteral("audio-input-microphone"));
  tray.setTitle(QStringLiteral("Kastword"));
  tray.setToolTip(QStringLiteral("audio-input-microphone"), QStringLiteral("Kastword"),
                  QStringLiteral("Offline dictation — %1").arg(controller.shortcutText()));

  QMenu trayMenu;
  QAction openAction(QStringLiteral("Open Kastword"), &trayMenu);
  QAction dictateAction(QStringLiteral("Start Dictation"), &trayMenu);
  trayMenu.addAction(&openAction);
  trayMenu.addAction(&dictateAction);
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

  QObject::connect(&controller, &AppController::stateChanged, &tray,
                   [&controller, &tray, &dictateAction] {
                     const AppController::State state = controller.state();
                     if (state == AppController::State::Recording) {
                       tray.setIconByName(QStringLiteral("media-record"));
                       dictateAction.setText(QStringLiteral("Stop and Transcribe"));
                     } else if (state == AppController::State::Transcribing) {
                       tray.setIconByName(QStringLiteral("view-refresh"));
                       dictateAction.setText(QStringLiteral("Transcribing…"));
                     } else if (state == AppController::State::Success) {
                       tray.setIconByName(QStringLiteral("dialog-ok-apply"));
                       dictateAction.setText(QStringLiteral("Start Dictation"));
                     } else {
                       tray.setIconByName(QStringLiteral("audio-input-microphone"));
                       dictateAction.setText(QStringLiteral("Start Dictation"));
                     }
                     dictateAction.setEnabled(state != AppController::State::Transcribing);
                     tray.setToolTip(QStringLiteral("audio-input-microphone"),
                                     QStringLiteral("Kastword"), controller.status());
                   });

  KNotification::event(KNotification::Notification, QStringLiteral("Kastword is ready"),
                       QStringLiteral("Running in the system tray. Press %1 to dictate.")
                           .arg(controller.shortcutText()),
                       QStringLiteral("audio-input-microphone"), KNotification::CloseOnTimeout);

  return app.exec();
}
