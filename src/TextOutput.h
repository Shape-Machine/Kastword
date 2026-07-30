// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class TextOutputTest;

class TextOutput final : public QObject {
  Q_OBJECT
public:
  explicit TextOutput(QObject *parent = nullptr);
  QString deliver(const QString &text, bool autoPaste);

private:
  // Allow the unit test to pin the exact virtual-key sequence used for Wayland paste delivery.
  friend class TextOutputTest;
  static QStringList waylandPasteArguments();
};
