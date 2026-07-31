// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QObject>
#include <QPair>
#include <QString>
#include <functional>

class TranscriptionWorker final : public QObject {
  Q_OBJECT

public:
  using TranscribeFunction =
      std::function<QPair<QString, QString>(const QByteArray &, const QString &, const QString &)>;

  explicit TranscriptionWorker(TranscribeFunction transcribe, QObject *parent = nullptr);

  void transcribe(QByteArray audio, QString model, QString language);

signals:
  void finished(const QString &text, const QString &error);

private:
  TranscribeFunction m_transcribe;
};
