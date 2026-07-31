// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "WhisperEngine.h"

#include <QFileInfo>
#include <algorithm>
#include <cstring>
#include <thread>
#include <utility>
#include <vector>
#include <whisper.h>

WhisperEngine::WhisperEngine()
    : WhisperEngine(
          [](const QString &path) {
            const whisper_context_params params = whisper_context_default_params();
            return whisper_init_from_file_with_params(path.toUtf8().constData(), params);
          },
          [](whisper_context *context) { whisper_free(context); }) {}

WhisperEngine::WhisperEngine(ContextLoader loader, ContextDeleter deleter)
    : m_loader(std::move(loader)), m_deleter(std::move(deleter)) {}

WhisperEngine::~WhisperEngine() {
  if (m_context)
    m_deleter(m_context);
}

bool WhisperEngine::loadModel(const QString &modelPath, QString *error) {
  if (!QFileInfo(modelPath).isFile()) {
    *error = QStringLiteral("Select a valid Whisper model file.");
    return false;
  }
  if (m_context && m_modelPath == modelPath)
    return true;

  whisper_context *context = m_loader(modelPath);
  if (!context) {
    *error = QStringLiteral("Could not load the Whisper model.");
    return false;
  }

  if (m_context)
    m_deleter(m_context);
  m_context = context;
  m_modelPath = modelPath;
  return true;
}

QString WhisperEngine::transcribe(const QByteArray &audio, const QString &modelPath,
                                  const QString &language, QString *error) {
  if (audio.size() < int(sizeof(float) * 1600)) {
    *error = QStringLiteral("The recording was too short.");
    return {};
  }
  if (audio.size() % int(sizeof(float)) != 0) {
    *error = QStringLiteral("The recording contains invalid audio data.");
    return {};
  }
  if (!loadModel(modelPath, error))
    return {};

  std::vector<float> samples(size_t(audio.size() / int(sizeof(float))));
  std::memcpy(samples.data(), audio.constData(), samples.size() * sizeof(float));
  whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
  params.print_progress = false;
  params.print_realtime = false;
  params.print_timestamps = false;
  params.no_timestamps = true;
  params.single_segment = false;
  params.n_threads = std::max(1U, std::thread::hardware_concurrency() / 2);
  const QByteArray languageUtf8 = language.toUtf8();
  params.language = languageUtf8.constData();

  const int result = whisper_full(m_context, params, samples.data(), int(samples.size()));
  if (result != 0) {
    *error = QStringLiteral("Whisper could not transcribe the recording.");
    return {};
  }

  QString text;
  const int segments = whisper_full_n_segments(m_context);
  for (int i = 0; i < segments; ++i)
    text += QString::fromUtf8(whisper_full_get_segment_text(m_context, i));
  return text.trimmed();
}
