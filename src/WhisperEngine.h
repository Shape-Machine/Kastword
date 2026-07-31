// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QString>
#include <functional>

struct whisper_context;

class WhisperEngine {
public:
  using ContextLoader = std::function<whisper_context *(const QString &)>;
  using ContextDeleter = std::function<void(whisper_context *)>;

  WhisperEngine();
  WhisperEngine(ContextLoader loader, ContextDeleter deleter);
  ~WhisperEngine();
  WhisperEngine(const WhisperEngine &) = delete;
  WhisperEngine &operator=(const WhisperEngine &) = delete;

  QString transcribe(const QByteArray &audio, const QString &modelPath, const QString &language,
                     QString *error);
  bool loadModel(const QString &modelPath, QString *error);

private:
  ContextLoader m_loader;
  ContextDeleter m_deleter;
  whisper_context *m_context = nullptr;
  QString m_modelPath;
};
