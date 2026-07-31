// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class TextOutputTest;

class TextOutput : public QObject {
  Q_OBJECT
public:
  explicit TextOutput(QObject *parent = nullptr);
  virtual QString deliver(const QString &text, bool autoPaste);

  enum class PasteMethod { ClipboardOnly, Xdotool, Ydotool };
  static PasteMethod choosePasteMethod(bool autoPaste, const QString &session,
                                       bool xdotoolAvailable, bool ydotoolAvailable);
  static QStringList x11PasteArguments();
  static QStringList waylandPasteArguments();
};
