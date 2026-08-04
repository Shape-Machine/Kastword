// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TextOutput.h"

#include <KLocalizedString>
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

class QtTextOutputPlatform final : public TextOutput::Platform {
public:
  QtTextOutputPlatform(QObject *owner, TextOutput::FocusReader focusReader = x11FocusedWindow)
      : m_owner(owner), m_focusReader(std::move(focusReader)) {}

  void setClipboardText(const QString &text, bool selection) override {
    QGuiApplication::clipboard()->setText(text, selection ? QClipboard::Selection
                                                          : QClipboard::Clipboard);
  }
  QString clipboardText(bool selection) const override {
    return QGuiApplication::clipboard()->text(selection ? QClipboard::Selection
                                                        : QClipboard::Clipboard);
  }
  void clearClipboard(bool selection) override {
    QGuiApplication::clipboard()->clear(selection ? QClipboard::Selection : QClipboard::Clipboard);
  }
  bool supportsSelection() const override {
    return QGuiApplication::clipboard()->supportsSelection();
  }
  void setKlipperText(const QString &text) override {
    QDBusInterface klipper(QStringLiteral("org.kde.klipper"), QStringLiteral("/klipper"),
                           QStringLiteral("org.kde.klipper.klipper"));
    if (klipper.isValid())
      klipper.call(QStringLiteral("setClipboardContents"), text);
  }
  QString klipperText(bool *available) const override {
    QDBusInterface klipper(QStringLiteral("org.kde.klipper"), QStringLiteral("/klipper"),
                           QStringLiteral("org.kde.klipper.klipper"));
    if (!klipper.isValid()) {
      *available = false;
      return {};
    }
    const QDBusReply<QString> current = klipper.call(QStringLiteral("getClipboardContents"));
    *available = current.isValid();
    return current.isValid() ? current.value() : QString();
  }
  QString sessionType() const override { return qEnvironmentVariable("XDG_SESSION_TYPE"); }
  QString findExecutable(const QString &name) const override {
    return QStandardPaths::findExecutable(name);
  }
  QString focusedWindow(const QString &helper) const override { return m_focusReader(helper); }
  void launchHelper(const QString &program, const QStringList &arguments,
                    HelperFinished finished) override {
    auto *process = new QProcess(m_owner);
    auto completed = std::make_shared<bool>(false);
    QObject::connect(process, &QProcess::errorOccurred, m_owner,
                     [process, completed, finished](QProcess::ProcessError error) {
                       if (*completed)
                         return;
                       *completed = true;
                       finished(error == QProcess::FailedToStart
                                    ? TextOutput::HelperResult::FailedToStart
                                    : TextOutput::HelperResult::Crashed,
                                -1);
                       process->deleteLater();
                     });
    QObject::connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), m_owner,
                     [process, completed, finished](int exitCode, QProcess::ExitStatus status) {
                       if (*completed)
                         return;
                       *completed = true;
                       finished(status == QProcess::CrashExit
                                    ? TextOutput::HelperResult::Crashed
                                    : (exitCode == 0 ? TextOutput::HelperResult::Success
                                                     : TextOutput::HelperResult::Failed),
                                exitCode);
                       process->deleteLater();
                     });
    process->start(program, arguments);
  }

private:
  QObject *m_owner;
  TextOutput::FocusReader m_focusReader;
};
} // namespace

TextOutput::TextOutput(QObject *parent)
    : QObject(parent), m_platform(std::make_unique<QtTextOutputPlatform>(this)) {}

TextOutput::TextOutput(FocusReader focusReader, QObject *parent)
    : QObject(parent),
      m_platform(std::make_unique<QtTextOutputPlatform>(this, std::move(focusReader))) {}

TextOutput::TextOutput(std::unique_ptr<Platform> platform, QObject *parent)
    : QObject(parent), m_platform(std::move(platform)) {}

