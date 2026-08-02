// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TextOutput.h"

#include <QClipboard>
#include <QDBusInterface>
#include <QDBusReply>
#include <QGuiApplication>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <utility>

namespace {
QString x11FocusedWindow(const QString &xdotool) {
  QProcess process;
  process.start(xdotool, {QStringLiteral("getwindowfocus")});
  if (!process.waitForFinished(1000) || process.exitStatus() != QProcess::NormalExit ||
      process.exitCode() != 0)
    return {};
  return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}
} // namespace

TextOutput::TextOutput(QObject *parent) : TextOutput(x11FocusedWindow, parent) {}

TextOutput::TextOutput(FocusReader focusReader, QObject *parent)
    : QObject(parent), m_focusReader(std::move(focusReader)) {}

TextOutput::PasteMethod TextOutput::choosePasteMethod(bool autoPaste, const QString &session,
                                                      bool xdotoolAvailable,
                                                      bool ydotoolAvailable) {
  if (!autoPaste)
    return PasteMethod::ClipboardOnly;
  if (session.compare(QStringLiteral("x11"), Qt::CaseInsensitive) == 0)
    return xdotoolAvailable ? PasteMethod::Xdotool : PasteMethod::ClipboardOnly;
  return ydotoolAvailable ? PasteMethod::Ydotool : PasteMethod::ClipboardOnly;
}

QStringList TextOutput::x11PasteArguments() {
  return {QStringLiteral("key"), QStringLiteral("--clearmodifiers"),
          QStringLiteral("shift+Insert")};
}

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

  const QString session = qEnvironmentVariable("XDG_SESSION_TYPE").toLower();
  const QString xdotool = QStandardPaths::findExecutable(QStringLiteral("xdotool"));
  const QString ydotool = QStandardPaths::findExecutable(QStringLiteral("ydotool"));
  const PasteMethod method =
      choosePasteMethod(autoPaste, session, !xdotool.isEmpty(), !ydotool.isEmpty());
  if (method == PasteMethod::ClipboardOnly && !autoPaste)
    return tr("Copied to clipboard.");

  if (method == PasteMethod::Xdotool) {
    // Defer the synthetic key press until Qt has advertised the new clipboard owner to the window
    // system. Pasting in this same event-loop turn can read the previous clipboard.
    scheduleX11Paste(xdotool);
    return tr("Copied to clipboard; automatic paste scheduled.");
  }
  if (method == PasteMethod::Ydotool) {
    // Konsole's Ctrl+Shift+V action reads the regular clipboard; Shift+Insert can instead read a
    // stale primary selection on Wayland. Allow the compositor to receive the new clipboard first.
    QTimer::singleShot(150, this, [this, ydotool] {
      startPaste(ydotool, waylandPasteArguments(), tr("Sent paste to the focused application."));
    });
    return tr("Copied to clipboard; automatic paste scheduled.");
  }
  return tr("Copied to clipboard; install %1 for automatic paste.")
      .arg(session == QStringLiteral("x11") ? QStringLiteral("xdotool")
                                            : QStringLiteral("ydotool"));
}

void TextOutput::scheduleX11Paste(const QString &xdotool) {
  const QString originalWindow = m_focusReader(xdotool);
  QTimer::singleShot(150, this, [this, xdotool, originalWindow] {
    if (originalWindow.isEmpty() || m_focusReader(xdotool) != originalWindow) {
      emit deliveryStatus(tr("Automatic paste was cancelled because focus changed."));
      return;
    }
    startPaste(xdotool, x11PasteArguments(), tr("Pasted into the focused application."));
  });
}

void TextOutput::startPaste(const QString &program, const QStringList &arguments,
                            const QString &success) {
  auto *process = new QProcess(this);
  connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
      emit deliveryStatus(tr("Automatic paste helper could not be started."));
      process->deleteLater();
    } else if (error == QProcess::Crashed) {
      emit deliveryStatus(tr("Automatic paste helper crashed."));
      process->deleteLater();
    }
  });
  connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
          [this, process, success](int exitCode, QProcess::ExitStatus exitStatus) {
            if (exitStatus == QProcess::NormalExit && exitCode == 0)
              emit deliveryStatus(success);
            else if (exitStatus == QProcess::NormalExit)
              emit deliveryStatus(
                  tr("Automatic paste helper failed with exit code %1.").arg(exitCode));
            process->deleteLater();
          });
  process->start(program, arguments);
}

void TextOutput::forget(const QString &text) {
  if (text.isEmpty())
    return;
  QClipboard *clipboard = QGuiApplication::clipboard();
  if (clipboard->text(QClipboard::Clipboard) == text)
    clipboard->clear(QClipboard::Clipboard);
  if (clipboard->supportsSelection() && clipboard->text(QClipboard::Selection) == text)
    clipboard->clear(QClipboard::Selection);

  QDBusInterface klipper(QStringLiteral("org.kde.klipper"), QStringLiteral("/klipper"),
                         QStringLiteral("org.kde.klipper.klipper"));
  if (klipper.isValid()) {
    const QDBusReply<QString> current = klipper.call(QStringLiteral("getClipboardContents"));
    if (current.isValid() && current.value() == text)
      klipper.call(QStringLiteral("setClipboardContents"), QString());
  }
}
