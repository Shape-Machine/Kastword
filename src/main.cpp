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
  bool showWindow = false;
  for (int i = 1; i < argc; ++i) {
    smokeTest = smokeTest || std::strcmp(argv[i], "--smoke-test") == 0;
    showWindow = showWindow || std::strcmp(argv[i], "--show-window") == 0;
  }
#ifdef Q_OS_UNIX
  if (!smokeTest && shouldRefuseElevatedExecution(getuid(), geteuid())) {
    std::fputs("Kastword refuses to run with elevated privileges.\n", stderr);
    return 1;
  }
#endif
  QApplication app(argc, argv);
  KLocalizedString::setApplicationDomain("kastword");
  QApplication::setOrganizationDomain(QStringLiteral("shape_machine.github.io"));
  // Keep the internal component name identical to the executable name. KGlobalAccel uses this
  // value as its persistent identifier, while the separately configured display name remains
  // the user-facing, capitalized "Kastword".
  QApplication::setApplicationName(QStringLiteral("kastword"));
  QApplication::setApplicationDisplayName(i18n("Kastword"));
  QApplication::setQuitOnLastWindowClosed(false);

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
  tray.setTitle(i18n("Kastword"));
  tray.setToolTip(QStringLiteral("audio-input-microphone"), i18n("Kastword"), controller.status());

  QMenu trayMenu;
  QAction openAction(i18n("Open Kastword"), &trayMenu);
  QAction dictateAction(i18n("Start Dictation"), &trayMenu);
  dictateAction.setEnabled(controller.modelReady());
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
                       dictateAction.setText(i18n("Stop and Transcribe"));
                     } else if (state == AppController::State::Transcribing) {
                       tray.setIconByName(QStringLiteral("view-refresh"));
                       dictateAction.setText(i18n("Transcribing…"));
                     } else if (state == AppController::State::Success) {
                       tray.setIconByName(QStringLiteral("dialog-ok-apply"));
                       dictateAction.setText(i18n("Start Dictation"));
                     } else {
                       tray.setIconByName(QStringLiteral("audio-input-microphone"));
                       dictateAction.setText(i18n("Start Dictation"));
                     }
                     dictateAction.setEnabled(controller.modelReady() &&
                                              state != AppController::State::Transcribing);
                     tray.setToolTip(QStringLiteral("audio-input-microphone"), i18n("Kastword"),
                                     controller.status());
                   });
  QObject::connect(&controller, &AppController::modelReadyChanged, &dictateAction,
                   [&controller, &dictateAction] {
                     dictateAction.setEnabled(controller.modelReady() &&
                                              !controller.isTranscribing());
                   });
  QObject::connect(&controller, &AppController::statusChanged, &tray, [&controller, &tray] {
    tray.setToolTip(QStringLiteral("audio-input-microphone"), i18n("Kastword"),
                    controller.status());
  });
  QObject::connect(&controller, &AppController::modelSetupRequested, window, [window] {
    window->show();
    window->raise();
    window->requestActivate();
  });

  if (showWindow) {
    window->show();
    window->raise();
    window->requestActivate();
  }

  if (controller.modelReady())
    KNotification::event(
        KNotification::Notification, i18n("Kastword is ready"),
        i18n("Running in the system tray. Press %1 to dictate.", controller.shortcutText()),
        QStringLiteral("audio-input-microphone"), KNotification::CloseOnTimeout);

  return app.exec();
}
