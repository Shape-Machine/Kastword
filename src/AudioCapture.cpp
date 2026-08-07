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

AudioCapture::AudioCapture(QObject *parent) : QObject(parent), m_backendFactory(createBackend) {
  connect(&m_mediaDevices, &QMediaDevices::audioInputsChanged, this, [this] {
    if (m_monitoring)
      stopSource();
    emit audioInputsChanged();
    restartMonitoring();
  });
}

AudioCapture::AudioCapture(DeviceProvider deviceProvider, QObject *parent)
    : QObject(parent), m_deviceProvider(std::move(deviceProvider)),
      m_backendFactory(createBackend) {}

AudioCapture::AudioCapture(const QAudioFormat &format, BackendFactory backendFactory,
                           QObject *parent)
    : QObject(parent), m_backendFactory(std::move(backendFactory)), m_injectedFormat(format),
      m_hasInjectedBackend(true) {}

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

QString AudioCapture::deviceId(const QAudioDevice &device) { return encodedDeviceId(device.id()); }

QString AudioCapture::encodedDeviceId(const QByteArray &backendId) {
  return QString::fromLatin1(
      backendId.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QList<AudioInputDevice> AudioCapture::audioInputs() const {
  QList<AudioInputDevice> result;
  const QList<QAudioDevice> devices = QMediaDevices::audioInputs();
  result.reserve(devices.size());
  for (const QAudioDevice &device : devices)
    result.append({deviceId(device), device.description(), device.isDefault()});
  return result;
}

void AudioCapture::setSelectedDeviceId(const QString &id) {
  if (m_selectedDeviceId == id)
    return;
  if (m_monitoring)
    stopSource();
  m_selectedDeviceId = id;
  emit audioInputsChanged();
  restartMonitoring();
}

QAudioDevice AudioCapture::selectedDevice() const {
  if (m_deviceProvider)
    return m_deviceProvider();
  if (m_selectedDeviceId.isEmpty())
    return QMediaDevices::defaultAudioInput();
  const QList<QAudioDevice> devices = QMediaDevices::audioInputs();
  for (const QAudioDevice &device : devices) {
    if (deviceId(device) == m_selectedDeviceId)
      return device;
  }
  return {};
}

bool AudioCapture::selectedDeviceAvailable() const {
  if (m_hasInjectedBackend)
    return true;
  return !selectedDevice().isNull();
}

QString AudioCapture::effectiveDeviceDescription() const { return selectedDevice().description(); }

bool AudioCapture::start(QString *error) {
  if (isRecording())
    return true;
  if (m_monitoring)
    stopSource();
  if (startSource(false, error))
    return true;
  restartMonitoring();
  return false;
}

void AudioCapture::setMonitoringEnabled(bool enabled) {
  if (m_monitoringRequested == enabled)
    return;
  m_monitoringRequested = enabled;
  if (!enabled) {
    if (m_monitoring)
      stopSource();
    setMonitoringError({});
    return;
  }
  restartMonitoring();
}

void AudioCapture::retryMonitoring() {
  if (!m_monitoringRequested)
    return;
  if (m_monitoring)
    stopSource();
  restartMonitoring();
}

bool AudioCapture::startSource(bool monitoring, QString *error) {
  if (m_source)
    return false;

  const QAudioDevice device = m_hasInjectedBackend ? QAudioDevice() : selectedDevice();
  if (device.isNull() && !m_hasInjectedBackend) {
    *error = m_selectedDeviceId.isEmpty()
                 ? i18n("No microphone is available.")
                 : i18n("The selected microphone is unavailable. Choose another audio input.");
    return false;
  }

  m_format = m_hasInjectedBackend ? m_injectedFormat : device.preferredFormat();
  if (!m_format.isValid() || m_format.bytesPerSample() == 0) {
    *error = i18n("The microphone reported an invalid audio format.");
    return false;
  }

  if (!monitoring)
    m_buffer.configure(m_format, m_maximumDurationSeconds);
  if (!m_backendFactory) {
    *error = i18n("Could not start microphone capture.");
    return false;
  }
  m_source = m_backendFactory(device, m_format);
  if (!m_source) {
    *error = i18n("Could not start microphone capture.");
    return false;
  }
  m_device = m_source->start();
  if (!m_device) {
    *error = i18n("Could not start microphone capture.");
    m_source.reset();
    return false;
  }
  m_monitoring = monitoring;
  if (monitoring)
    setMonitoringError({});

  connect(m_source.get(), &AudioCaptureBackend::stateChanged, this, [this](QAudio::State state) {
    if (state != QAudio::StoppedState || !m_source || m_source->error() == QAudio::NoError)
      return;
    const QPointer<AudioCaptureBackend> failedSource = m_source.get();
    const QAudio::Error captureError = m_source->error();
    QTimer::singleShot(0, this, [this, failedSource, captureError] {
      if (!failedSource || m_source.get() != failedSource)
        return;
      const bool monitoring = m_monitoring;
      m_device = nullptr;
      m_source.reset();
      m_monitoring = false;
      if (!monitoring)
        m_buffer.configure(m_format, m_maximumDurationSeconds);
      emit levelChanged(0.0);
      if (monitoring)
        setMonitoringError(errorMessageFor(captureError));
      else
        emit captureFailed(errorMessageFor(captureError));
    });
  });

  connect(m_device, &QIODevice::readyRead, this, [this] {
    const QByteArray chunk = m_device->readAll();
    if (m_monitoring) {
      emit levelChanged(normalizedAudioPeak(chunk, m_format));
      return;
    }
    if (!m_buffer.append(chunk)) {
      m_device = nullptr;
      m_source->stop();
      m_source.reset();
      emit levelChanged(0.0);
      emit captureFailed(i18n("Recording stopped after reaching the configured duration limit."));
      restartMonitoring();
      return;
    }
    emit levelChanged(normalizedAudioPeak(chunk, m_format));
  });
  return true;
}

QByteArray AudioCapture::stop() {
  if (!isRecording())
    return {};
  stopSource();
  QByteArray audio = m_buffer.takeForWhisper();
  restartMonitoring();
  return audio;
}

void AudioCapture::stopSource() {
  if (!m_source)
    return;
  m_source->stop();
  m_device = nullptr;
  m_source.reset();
  m_monitoring = false;
  emit levelChanged(0.0);
}

void AudioCapture::restartMonitoring() {
  if (!m_monitoringRequested || m_source)
    return;
  if (!selectedDeviceAvailable()) {
    setMonitoringError({});
    return;
  }
  QString error;
  if (!startSource(true, &error))
    setMonitoringError(error);
}

void AudioCapture::setMonitoringError(const QString &error) {
  if (m_monitoringError == error)
    return;
  m_monitoringError = error;
  emit monitoringErrorChanged();
}
