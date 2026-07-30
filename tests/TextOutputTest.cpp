// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TextOutput.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QTest>

class TextOutputTest final : public QObject {
  Q_OBJECT

private slots:
  void copiesTranscriptionToAvailableClipboards();
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
