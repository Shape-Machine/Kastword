// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TextOutput.h"

#include <QClipboard>
#include <QDBusInterface>
#include <QGuiApplication>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

TextOutput::TextOutput(QObject *parent) : QObject(parent) {}

QStringList TextOutput::waylandPasteArguments() {
  // KEY_LEFTCTRL=29, KEY_LEFTSHIFT=42, and KEY_V=47.
  return {QStringLiteral("key"),  QStringLiteral("29:1"), QStringLiteral("42:1"),
          QStringLiteral("47:1"), QStringLiteral("47:0"), QStringLiteral("42:0"),
          QStringLiteral("29:0")};
}

QString TextOutput::deliver(const QString &text, bool autoPaste) {
  QClipboard *clipboard = QGuiApplication::clipboard();
  clipboard->setText(text, QClipboard::Clipboard);
  // Terminal emulators commonly map Shift+Insert to the primary selection,
  // while regular applications map it to the clipboard. Keep both buffers in
  // sync so the cross-application paste shortcut cannot insert stale text.
  if (clipboard->supportsSelection())
    clipboard->setText(text, QClipboard::Selection);

  // Plasma's clipboard manager can briefly retain ownership of its previous history entry after
  // a Wayland client updates QClipboard. Hand the same text to Klipper synchronously so terminal
  // paste actions cannot observe that stale entry. This is a no-op outside a Plasma session.
  QDBusInterface klipper(QStringLiteral("org.kde.klipper"), QStringLiteral("/klipper"),
                         QStringLiteral("org.kde.klipper.klipper"));
  if (klipper.isValid())
    klipper.call(QStringLiteral("setClipboardContents"), text);

  if (!autoPaste)
    return tr("Copied to clipboard.");

  const QString session = qEnvironmentVariable("XDG_SESSION_TYPE").toLower();
  if (session == QStringLiteral("x11")) {
    const QString tool = QStandardPaths::findExecutable(QStringLiteral("xdotool"));
    if (!tool.isEmpty()) {
      // Defer the synthetic key press until Qt has advertised the new clipboard owner to the
      // window system. Pasting in this same event-loop turn can read the previous clipboard.
      QTimer::singleShot(150, this, [tool] {
        QProcess::startDetached(tool, {QStringLiteral("key"), QStringLiteral("--clearmodifiers"),
                                       QStringLiteral("shift+Insert")});
      });
      return tr("Pasted into the focused application.");
    }
  } else {
    const QString tool = QStandardPaths::findExecutable(QStringLiteral("ydotool"));
    if (!tool.isEmpty()) {
      // Konsole's Ctrl+Shift+V action reads the regular clipboard; Shift+Insert can instead read a
      // stale primary selection on Wayland.
      // Wayland clipboard ownership is asynchronous, so allow the compositor to receive the new
      // contents before asking the focused application to paste them.
      QTimer::singleShot(150, this,
                         [tool] { QProcess::startDetached(tool, waylandPasteArguments()); });
      return tr("Sent paste to the focused application.");
    }
  }
  return tr("Copied to clipboard; install %1 for automatic paste.")
      .arg(session == QStringLiteral("x11") ? QStringLiteral("xdotool")
                                            : QStringLiteral("ydotool"));
}
