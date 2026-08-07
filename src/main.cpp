// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AppController.h"
#include "PlatformIntegration.h"
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
  trayMenu.addAction(&openAction);
  trayMenu.addAction(&dictateAction);
  tray.setContextMenu(&trayMenu);

  const WindowActivation windowActivation = {
      [window] { return window->isVisible(); },
      [window] { window->hide(); },
      [window] { window->show(); },
      [window] { window->raise(); },
      [window] { window->requestActivate(); },
  };
  QObject::connect(
      &tray, &KStatusNotifierItem::activateRequested, window,
      [windowActivation](bool, const QPoint &) { activateWindow(windowActivation, true); });
  QObject::connect(&dbusService, &KDBusService::activateRequested, window,
                   [windowActivation](const QStringList &, const QString &) {
                     activateWindow(windowActivation, false);
                   });
  QObject::connect(&openAction, &QAction::triggered, window,
                   [windowActivation] { activateWindow(windowActivation, true); });
  QObject::connect(&dictateAction, &QAction::triggered, &controller, &AppController::toggle);

  const auto updateTrayPresentation = [&controller, &tray, &dictateAction] {
    const TrayPresentation presentation =
        trayPresentation(int(controller.state()), controller.dictationActionEnabled());
    tray.setIconByName(presentation.iconName);
    dictateAction.setText(presentation.actionText);
    dictateAction.setEnabled(presentation.actionEnabled);
    tray.setToolTip(QStringLiteral("audio-input-microphone"), i18n("Kastword"),
                    controller.status());
  };
  updateTrayPresentation();
  QObject::connect(&controller, &AppController::stateChanged, &tray, updateTrayPresentation);
  QObject::connect(&controller, &AppController::dictationAvailabilityChanged, &tray,
                   updateTrayPresentation);
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

  if (controller.dictationActionEnabled()) {
    const QString readyText =
        controller.shortcut().isEmpty()
            ? i18n("Running in the system tray. Open Kastword or use the tray menu to dictate.")
            : i18n("Running in the system tray. Press %1 to dictate.", controller.shortcutText());
    KNotification::event(KNotification::Notification, i18n("Kastword is ready"), readyText,
                         QStringLiteral("audio-input-microphone"), KNotification::CloseOnTimeout);
  }

  return app.exec();
}
