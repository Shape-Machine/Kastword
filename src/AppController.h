// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "AudioCapture.h"
#include "ModelManager.h"
#include "TextOutput.h"
#include "TranscriptionWorker.h"

#include <KConfig>
#include <QAction>
#include <QObject>
#include <QPair>
#include <QPointer>
#include <QString>
#include <QThread>
#include <QUrl>
#include <QVariantList>
#include <memory>

class KNotification;

class AppController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(State state READ state NOTIFY stateChanged)
  Q_PROPERTY(bool idle READ isIdle NOTIFY stateChanged)
  Q_PROPERTY(bool recording READ isRecording NOTIFY stateChanged)
  Q_PROPERTY(bool transcribing READ isTranscribing NOTIFY stateChanged)
  Q_PROPERTY(QString status READ status NOTIFY statusChanged)
  Q_PROPERTY(QString transcript READ transcript NOTIFY transcriptChanged)
  Q_PROPERTY(QString modelPath READ modelPath WRITE setModelPath NOTIFY modelPathChanged)
  Q_PROPERTY(bool modelReady READ modelReady NOTIFY modelReadyChanged)
  Q_PROPERTY(bool modelSetupRequired READ modelSetupRequired NOTIFY modelReadyChanged)
  Q_PROPERTY(ModelManager *modelManager READ modelManager CONSTANT)
  Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
  Q_PROPERTY(QVariantList availableLanguages READ availableLanguages NOTIFY modelReadyChanged)
  Q_PROPERTY(bool autoPaste READ autoPaste WRITE setAutoPaste NOTIFY autoPasteChanged)
  Q_PROPERTY(int recordingLimitMinutes READ recordingLimitMinutes WRITE setRecordingLimitMinutes
                 NOTIFY recordingLimitMinutesChanged)
  Q_PROPERTY(qreal level READ level NOTIFY levelChanged)
  Q_PROPERTY(QString shortcutText READ shortcutText CONSTANT)

public:
  enum class State { Idle, Recording, Transcribing, Success };
  Q_ENUM(State)

  using TranscribeFunction = TranscriptionWorker::TranscribeFunction;

  explicit AppController(QObject *parent = nullptr);
  AppController(std::unique_ptr<AudioCapture> audio, std::unique_ptr<TextOutput> output,
                TranscribeFunction transcribe, bool desktopIntegration, QObject *parent = nullptr);
  AppController(std::unique_ptr<AudioCapture> audio, std::unique_ptr<TextOutput> output,
                TranscribeFunction transcribe, bool desktopIntegration, bool requireModel,
                QObject *parent = nullptr, std::unique_ptr<ModelManager> modelManager = {});
  ~AppController() override;
  State state() const { return m_state; }
  bool isIdle() const { return m_state == State::Idle; }
  bool isRecording() const { return m_state == State::Recording; }
  bool isTranscribing() const { return m_state == State::Transcribing; }
  QString status() const { return m_status; }
  QString transcript() const { return m_transcript; }
  QString modelPath() const { return m_modelPath; }
  bool modelReady() const { return !m_requireModel || m_modelManager->modelReady(); }
  bool modelSetupRequired() const {
    return m_requireModel && !m_modelManager->modelReady() &&
           !m_modelManager->restoringActiveModel();
  }
  ModelManager *modelManager() const { return m_modelManager.get(); }
  QString language() const { return m_language; }
  QVariantList availableLanguages() const;
  bool autoPaste() const { return m_autoPaste; }
  int recordingLimitMinutes() const { return m_recordingLimitMinutes; }
  qreal level() const { return m_level; }
  QString shortcutText() const;

  void setModelPath(const QString &value);
  void setLanguage(const QString &value);
  void setAutoPaste(bool value);
  void setRecordingLimitMinutes(int value);
  QAction *shortcutAction() { return &m_shortcut; }

  Q_INVOKABLE void toggle();
  Q_INVOKABLE void copyTranscript();
  Q_INVOKABLE void forgetTranscript();
  Q_INVOKABLE void setModelUrl(const QUrl &url);
  Q_INVOKABLE bool removeModel(const QString &id);

signals:
  void stateChanged();
  void statusChanged();
  void transcriptChanged();
  void modelPathChanged();
  void modelReadyChanged();
  void modelSetupRequested();
  void languageChanged();
  void autoPasteChanged();
  void recordingLimitMinutesChanged();
  void levelChanged();

private:
  void setState(State value);
  void setStatus(const QString &value);
  void saveSettings();
  void transcribe(QByteArray audio);
  void handleTranscriptionFinished(const QString &text, const QString &error);
  void handleCaptureFailure(const QString &error);
  void initialize();
  void showStatusNotification(const QString &title, const QString &text, const QString &iconName,
                              bool persistent = false);

  std::unique_ptr<AudioCapture> m_audio;
  std::unique_ptr<TextOutput> m_output;
  std::unique_ptr<ModelManager> m_modelManager;
  QThread m_transcriptionThread;
  TranscriptionWorker *m_transcriptionWorker;
  bool m_desktopIntegration;
  bool m_requireModel;
  KConfig m_config;
  QAction m_shortcut;
  State m_state = State::Idle;
  QString m_status;
  QString m_transcript;
  QString m_modelPath;
  QString m_language = QStringLiteral("en");
  bool m_autoPaste = false;
  int m_recordingLimitMinutes = 5;
  qreal m_level = 0.0;
  QPointer<KNotification> m_statusNotification;
};
