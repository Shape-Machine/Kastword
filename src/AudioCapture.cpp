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
class QtAudioCaptureBackend final : public AudioCaptureBackend {
public:
  QtAudioCaptureBackend(const QAudioDevice &device, const QAudioFormat &format)
      : m_source(device, format) {
    connect(&m_source, &QAudioSource::stateChanged, this, &AudioCaptureBackend::stateChanged);
  }

  QIODevice *start() override { return m_source.start(); }
  void stop() override { m_source.stop(); }
  QAudio::Error error() const override { return m_source.error(); }

private:
  QAudioSource m_source;
};

std::unique_ptr<AudioCaptureBackend> createBackend(const QAudioDevice &device,
                                                   const QAudioFormat &format) {
  return std::make_unique<QtAudioCaptureBackend>(device, format);
}
} // namespace

QString AudioCapture::errorMessageFor(QAudio::Error error) {
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

AudioCapture::AudioCapture(QObject *parent)
    : AudioCapture([] { return QMediaDevices::defaultAudioInput(); }, parent) {}

AudioCapture::AudioCapture(DeviceProvider deviceProvider, QObject *parent)
    : QObject(parent), m_deviceProvider(std::move(deviceProvider)),
      m_backendFactory(createBackend) {}

AudioCapture::AudioCapture(const QAudioFormat &format, BackendFactory backendFactory,
                           QObject *parent)
    : QObject(parent), m_backendFactory(std::move(backendFactory)), m_injectedFormat(format) {}

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

  const QAudioDevice device = m_injectedFormat.isValid() ? QAudioDevice() : m_deviceProvider();
  if (device.isNull() && !m_injectedFormat.isValid()) {
    *error = i18n("No microphone is available.");
    return false;
  }

  m_format = m_injectedFormat.isValid() ? m_injectedFormat : device.preferredFormat();
  if (!m_format.isValid() || m_format.bytesPerSample() == 0) {
    *error = i18n("The microphone reported an invalid audio format.");
    return false;
  }

  m_buffer.configure(m_format, m_maximumDurationSeconds);
  m_source = m_backendFactory(device, m_format);
  m_device = m_source->start();
  if (!m_device) {
    *error = i18n("Could not start microphone capture.");
    m_source.reset();
    return false;
  }

  connect(m_source.get(), &AudioCaptureBackend::stateChanged, this, [this](QAudio::State state) {
    if (state != QAudio::StoppedState || !m_source || m_source->error() == QAudio::NoError)
      return;
    const QPointer<AudioCaptureBackend> failedSource = m_source.get();
    const QAudio::Error captureError = m_source->error();
    QTimer::singleShot(0, this, [this, failedSource, captureError] {
      if (!failedSource || m_source.get() != failedSource)
        return;
      m_device = nullptr;
      m_source.reset();
      m_buffer.configure(m_format, m_maximumDurationSeconds);
      emit levelChanged(0.0);
      emit captureFailed(errorMessageFor(captureError));
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
