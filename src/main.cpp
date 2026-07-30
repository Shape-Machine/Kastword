#include "AppController.h"

#include <KLocalizedContext>
#include <KLocalizedString>
#include <KStatusNotifierItem>
#include <QApplication>
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
    QObject::connect(&tray, &KStatusNotifierItem::activateRequested, window,
                     [window](bool, const QPoint &) { window->show(); window->raise(); window->requestActivate(); });

    return app.exec();
}
