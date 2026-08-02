// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TextOutput.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QSignalSpy>
#include <QTest>

Q_DECLARE_METATYPE(TextOutput::PasteMethod)

class TextOutputTest final : public QObject {
  Q_OBJECT

private slots:
  void copiesTranscriptionToAvailableClipboards();
  void forgetsOnlyMatchingClipboardText();
  void reportsPasteHelperFailures();
  void cancelsX11PasteWhenFocusChanges();
  void pastesOnX11WhenFocusIsUnchanged();
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

void TextOutputTest::forgetsOnlyMatchingClipboardText() {
  TextOutput output;
  QClipboard *clipboard = QGuiApplication::clipboard();
  const QString transcription = QStringLiteral("private transcription");
  output.deliver(transcription, false);

  clipboard->setText(QStringLiteral("new clipboard text"), QClipboard::Clipboard);
  output.forget(transcription);
  QCOMPARE(clipboard->text(QClipboard::Clipboard), QStringLiteral("new clipboard text"));

  clipboard->setText(transcription, QClipboard::Clipboard);
  output.forget(transcription);
  QVERIFY(clipboard->text(QClipboard::Clipboard).isEmpty());
}

void TextOutputTest::reportsPasteHelperFailures() {
  TextOutput output;
  QSignalSpy status(&output, &TextOutput::deliveryStatus);

  output.startPaste(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), QStringLiteral("exit 7")},
                    QStringLiteral("unexpected success"));
  QTRY_COMPARE(status.count(), 1);
  QCOMPARE(status.takeFirst().at(0).toString(),
           QStringLiteral("Automatic paste helper failed with exit code 7."));

  output.startPaste(QStringLiteral("/missing/kastword-paste-helper"), {},
                    QStringLiteral("unexpected success"));
  QTRY_COMPARE(status.count(), 1);
  QCOMPARE(status.takeFirst().at(0).toString(),
           QStringLiteral("Automatic paste helper could not be started."));
}

void TextOutputTest::cancelsX11PasteWhenFocusChanges() {
  int reads = 0;
  TextOutput output([&reads](const QString &) {
    ++reads;
    return QString::number(reads);
  });
  QSignalSpy status(&output, &TextOutput::deliveryStatus);

  output.scheduleX11Paste(QStringLiteral("/bin/true"));

  QTRY_COMPARE(status.count(), 1);
  QCOMPARE(status.takeFirst().at(0).toString(),
           QStringLiteral("Automatic paste was cancelled because focus changed."));
  QCOMPARE(reads, 2);
}

void TextOutputTest::pastesOnX11WhenFocusIsUnchanged() {
  int reads = 0;
  TextOutput output([&reads](const QString &) {
    ++reads;
    return QStringLiteral("42");
  });
  QSignalSpy status(&output, &TextOutput::deliveryStatus);

  output.scheduleX11Paste(QStringLiteral("/bin/true"));

  QTRY_COMPARE(status.count(), 1);
  QCOMPARE(status.takeFirst().at(0).toString(),
           QStringLiteral("Pasted into the focused application."));
  QCOMPARE(reads, 2);
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
