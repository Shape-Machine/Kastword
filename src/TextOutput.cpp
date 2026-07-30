// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TextOutput.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QProcess>
#include <QStandardPaths>

TextOutput::TextOutput(QObject *parent) : QObject(parent) {}

QString TextOutput::deliver(const QString &text, bool autoPaste) {
  QClipboard *clipboard = QGuiApplication::clipboard();
  clipboard->setText(text, QClipboard::Clipboard);
  // Terminal emulators commonly map Shift+Insert to the primary selection,
  // while regular applications map it to the clipboard. Keep both buffers in
  // sync so the cross-application paste shortcut cannot insert stale text.
  if (clipboard->supportsSelection())
    clipboard->setText(text, QClipboard::Selection);
  if (!autoPaste)
    return tr("Copied to clipboard.");

  const QString session = qEnvironmentVariable("XDG_SESSION_TYPE").toLower();
  if (session == QStringLiteral("x11")) {
    const QString tool = QStandardPaths::findExecutable(QStringLiteral("xdotool"));
    if (!tool.isEmpty()) {
      QProcess::startDetached(tool, {QStringLiteral("key"), QStringLiteral("--clearmodifiers"),
                                     QStringLiteral("shift+Insert")});
      return tr("Pasted into the focused application.");
    }
  } else {
    const QString tool = QStandardPaths::findExecutable(QStringLiteral("ydotool"));
    if (!tool.isEmpty()) {
      // KEY_LEFTSHIFT=42 and KEY_INSERT=110. Shift+Insert is understood as
      // paste by both terminal emulators and regular KDE text controls.
      QProcess::startDetached(tool, {QStringLiteral("key"), QStringLiteral("42:1"),
                                     QStringLiteral("110:1"), QStringLiteral("110:0"),
                                     QStringLiteral("42:0")});
      return tr("Sent paste to the focused application.");
    }
  }
  return tr("Copied to clipboard; install %1 for automatic paste.")
      .arg(session == QStringLiteral("x11") ? QStringLiteral("xdotool")
                                            : QStringLiteral("ydotool"));
}
