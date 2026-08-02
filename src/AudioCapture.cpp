// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AudioCapture.h"

#include "AudioConversion.h"
#include <KLocalizedString>

#include <QAudioDevice>
#include <QAudioFormat>
#include <QPointer>
#include <QTimer>
#include <utility>

namespace {
QString captureErrorMessage(QAudio::Error error) {
  switch (error) {
  case QAudio::OpenError:
    return i18n("The microphone could not be opened.");
  case QAudio::IOError:
    return i18n("The microphone stopped responding.");
  case QAudio::FatalError:
    return i18n("The microphone backend failed.");
  case QAudio::NoError:
    break;
  }
  return i18n("Microphone capture stopped unexpectedly.");
}
} // namespace

AudioCapture::AudioCapture(QObject *parent) : QObject(parent) {}

void CapturedAudioBuffer::configure(const QAudioFormat &format, int maximumDurationSeconds) {
  m_format = format;
  m_maximumDurationSeconds = maximumDurationSeconds;
  m_audio.clear();
}

bool CapturedAudioBuffer::append(const QByteArray &chunk) {
  const qsizetype limit = maximumCaptureBytes(m_format, m_maximumDurationSeconds);
  if (!audioAppendFitsLimit(m_audio.size(), chunk.size(), limit)) {
    m_audio.clear();
    return false;
  }
  m_audio.append(chunk);
  return true;
}

QByteArray CapturedAudioBuffer::takeForWhisper() {
  return convertAudioForWhisper(std::exchange(m_audio, {}), m_format);
}

bool AudioCapture::start(QString *error) {
  if (m_source)
    return true;

  const QAudioDevice device = QMediaDevices::defaultAudioInput();
  if (device.isNull()) {
    *error = i18n("No microphone is available.");
    return false;
  }

  m_format = device.preferredFormat();
  if (!m_format.isValid() || m_format.bytesPerSample() == 0) {
    *error = i18n("The microphone reported an invalid audio format.");
    return false;
  }

  m_buffer.configure(m_format, m_maximumDurationSeconds);
  m_source = std::make_unique<QAudioSource>(device, m_format);
  m_device = m_source->start();
  if (!m_device) {
    *error = i18n("Could not start microphone capture.");
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
      m_buffer.configure(m_format, m_maximumDurationSeconds);
      emit levelChanged(0.0);
      emit captureFailed(captureErrorMessage(captureError));
    });
  });

  connect(m_device, &QIODevice::readyRead, this, [this] {
    const QByteArray chunk = m_device->readAll();
    if (!m_buffer.append(chunk)) {
      m_device = nullptr;
      m_source->stop();
      m_source.reset();
      emit levelChanged(0.0);
      emit captureFailed(i18n("Recording stopped after reaching the configured duration limit."));
      return;
    }
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
  return m_buffer.takeForWhisper();
}
