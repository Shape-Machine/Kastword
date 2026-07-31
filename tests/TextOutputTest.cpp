// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TextOutput.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QTest>

Q_DECLARE_METATYPE(TextOutput::PasteMethod)

class TextOutputTest final : public QObject {
  Q_OBJECT

private slots:
  void copiesTranscriptionToAvailableClipboards();
  void choosesPasteMethod_data();
  void choosesPasteMethod();
  void usesExpectedX11Arguments();
  void usesRegularClipboardShortcutOnWayland();
};

void TextOutputTest::copiesTranscriptionToAvailableClipboards() {
  const QString transcription = QStringLiteral("Kastword clipboard regression test");
  TextOutput output;

  QCOMPARE(output.deliver(transcription, false), QStringLiteral("Copied to clipboard."));
  QCOMPARE(QGuiApplication::clipboard()->text(QClipboard::Clipboard), transcription);
  if (QGuiApplication::clipboard()->supportsSelection())
    QCOMPARE(QGuiApplication::clipboard()->text(QClipboard::Selection), transcription);
}

void TextOutputTest::choosesPasteMethod_data() {
  QTest::addColumn<bool>("autoPaste");
  QTest::addColumn<QString>("session");
  QTest::addColumn<bool>("xdotoolAvailable");
  QTest::addColumn<bool>("ydotoolAvailable");
  QTest::addColumn<TextOutput::PasteMethod>("expected");

  using Method = TextOutput::PasteMethod;
  QTest::newRow("disabled") << false << QStringLiteral("wayland") << true << true
                            << Method::ClipboardOnly;
  QTest::newRow("x11 helper") << true << QStringLiteral("x11") << true << false << Method::Xdotool;
  QTest::newRow("x11 missing") << true << QStringLiteral("X11") << false << true
                               << Method::ClipboardOnly;
  QTest::newRow("wayland helper") << true << QStringLiteral("wayland") << false << true
                                  << Method::Ydotool;
  QTest::newRow("wayland missing")
      << true << QStringLiteral("wayland") << true << false << Method::ClipboardOnly;
  QTest::newRow("unknown session")
      << true << QStringLiteral("unknown") << false << true << Method::Ydotool;
}

void TextOutputTest::choosesPasteMethod() {
  QFETCH(bool, autoPaste);
  QFETCH(QString, session);
  QFETCH(bool, xdotoolAvailable);
  QFETCH(bool, ydotoolAvailable);
  QFETCH(TextOutput::PasteMethod, expected);

  QCOMPARE(TextOutput::choosePasteMethod(autoPaste, session, xdotoolAvailable, ydotoolAvailable),
           expected);
}

void TextOutputTest::usesExpectedX11Arguments() {
  QCOMPARE(TextOutput::x11PasteArguments(),
           QStringList({QStringLiteral("key"), QStringLiteral("--clearmodifiers"),
                        QStringLiteral("shift+Insert")}));
}

void TextOutputTest::usesRegularClipboardShortcutOnWayland() {
  const QStringList arguments = TextOutput::waylandPasteArguments();

  // Ctrl+Shift+V makes Konsole read the regular clipboard updated by Kastword and Klipper.
  QCOMPARE(arguments,
           QStringList({QStringLiteral("key"), QStringLiteral("29:1"), QStringLiteral("42:1"),
                        QStringLiteral("47:1"), QStringLiteral("47:0"), QStringLiteral("42:0"),
                        QStringLiteral("29:0")}));
  // KEY_INSERT=110 would make Konsole read the primary selection, which may contain stale text.
  QVERIFY(!arguments.contains(QStringLiteral("110:1")));
}

QTEST_MAIN(TextOutputTest)
#include "TextOutputTest.moc"
