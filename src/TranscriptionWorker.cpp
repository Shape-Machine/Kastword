// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TranscriptionWorker.h"

#include <exception>
#include <utility>

TranscriptionWorker::TranscriptionWorker(TranscribeFunction transcribe, QObject *parent)
    : QObject(parent), m_transcribe(std::move(transcribe)) {}

void TranscriptionWorker::transcribe(QByteArray audio, QString model, QString language) {
  try {
    const auto [text, error] = m_transcribe(audio, model, language);
    emit finished(text, error);
  } catch (const std::exception &exception) {
    emit finished({}, tr("Transcription failed unexpectedly: %1").arg(exception.what()));
  } catch (...) {
    emit finished({}, tr("Transcription failed unexpectedly."));
  }
}
