// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAudioFormat>
#include <QAudioSource>
#include <QByteArray>
#include <QMediaDevices>
#include <QObject>
#include <memory>

class CapturedAudioBuffer {
public:
  void configure(const QAudioFormat &format, int maximumDurationSeconds);
  bool append(const QByteArray &chunk);
  QByteArray takeForWhisper();
  qsizetype size() const { return m_audio.size(); }

private:
  QAudioFormat m_format;
  QByteArray m_audio;
  int m_maximumDurationSeconds = 5 * 60;
};

class AudioCapture : public QObject {
  Q_OBJECT
public:
  explicit AudioCapture(QObject *parent = nullptr);

  virtual bool start(QString *error);
  virtual QByteArray stop();
  virtual bool isRecording() const { return m_source != nullptr; }
  void setMaximumDurationSeconds(int seconds) { m_maximumDurationSeconds = seconds; }

signals:
  void levelChanged(qreal level);
  void captureFailed(const QString &error);

private:
  std::unique_ptr<QAudioSource> m_source;
  QIODevice *m_device = nullptr;
  QAudioFormat m_format;
  CapturedAudioBuffer m_buffer;
  int m_maximumDurationSeconds = 5 * 60;
};
