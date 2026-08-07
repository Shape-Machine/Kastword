// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QFlags>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <functional>
#include <memory>

class TextOutputTest;

class TextOutput : public QObject {
  Q_OBJECT
public:
  using FocusReader = std::function<QString(const QString &)>;
  enum class HelperResult { Success, FailedToStart, Crashed, ProcessError, Failed };
  class Platform {
  public:
    using HelperFinished = std::function<void(HelperResult, int)>;
    virtual ~Platform() = default;
    virtual void setClipboardText(const QString &text, bool selection) = 0;
    virtual QString clipboardText(bool selection) const = 0;
    virtual void clearClipboard(bool selection) = 0;
    virtual bool supportsSelection() const = 0;
    virtual void setKlipperText(const QString &text) = 0;
    virtual QString klipperText(bool *available) const = 0;
    virtual QString sessionType() const = 0;
    virtual QString findExecutable(const QString &name) const = 0;
    virtual QString focusedWindow(const QString &helper) const = 0;
    virtual void launchHelper(const QString &program, const QStringList &arguments,
                              HelperFinished finished) = 0;
  };

  explicit TextOutput(QObject *parent = nullptr);
  TextOutput(FocusReader focusReader, QObject *parent = nullptr);
  TextOutput(std::unique_ptr<Platform> platform, QObject *parent = nullptr);
  ~TextOutput() override;
  virtual QString deliver(const QString &text, bool autoPaste);
  virtual void forget(const QString &text);

  enum PasteShortcut {
    CtrlV = 0x1,
    CtrlShiftV = 0x2,
    ShiftInsert = 0x4,
  };
  Q_DECLARE_FLAGS(PasteShortcuts, PasteShortcut)
  Q_FLAG(PasteShortcuts)
  virtual void setPasteShortcuts(PasteShortcuts shortcuts);
  PasteShortcuts pasteShortcuts() const { return m_pasteShortcuts; }

  enum class PasteMethod { ClipboardOnly, Xdotool, Ydotool };
  static PasteMethod choosePasteMethod(bool autoPaste, const QString &session,
                                       bool xdotoolAvailable, bool ydotoolAvailable);
  static QStringList x11PasteArguments(PasteShortcuts shortcuts);
  static QStringList waylandPasteArguments(PasteShortcuts shortcuts);
  static HelperResult helperResultForProcessError(QProcess::ProcessError error);

signals:
  void deliveryStatus(const QString &status);
  void deliveryFailed(const QString &status);

private:
  friend class TextOutputTest;
  void reportDeliveryFailure(const QString &status);
  void scheduleX11Paste(const QString &xdotool, const QStringList &arguments);
  void startPaste(const QString &program, const QStringList &arguments, const QString &success);
  std::unique_ptr<Platform> m_platform;
  PasteShortcuts m_pasteShortcuts = CtrlV | CtrlShiftV | ShiftInsert;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(TextOutput::PasteShortcuts)
