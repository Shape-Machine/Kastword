// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AudioCapture.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>
#include <vector>

namespace {
float sampleAt(const char *data, qsizetype index, QAudioFormat::SampleFormat format) {
  switch (format) {
  case QAudioFormat::UInt8:
    return (static_cast<unsigned char>(data[index]) - 128.0F) / 128.0F;
  case QAudioFormat::Int16: {
    qint16 value;
    std::memcpy(&value, data + index * 2, 2);
    return value / 32768.0F;
  }
  case QAudioFormat::Int32: {
    qint32 value;
    std::memcpy(&value, data + index * 4, 4);
    return value / 2147483648.0F;
  }
  case QAudioFormat::Float: {
    float value;
    std::memcpy(&value, data + index * 4, 4);
    return value;
  }
  default:
    return 0.0F;
  }
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

  connect(m_device, &QIODevice::readyRead, this, [this] {
    const QByteArray chunk = m_device->readAll();
    m_audio.append(chunk);
    const qsizetype count = chunk.size() / m_format.bytesPerSample();
    float peak = 0.0F;
    for (qsizetype i = 0; i < count; ++i)
      peak = std::max(peak, std::abs(sampleAt(chunk.constData(), i, m_format.sampleFormat())));
    emit levelChanged(std::clamp<qreal>(peak, 0.0, 1.0));
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
  const qsizetype frames = native.size() / m_format.bytesPerFrame();
  if (frames <= 0)
    return {};

  std::vector<float> mono(static_cast<size_t>(frames));
  for (qsizetype frame = 0; frame < frames; ++frame) {
    float sum = 0.0F;
    for (int channel = 0; channel < m_format.channelCount(); ++channel) {
      const qsizetype sample = frame * m_format.channelCount() + channel;
      sum += sampleAt(native.constData(), sample, m_format.sampleFormat());
    }
    mono[size_t(frame)] = sum / m_format.channelCount();
  }

  const qsizetype outputFrames = frames * 16000 / m_format.sampleRate();
  QByteArray output(outputFrames * qsizetype(sizeof(float)), Qt::Uninitialized);
  auto *destination = reinterpret_cast<float *>(output.data());
  for (qsizetype i = 0; i < outputFrames; ++i) {
    const double sourcePosition = double(i) * m_format.sampleRate() / 16000.0;
    const qsizetype left = std::min<qsizetype>(qsizetype(sourcePosition), frames - 1);
    const qsizetype right = std::min(left + 1, frames - 1);
    const float fraction = float(sourcePosition - left);
    destination[i] = mono[size_t(left)] * (1.0F - fraction) + mono[size_t(right)] * fraction;
  }
  return output;
}
