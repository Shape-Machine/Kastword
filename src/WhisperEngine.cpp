// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "WhisperEngine.h"

#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <cstring>
#include <limits>
#include <thread>
#include <utility>
#include <vector>
#include <whisper.h>
#ifdef Q_OS_LINUX
#include <sys/stat.h>
#endif

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
  QFile model(modelPath);
  if (!model.open(QIODevice::ReadOnly)) {
    *error = QStringLiteral("Select a valid Whisper model file.");
    return false;
  }
#ifdef Q_OS_LINUX
  struct stat modelStat{};
  if (model.handle() < 0 || fstat(model.handle(), &modelStat) != 0 || !S_ISREG(modelStat.st_mode)) {
    *error = QStringLiteral("Select a regular Whisper model file.");
    return false;
  }
  const qint64 modelSize = modelStat.st_size;
#else
  const QFileInfo modelInfo(model);
  if (!modelInfo.isFile()) {
    *error = QStringLiteral("Select a regular Whisper model file.");
    return false;
  }
  const qint64 modelSize = modelInfo.size();
#endif
  if (modelSize > maximumModelBytes()) {
    *error = QStringLiteral("The selected Whisper model is too large.");
    return false;
  }
  if (model.read(4) != QByteArray::fromHex("6c6d6767")) {
    *error = QStringLiteral("The selected file is not a supported Whisper model.");
    return false;
  }
  if (m_context && m_modelPath == modelPath)
    return true;

#ifdef Q_OS_LINUX
  // Keep this descriptor open while Whisper reopens it through procfs. This pins the validated
  // inode even if the original path or a symlink is replaced between validation and parsing.
  const QString loaderPath = QStringLiteral("/proc/self/fd/%1").arg(model.handle());
#else
  const QString loaderPath = modelPath;
#endif
  whisper_context *context = m_loader(loaderPath);
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

bool WhisperEngine::validateAudioSize(qsizetype byteCount, QString *error) {
  if (byteCount < qsizetype(sizeof(float) * 1600)) {
    *error = QStringLiteral("The recording was too short.");
    return false;
  }
  if (byteCount % qsizetype(sizeof(float)) != 0) {
    *error = QStringLiteral("The recording contains invalid audio data.");
    return false;
  }
  if (byteCount / qsizetype(sizeof(float)) > std::numeric_limits<int>::max()) {
    *error = QStringLiteral("The recording is too long to transcribe safely.");
    return false;
  }
  return true;
}

QString WhisperEngine::transcribe(const QByteArray &audio, const QString &modelPath,
                                  const QString &language, QString *error) {
  if (!validateAudioSize(audio.size(), error))
    return {};
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
