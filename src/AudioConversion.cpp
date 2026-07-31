// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AudioConversion.h"

#include <algorithm>
#include <cmath>
#include <cstring>
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

qreal normalizedAudioPeak(const QByteArray &native, const QAudioFormat &format) {
  if (!format.isValid() || format.bytesPerSample() <= 0)
    return 0.0;

  const qsizetype count = native.size() / format.bytesPerSample();
  float peak = 0.0F;
  for (qsizetype i = 0; i < count; ++i)
    peak = std::max(peak, std::abs(sampleAt(native.constData(), i, format.sampleFormat())));
  return std::clamp<qreal>(peak, 0.0, 1.0);
}

QByteArray convertAudioForWhisper(const QByteArray &native, const QAudioFormat &format) {
  if (!format.isValid() || format.bytesPerFrame() <= 0 || format.sampleRate() <= 0 ||
      format.channelCount() <= 0)
    return {};

  const qsizetype frames = native.size() / format.bytesPerFrame();
  if (frames <= 0)
    return {};

  std::vector<float> mono(static_cast<size_t>(frames));
  for (qsizetype frame = 0; frame < frames; ++frame) {
    float sum = 0.0F;
    for (int channel = 0; channel < format.channelCount(); ++channel) {
      const qsizetype sample = frame * format.channelCount() + channel;
      sum += sampleAt(native.constData(), sample, format.sampleFormat());
    }
    mono[size_t(frame)] = sum / format.channelCount();
  }

  const qsizetype outputFrames = frames * 16000 / format.sampleRate();
  QByteArray output(outputFrames * qsizetype(sizeof(float)), Qt::Uninitialized);
  auto *destination = reinterpret_cast<float *>(output.data());
  for (qsizetype i = 0; i < outputFrames; ++i) {
    const double sourcePosition = double(i) * format.sampleRate() / 16000.0;
    const qsizetype left = std::min<qsizetype>(qsizetype(sourcePosition), frames - 1);
    const qsizetype right = std::min(left + 1, frames - 1);
    const float fraction = float(sourcePosition - left);
    destination[i] = mono[size_t(left)] * (1.0F - fraction) + mono[size_t(right)] * fraction;
  }
  return output;
}
