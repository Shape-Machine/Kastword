#include "AppController.h"

#include <KLocalizedContext>
#include <KLocalizedString>
#include <KStatusNotifierItem>
#include <QApplication>
#include <QAction>
#include <QMenu>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QApplication::setOrganizationDomain(QStringLiteral("kde.org"));
    QApplication::setApplicationName(QStringLiteral("kastword"));
    QApplication::setApplicationDisplayName(QStringLiteral("Kastword"));
    QApplication::setQuitOnLastWindowClosed(false);
    KLocalizedString::setApplicationDomain("kastword");

    AppController controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &controller);
    engine.rootContext()->setContextObject(new KLocalizedContext(&engine));
    engine.loadFromModule(QStringLiteral("org.kde.kastword"), QStringLiteral("Main"));
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
    QObject::connect(&openAction, &QAction::triggered, window, toggleWindow);
    QObject::connect(&dictateAction, &QAction::triggered, &controller, &AppController::toggle);
    QObject::connect(&quitAction, &QAction::triggered, &app, &QApplication::quit);

    QObject::connect(&controller, &AppController::stateChanged, &tray, [&controller, &tray, &dictateAction] {
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
        tray.setToolTip(QStringLiteral("audio-input-microphone"), QStringLiteral("Kastword"),
                        controller.status());
    });

    return app.exec();
}
