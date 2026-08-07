// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSource>
#include <QByteArray>
#include <QList>
#include <QMediaDevices>
#include <QObject>
#include <QString>
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

struct AudioInputDevice {
  QString id;
  QString description;
  bool isDefault = false;
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
  virtual QList<AudioInputDevice> audioInputs() const;
  virtual QString selectedDeviceId() const { return m_selectedDeviceId; }
  virtual void setSelectedDeviceId(const QString &id);
  virtual bool selectedDeviceAvailable() const;
  virtual QString effectiveDeviceDescription() const;
  void setMaximumDurationSeconds(int seconds) { m_maximumDurationSeconds = seconds; }
  static QString errorMessageFor(QAudio::Error error);
  static QString deviceId(const QAudioDevice &device);
  static QString encodedDeviceId(const QByteArray &backendId);

signals:
  void levelChanged(qreal level);
  void captureFailed(const QString &error);
  void audioInputsChanged();

private:
  QAudioDevice selectedDevice() const;
  std::unique_ptr<AudioCaptureBackend> m_source;
  QIODevice *m_device = nullptr;
  QAudioFormat m_format;
  CapturedAudioBuffer m_buffer;
  int m_maximumDurationSeconds = 5 * 60;
  DeviceProvider m_deviceProvider;
  BackendFactory m_backendFactory;
  QMediaDevices m_mediaDevices;
  QString m_selectedDeviceId;
  QAudioFormat m_injectedFormat;
  bool m_hasInjectedBackend = false;
};