TextOutput::~TextOutput() = default;

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
          QStringLiteral("29:0"), QStringLiteral("29:1"), QStringLiteral("47:1"),
          QStringLiteral("47:0"), QStringLiteral("29:0")};
}

QString TextOutput::deliver(const QString &text, bool autoPaste) {
  m_platform->setClipboardText(text, false);
  // Terminal emulators commonly map Shift+Insert to the primary selection,
  // while regular applications map it to the clipboard. Keep both buffers in
  // sync so the cross-application paste shortcut cannot insert stale text.
  if (m_platform->supportsSelection())
    m_platform->setClipboardText(text, true);

  // Plasma's clipboard manager can briefly retain ownership of its previous history entry after
  // a Wayland client updates QClipboard. Hand the same text to Klipper synchronously so terminal
  // paste actions cannot observe that stale entry. This is a no-op outside a Plasma session.
  m_platform->setKlipperText(text);

  const QString session = m_platform->sessionType().toLower();
  const QString xdotool = m_platform->findExecutable(QStringLiteral("xdotool"));
  const QString ydotool = m_platform->findExecutable(QStringLiteral("ydotool"));
  const PasteMethod method =
      choosePasteMethod(autoPaste, session, !xdotool.isEmpty(), !ydotool.isEmpty());
  if (method == PasteMethod::ClipboardOnly && !autoPaste)
    return i18n("Copied to clipboard.");

  if (method == PasteMethod::Xdotool) {
    // Defer the synthetic key press until Qt has advertised the new clipboard owner to the window
    // system. Pasting in this same event-loop turn can read the previous clipboard.
    scheduleX11Paste(xdotool);
    return i18n("Copied to clipboard; automatic paste scheduled.");
  }
  if (method == PasteMethod::Ydotool) {
    // Konsole's Ctrl+Shift+V action reads the regular clipboard; Shift+Insert can instead read a
    // stale primary selection on Wayland. Allow the compositor to receive the new clipboard first.
    QTimer::singleShot(150, this, [this, ydotool] {
      startPaste(ydotool, waylandPasteArguments(), i18n("Sent paste to the focused application."));
    });
    return i18n("Copied to clipboard; automatic paste scheduled.");
  }
  return i18n("Copied to clipboard; install %1 for automatic paste.",
              session == QStringLiteral("x11") ? QStringLiteral("xdotool")
                                               : QStringLiteral("ydotool"));
}

void TextOutput::scheduleX11Paste(const QString &xdotool) {
  const QString originalWindow = m_platform->focusedWindow(xdotool);
  QTimer::singleShot(150, this, [this, xdotool, originalWindow] {
    if (originalWindow.isEmpty() || m_platform->focusedWindow(xdotool) != originalWindow) {
      emit deliveryStatus(i18n("Automatic paste was cancelled because focus changed."));
      return;
    }
    startPaste(xdotool, x11PasteArguments(), i18n("Pasted into the focused application."));
  });
}

void TextOutput::startPaste(const QString &program, const QStringList &arguments,
                            const QString &success) {
  m_platform->launchHelper(program, arguments, [this, success](HelperResult result, int exitCode) {
    if (result == HelperResult::Success)
      emit deliveryStatus(success);
    else if (result == HelperResult::FailedToStart)
      emit deliveryStatus(i18n("Automatic paste helper could not be started."));
    else if (result == HelperResult::Crashed)
      emit deliveryStatus(i18n("Automatic paste helper crashed."));
    else
      emit deliveryStatus(i18n("Automatic paste helper failed with exit code %1.", exitCode));
  });
}

void TextOutput::forget(const QString &text) {
  if (text.isEmpty())
    return;
  if (m_platform->clipboardText(false) == text)
    m_platform->clearClipboard(false);
  if (m_platform->supportsSelection() && m_platform->clipboardText(true) == text)
    m_platform->clearClipboard(true);

  bool klipperAvailable = false;
  const QString current = m_platform->klipperText(&klipperAvailable);
  if (klipperAvailable && current == text)
    m_platform->setKlipperText({});
}
