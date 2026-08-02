// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AudioCapture.h"

#include "AudioConversion.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QPointer>
#include <QTimer>
#include <utility>

namespace {
QString captureErrorMessage(QAudio::Error error) {
  switch (error) {
  case QAudio::OpenError:
    return AudioCapture::tr("The microphone could not be opened.");
  case QAudio::IOError:
    return AudioCapture::tr("The microphone stopped responding.");
  case QAudio::FatalError:
    return AudioCapture::tr("The microphone backend failed.");
  case QAudio::NoError:
    break;
  }
  return AudioCapture::tr("Microphone capture stopped unexpectedly.");
}
} // namespace

AudioCapture::AudioCapture(QObject *parent) : QObject(parent) {}

bool AudioCapture::start(QString *error) {
  if (m_source)
    return true;

  const QAudioDevice device = QMediaDevices::defaultAudioInput();
  if (device.isNull()) {
    *error = tr("No microphone is available.");
    return false;
  }

  m_format = device.preferredFormat();
  if (!m_format.isValid() || m_format.bytesPerSample() == 0) {
    *error = tr("The microphone reported an invalid audio format.");
    return false;
  }

  m_audio.clear();
  m_source = std::make_unique<QAudioSource>(device, m_format);
  m_device = m_source->start();
  if (!m_device) {
    *error = tr("Could not start microphone capture.");
    m_source.reset();
    return false;
  }

  connect(m_source.get(), &QAudioSource::stateChanged, this, [this](QAudio::State state) {
    if (state != QAudio::StoppedState || !m_source || m_source->error() == QAudio::NoError)
      return;
    const QPointer<QAudioSource> failedSource = m_source.get();
    const QAudio::Error captureError = m_source->error();
    QTimer::singleShot(0, this, [this, failedSource, captureError] {
      if (!failedSource || m_source.get() != failedSource)
        return;
      m_device = nullptr;
      m_source.reset();
      m_audio.clear();
      emit levelChanged(0.0);
      emit captureFailed(captureErrorMessage(captureError));
    });
  });

  connect(m_device, &QIODevice::readyRead, this, [this] {
    const QByteArray chunk = m_device->readAll();
    const qsizetype limit = maximumCaptureBytes(m_format, m_maximumDurationSeconds);
    if (!audioAppendFitsLimit(m_audio.size(), chunk.size(), limit)) {
      m_device = nullptr;
      m_source->stop();
      m_source.reset();
      m_audio.clear();
      emit levelChanged(0.0);
      emit captureFailed(tr("Recording stopped after reaching the configured duration limit."));
      return;
    }
    m_audio.append(chunk);
    emit levelChanged(normalizedAudioPeak(chunk, m_format));
  });
  return true;
}

QByteArray AudioCapture::stop() {
  if (!m_source)
    return {};
  m_source->stop();
  m_device = nullptr;
  m_source.reset();
  emit levelChanged(0.0);
  const QByteArray native = std::exchange(m_audio, {});
  return convertAudioForWhisper(native, m_format);
}
