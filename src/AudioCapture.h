// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSource>
#include <QByteArray>
#include <QMediaDevices>
#include <QObject>
#include <functional>
#include <memory>

class AudioCaptureBackend : public QObject {
  Q_OBJECT
public:
  using QObject::QObject;
  virtual QIODevice *start() = 0;
  virtual void stop() = 0;
  virtual QAudio::Error error() const = 0;

signals:
  void stateChanged(QAudio::State state);
};

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
  using DeviceProvider = std::function<QAudioDevice()>;
  using BackendFactory = std::function<std::unique_ptr<AudioCaptureBackend>(const QAudioDevice &,
                                                                            const QAudioFormat &)>;
  explicit AudioCapture(QObject *parent = nullptr);
  AudioCapture(DeviceProvider deviceProvider, QObject *parent = nullptr);
  AudioCapture(const QAudioFormat &format, BackendFactory backendFactory,
               QObject *parent = nullptr);

  virtual bool start(QString *error);
  virtual QByteArray stop();
  virtual bool isRecording() const { return m_source != nullptr; }
  void setMaximumDurationSeconds(int seconds) { m_maximumDurationSeconds = seconds; }
  static QString errorMessageFor(QAudio::Error error);

signals:
  void levelChanged(qreal level);
  void captureFailed(const QString &error);

private:
  std::unique_ptr<AudioCaptureBackend> m_source;
  QIODevice *m_device = nullptr;
  QAudioFormat m_format;
  CapturedAudioBuffer m_buffer;
  int m_maximumDurationSeconds = 5 * 60;
  DeviceProvider m_deviceProvider;
  BackendFactory m_backendFactory;
  QAudioFormat m_injectedFormat;
};
