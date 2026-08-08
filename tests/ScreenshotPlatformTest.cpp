// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ScreenshotPlatform.h"

#include <QTest>

class ScreenshotPlatformTest final : public QObject {
  Q_OBJECT

private slots:
  void recognizesNativeDesktopPlatforms_data() {
    QTest::addColumn<QString>("platformName");
    QTest::addColumn<bool>("expected");

    QTest::newRow("X11") << QStringLiteral("xcb") << true;
    QTest::newRow("Wayland") << QStringLiteral("wayland") << true;
    QTest::newRow("Wayland EGL") << QStringLiteral("wayland-egl") << true;
    QTest::newRow("offscreen") << QStringLiteral("offscreen") << false;
    QTest::newRow("minimal") << QStringLiteral("minimal") << false;
    QTest::newRow("minimal EGL") << QStringLiteral("minimalegl") << false;
    QTest::newRow("VNC") << QStringLiteral("vnc") << false;
    QTest::newRow("EGLFS") << QStringLiteral("eglfs") << false;
    QTest::newRow("Linux framebuffer") << QStringLiteral("linuxfb") << false;
  }

  void recognizesNativeDesktopPlatforms() {
    QFETCH(QString, platformName);
    QFETCH(bool, expected);

    QCOMPARE(isNativeDesktopPlatform(platformName), expected);
  }
};

QTEST_GUILESS_MAIN(ScreenshotPlatformTest)

#include "ScreenshotPlatformTest.moc"
