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
};

void TextOutputTest::copiesTranscriptionToAvailableClipboards() {
  const QString transcription = QStringLiteral("Kastword clipboard regression test");
  TextOutput output;

  QCOMPARE(output.deliver(transcription, false), QStringLiteral("Copied to clipboard."));
  QCOMPARE(QGuiApplication::clipboard()->text(QClipboard::Clipboard), transcription);
  if (QGuiApplication::clipboard()->supportsSelection())
    QCOMPARE(QGuiApplication::clipboard()->text(QClipboard::Selection), transcription);
}

QTEST_MAIN(TextOutputTest)
#include "TextOutputTest.moc"
