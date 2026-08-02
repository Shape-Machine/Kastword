// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

class TextOutputTest;

class TextOutput : public QObject {
  Q_OBJECT
public:
  using FocusReader = std::function<QString(const QString &)>;
  explicit TextOutput(QObject *parent = nullptr);
  TextOutput(FocusReader focusReader, QObject *parent = nullptr);
  virtual QString deliver(const QString &text, bool autoPaste);
  virtual void forget(const QString &text);

  enum class PasteMethod { ClipboardOnly, Xdotool, Ydotool };
  static PasteMethod choosePasteMethod(bool autoPaste, const QString &session,
                                       bool xdotoolAvailable, bool ydotoolAvailable);
  static QStringList x11PasteArguments();
  static QStringList waylandPasteArguments();

signals:
  void deliveryStatus(const QString &status);

private:
  friend class TextOutputTest;
  void scheduleX11Paste(const QString &xdotool);
  void startPaste(const QString &program, const QStringList &arguments, const QString &success);
  FocusReader m_focusReader;
};
