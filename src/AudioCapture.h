// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAudioFormat>
#include <QAudioSource>
#include <QByteArray>
#include <QMediaDevices>
#include <QObject>
#include <memory>

class AudioCapture final : public QObject {
  Q_OBJECT
public:
  explicit AudioCapture(QObject *parent = nullptr);

  bool start(QString *error);
  QByteArray stop();
  bool isRecording() const { return m_source != nullptr; }

signals:
  void levelChanged(qreal level);

private:
  std::unique_ptr<QAudioSource> m_source;
  QIODevice *m_device = nullptr;
  QAudioFormat m_format;
  QByteArray m_audio;
};
