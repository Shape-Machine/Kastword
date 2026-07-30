// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QString>

class WhisperEngine {
public:
  static QString transcribe(const QByteArray &audio, const QString &modelPath,
                            const QString &language, QString *error);
};
