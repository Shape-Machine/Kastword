// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAudioFormat>
#include <QAudioSource>
#include <QByteArray>
#include <QMediaDevices>
#include <QObject>
#include <memory>

class AudioCapture : public QObject {
  Q_OBJECT
public:
  explicit AudioCapture(QObject *parent = nullptr);

  virtual bool start(QString *error);
  virtual QByteArray stop();
  virtual bool isRecording() const { return m_source != nullptr; }

signals:
  void levelChanged(qreal level);
  void captureFailed(const QString &error);

private:
  std::unique_ptr<QAudioSource> m_source;
  QIODevice *m_device = nullptr;
  QAudioFormat m_format;
  QByteArray m_audio;
};
