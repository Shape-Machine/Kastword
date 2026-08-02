// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAudioFormat>
#include <QByteArray>

QByteArray convertAudioForWhisper(const QByteArray &native, const QAudioFormat &format);
qsizetype maximumCaptureBytes(const QAudioFormat &format, int durationSeconds);
constexpr qsizetype maximumCapturedAudioBytes() { return 256 * 1024 * 1024; }
bool audioAppendFitsLimit(qsizetype currentBytes, qsizetype incomingBytes, qsizetype limit);
bool resampledFrameCount(qsizetype inputFrames, int inputRate, qsizetype *outputFrames);
qreal normalizedAudioPeak(const QByteArray &native, const QAudioFormat &format);
