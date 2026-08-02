// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AudioConversion.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
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

qsizetype maximumCaptureBytes(const QAudioFormat &format, int durationSeconds) {
  if (!format.isValid() || format.bytesPerFrame() <= 0 || format.sampleRate() <= 0 ||
      durationSeconds <= 0)
    return 0;
  const qsizetype rate = format.sampleRate();
  const qsizetype frameBytes = format.bytesPerFrame();
  const qsizetype maximum = std::numeric_limits<qsizetype>::max();
  if (rate > maximum / frameBytes || rate * frameBytes > maximum / durationSeconds)
    return 0;
  const qsizetype durationBytes = rate * frameBytes * durationSeconds;
  const qsizetype maximumFrames = maximumCapturedAudioBytes() / qsizetype(sizeof(float));
  if (frameBytes > maximum / maximumFrames)
    return 0;
  const qsizetype conversionSafeBytes = maximumFrames * frameBytes;
  return std::min({durationBytes, maximumCapturedAudioBytes(), conversionSafeBytes});
}

bool audioAppendFitsLimit(qsizetype currentBytes, qsizetype incomingBytes, qsizetype limit) {
  return currentBytes >= 0 && incomingBytes >= 0 && limit > 0 && currentBytes <= limit &&
         incomingBytes <= limit - currentBytes;
}

bool resampledFrameCount(qsizetype inputFrames, int inputRate, qsizetype *outputFrames) {
  if (inputFrames <= 0 || inputRate <= 0 || !outputFrames)
    return false;
  constexpr qsizetype whisperRate = 16000;
  const qsizetype maximum = std::numeric_limits<qsizetype>::max();
  const qsizetype wholeSeconds = inputFrames / inputRate;
  const qsizetype remainder = inputFrames % inputRate;
  if (wholeSeconds > maximum / whisperRate || remainder > maximum / whisperRate)
    return false;
  const qsizetype wholeFrames = wholeSeconds * whisperRate;
  const qsizetype partialFrames = remainder * whisperRate / inputRate;
  if (wholeFrames > maximum - partialFrames)
    return false;
  *outputFrames = wholeFrames + partialFrames;
  return *outputFrames > 0;
}

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
  if (frames > maximumCapturedAudioBytes() / qsizetype(sizeof(float)))
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

  qsizetype outputFrames = 0;
  if (!resampledFrameCount(frames, format.sampleRate(), &outputFrames) ||
      outputFrames > maximumCapturedAudioBytes() / qsizetype(sizeof(float)))
    return {};
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
