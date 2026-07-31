// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AudioCapture.h"

#include "AudioConversion.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <utility>

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

  connect(m_device, &QIODevice::readyRead, this, [this] {
    const QByteArray chunk = m_device->readAll();
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
