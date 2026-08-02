// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TranscriptionWorker.h"

#include <KLocalizedString>
#include <exception>
#include <utility>

TranscriptionWorker::TranscriptionWorker(TranscribeFunction transcribe, QObject *parent)
    : QObject(parent), m_transcribe(std::move(transcribe)) {}

void TranscriptionWorker::transcribe(QByteArray audio, QString model, QString language) {
  try {
    const auto [text, error] = m_transcribe(audio, model, language);
    emit finished(text, error);
  } catch (const std::exception &exception) {
    emit finished({}, i18n("Transcription failed unexpectedly: %1", exception.what()));
  } catch (...) {
    emit finished({}, i18n("Transcription failed unexpectedly."));
  }
}
