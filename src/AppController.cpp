// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AppController.h"

#include "PlatformIntegration.h"
#include "WhisperEngine.h"
#include <KConfigGroup>
#include <KLocalizedString>
#include <QCoreApplication>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTimer>
#include <memory>

namespace {
constexpr auto defaultShortcut = "Meta+Z";

const QStringList &supportedLanguageCodes() {
  static const QStringList codes = {
      QStringLiteral("en"), QStringLiteral("auto"), QStringLiteral("de"), QStringLiteral("fr"),
      QStringLiteral("es"), QStringLiteral("nl"),   QStringLiteral("it"), QStringLiteral("pt"),
  };
  return codes;
}

} // namespace

AppController::AppController(QObject *parent)
    : AppController(
          std::make_unique<AudioCapture>(), std::make_unique<TextOutput>(),
          [engine = std::make_shared<WhisperEngine>()](
              const QByteArray &audio, const QString &model, const QString &language) {
            QString error;
            const QString text = engine->transcribe(audio, model, language, &error);
            return qMakePair(text, error);
          },
          true, true, parent) {}

AppController::AppController(std::unique_ptr<AudioCapture> audio,
                             std::unique_ptr<TextOutput> output, TranscribeFunction transcribe,
                             bool desktopIntegration, QObject *parent)
    : AppController(std::move(audio), std::move(output), std::move(transcribe), desktopIntegration,
                    false, parent) {}

AppController::AppController(std::unique_ptr<AudioCapture> audio,
                             std::unique_ptr<TextOutput> output, TranscribeFunction transcribe,
                             bool desktopIntegration, bool requireModel, QObject *parent,
                             std::unique_ptr<ModelManager> modelManager,
                             std::unique_ptr<DesktopIntegration> desktopServices)
    : QObject(parent), m_audio(std::move(audio)), m_output(std::move(output)),
      m_modelManager(modelManager ? std::move(modelManager) : std::make_unique<ModelManager>()),
      m_desktopServices(desktopServices
                            ? std::move(desktopServices)
                            : (desktopIntegration ? createDesktopIntegration() : nullptr)),
      m_transcriptionWorker(new TranscriptionWorker(std::move(transcribe))),
      m_desktopIntegration(desktopIntegration), m_requireModel(requireModel),
      m_config(QStringLiteral("kastwordrc")), m_shortcut(i18n("Toggle dictation"), this) {
  Q_ASSERT(m_audio);
  Q_ASSERT(m_output);
  m_transcriptionWorker->moveToThread(&m_transcriptionThread);
  connect(&m_transcriptionThread, &QThread::finished, m_transcriptionWorker, &QObject::deleteLater);
  connect(m_transcriptionWorker, &TranscriptionWorker::finished, this,
          &AppController::handleTranscriptionFinished);
  m_transcriptionThread.start();
  initialize();
}

AppController::~AppController() {
  m_transcriptionThread.quit();
  m_transcriptionThread.wait();
}

void AppController::initialize() {
  const KConfigGroup group(&m_config, QStringLiteral("General"));
  m_modelPath = group.readEntry("ModelPath", QString());
  m_language = group.readEntry("Language", QStringLiteral("en"));
  if (!supportedLanguageCodes().contains(m_language)) {
    m_language = QStringLiteral("en");
    KConfigGroup writableGroup(&m_config, QStringLiteral("General"));
    writableGroup.writeEntry("Language", m_language);
    writableGroup.sync();
  }
  m_autoPaste = group.readEntry("AutoPaste", false);
  m_recordingLimitMinutes = qBound(1, group.readEntry("RecordingLimitMinutes", 5), 60);
  m_audio->setMaximumDurationSeconds(m_recordingLimitMinutes * 60);

  connect(m_modelManager.get(), &ModelManager::activeModelPathChanged, this, [this] {
    const QString path = m_modelManager->activeModelPath();
    if (m_modelManager->activeModelEnglishOnly() && m_language != QStringLiteral("en")) {
      m_language = QStringLiteral("en");
      saveSettings();
      emit languageChanged();
    }
    if (m_modelPath != path) {
      m_modelPath = path;
      saveSettings();
      emit modelPathChanged();
    }
    bool recordingStopped = false;
    if (!modelReady() && m_audio->isRecording()) {
      m_audio->stop();
      setState(State::Idle);
      setStatus(i18n("Recording stopped because the speech model is no longer available."));
      recordingStopped = true;
    }
    if (modelReady())
      setStatus(i18n("Ready"));
    else if (m_modelManager->verificationPending() && !recordingStopped)
      setStatus(i18n("Verifying the speech model…"));
    m_shortcut.setEnabled(modelReady());
    emit modelReadyChanged();
  });
  connect(m_modelManager.get(), &ModelManager::changed, this, [this] {
    if (!modelReady() && m_modelManager->verificationPending())
      setStatus(i18n("Verifying the speech model…"));
  });
  connect(m_modelManager.get(), &ModelManager::setupRequired, this, [this] {
    if (!m_modelPath.isEmpty()) {
      m_modelPath.clear();
      saveSettings();
      emit modelPathChanged();
    }
    setStatus(i18n("Choose a speech model to enable dictation."));
    emit modelReadyChanged();
    emit modelSetupRequested();
  });
  if (m_requireModel)
    m_modelManager->restoreActiveModel(m_modelPath);

  if (m_desktopIntegration) {
    m_shortcut.setObjectName(QStringLiteral("toggle-dictation"));
    const QList<QKeySequence> shortcut = {QKeySequence(shortcutText())};
    if (!group.readEntry("GlobalAccelComponentIdentityV2", false)) {
      // An earlier build used the display name as the component ID, creating a second registration
      // that competed with the executable's stable lowercase ID. Remove that duplicate once.
      m_desktopServices->cleanShortcutComponent(QStringLiteral("Kastword"));
      KConfigGroup writableGroup(&m_config, QStringLiteral("General"));
      writableGroup.writeEntry("GlobalAccelComponentIdentityV2", true);
      writableGroup.sync();
    }
    m_desktopServices->configureShortcut(&m_shortcut, shortcut);
    if (!group.readEntry("ShortcutMigratedToMetaZ", false)) {
      const QList<QKeySequence> currentShortcut = m_desktopServices->shortcuts(&m_shortcut);
      const QList<QKeySequence> previousDefault = {QKeySequence(QStringLiteral("Meta+Shift+D"))};
      const QList<QKeySequence> originalDefault = {QKeySequence(QStringLiteral("Meta+D"))};
      // Update only Kastword's former defaults. An empty or different shortcut is a user choice.
      if (currentShortcut == previousDefault || currentShortcut == originalDefault)
        m_desktopServices->setShortcuts(&m_shortcut, shortcut, false);
      KConfigGroup writableGroup(&m_config, QStringLiteral("General"));
      writableGroup.writeEntry("ShortcutMigratedToMetaZ", true);
      writableGroup.sync();
    }
    connect(&m_shortcut, &QAction::triggered, this, &AppController::toggle);
  }
  m_shortcut.setEnabled(modelReady());
  connect(m_audio.get(), &AudioCapture::levelChanged, this, [this](qreal value) {
    m_level = value;
    emit levelChanged();
  });
  connect(m_audio.get(), &AudioCapture::captureFailed, this, &AppController::handleCaptureFailure);
  connect(m_output.get(), &TextOutput::deliveryStatus, this, [this](const QString &status) {
    setStatus(status);
    if (m_desktopIntegration && status.contains(i18n("failed"), Qt::CaseInsensitive))
      m_desktopServices->showNotification(DesktopIntegration::NotificationKind::Error,
                                          i18n("Automatic paste failed"), status);
  });
  if (modelReady())
    setStatus(i18n("Ready"));
  else if (m_modelManager->verificationPending())
    setStatus(i18n("Verifying the speech model…"));
  else
    setStatus(i18n("Choose a speech model to enable dictation."));
}

QString AppController::shortcutText() const { return QString::fromLatin1(defaultShortcut); }

QVariantList AppController::availableLanguages() const {
  QVariantList languages = {
      QVariantMap{{QStringLiteral("code"), QStringLiteral("en")},
                  {QStringLiteral("name"), i18n("English")}},
      QVariantMap{{QStringLiteral("code"), QStringLiteral("auto")},
                  {QStringLiteral("name"), i18n("Automatic")}},
      QVariantMap{{QStringLiteral("code"), QStringLiteral("de")},
                  {QStringLiteral("name"), i18n("German")}},
      QVariantMap{{QStringLiteral("code"), QStringLiteral("fr")},
                  {QStringLiteral("name"), i18n("French")}},
      QVariantMap{{QStringLiteral("code"), QStringLiteral("es")},
                  {QStringLiteral("name"), i18n("Spanish")}},
      QVariantMap{{QStringLiteral("code"), QStringLiteral("nl")},
                  {QStringLiteral("name"), i18n("Dutch")}},
      QVariantMap{{QStringLiteral("code"), QStringLiteral("it")},
                  {QStringLiteral("name"), i18n("Italian")}},
      QVariantMap{{QStringLiteral("code"), QStringLiteral("pt")},
                  {QStringLiteral("name"), i18n("Portuguese")}},
  };
  if (m_requireModel && m_modelManager->activeModelEnglishOnly())
    languages = {languages.constFirst()};
  return languages;
}

void AppController::setModelPath(const QString &value) {
  if (m_modelPath == value)
    return;
  m_modelPath = value;
  saveSettings();
  emit modelPathChanged();
}

void AppController::setLanguage(const QString &value) {
  const QString supportedValue =
      supportedLanguageCodes().contains(value) ? value : QStringLiteral("en");
  if (m_language == supportedValue)
    return;
  m_language = supportedValue;
  saveSettings();
  emit languageChanged();
}

void AppController::setModelUrl(const QUrl &url) {
  if (m_requireModel)
    m_modelManager->selectLocalModel(url);
  else if (url.isLocalFile())
    setModelPath(url.toLocalFile());
}

bool AppController::removeModel(const QString &id) {
  if (m_state != State::Idle)
    return false;
  return m_modelManager->removeModel(id);
}

void AppController::setAutoPaste(bool value) {
  if (m_autoPaste == value)
    return;
  m_autoPaste = value;
  saveSettings();
  emit autoPasteChanged();
}

void AppController::setRecordingLimitMinutes(int value) {
  value = qBound(1, value, 60);
  if (m_recordingLimitMinutes == value)
    return;
  m_recordingLimitMinutes = value;
  m_audio->setMaximumDurationSeconds(value * 60);
  saveSettings();
  emit recordingLimitMinutesChanged();
}

void AppController::forgetTranscript() {
  if (m_transcript.isEmpty())
    return;
  m_output->forget(m_transcript);
  m_transcript.clear();
  emit transcriptChanged();
  setStatus(i18n(
      "Cleared from Kastword and matching current clipboards. Clipboard history may retain it."));
}

void AppController::copyTranscript() {
  if (m_transcript.isEmpty())
    return;
  setStatus(m_output->deliver(m_transcript, false));
}

void AppController::toggle() {
  if (m_audio->isRecording()) {
    QByteArray audio = m_audio->stop();
    if (!modelReady()) {
      setState(State::Idle);
      setStatus(i18n("Recording stopped because the speech model is no longer available."));
      emit modelSetupRequested();
      return;
    }
    setState(State::Transcribing);
    setStatus(i18n("Transcribing locally…"));
    showStatusNotification(i18n("Kastword"), i18n("Transcribing locally…"),
                           QStringLiteral("view-refresh"), true);
    transcribe(std::move(audio));
    return;
  }
  if (!modelReady()) {
    setStatus(i18n("Choose a speech model before starting dictation."));
    emit modelSetupRequested();
    return;
  }
  if (m_state == State::Transcribing) {
    setStatus(i18n("Transcription is already in progress."));
    return;
  }
  QString error;
  if (!m_audio->start(&error)) {
    setStatus(error);
    if (m_desktopIntegration)
      m_desktopServices->showNotification(DesktopIntegration::NotificationKind::Error,
                                          i18n("Kastword"), error);
    return;
  }
  setState(State::Recording);
  setStatus(i18n("Listening — press %1 to finish.", shortcutText()));
  showStatusNotification(i18n("Dictation started"), i18n("Press %1 again to stop.", shortcutText()),
                         QStringLiteral("media-record"), true);
}

void AppController::transcribe(QByteArray audio) {
  const QString model = m_modelPath;
  const QString languageCode = m_language;
  QMetaObject::invokeMethod(
      m_transcriptionWorker,
      [worker = m_transcriptionWorker, audio = std::move(audio), model, languageCode]() mutable {
        worker->transcribe(std::move(audio), model, languageCode);
      },
      Qt::QueuedConnection);
}

void AppController::handleTranscriptionFinished(const QString &text, const QString &error) {
  if (!error.isEmpty()) {
    setState(State::Idle);
    setStatus(error);
    if (m_desktopServices)
      m_desktopServices->closeStatusNotification();
    if (m_desktopIntegration)
      m_desktopServices->showNotification(DesktopIntegration::NotificationKind::Error,
                                          i18n("Transcription failed"), error);
    return;
  }
  const QString trimmedText = text.trimmed();
  if (trimmedText.isEmpty()) {
    setState(State::Idle);
    setStatus(i18n("No speech detected."));
    if (m_desktopServices)
      m_desktopServices->closeStatusNotification();
    return;
  }
  m_transcript = trimmedText;
  emit transcriptChanged();
  const QString delivery = m_output->deliver(trimmedText, m_autoPaste);
  setStatus(delivery);
  setState(State::Success);
  showStatusNotification(i18n("Dictation complete"), delivery, QStringLiteral("dialog-ok-apply"));
  QTimer::singleShot(1200, this, [this] {
    if (m_state == State::Success)
      setState(State::Idle);
  });
}

void AppController::handleCaptureFailure(const QString &error) {
  if (m_state != State::Recording)
    return;
  if (!qFuzzyIsNull(m_level)) {
    m_level = 0.0;
    emit levelChanged();
  }
  setState(State::Idle);
  setStatus(error);
  if (m_desktopServices)
    m_desktopServices->closeStatusNotification();
  if (m_desktopIntegration)
    m_desktopServices->showNotification(DesktopIntegration::NotificationKind::Error,
                                        i18n("Recording failed"), error);
}

void AppController::setState(State value) {
  if (m_state == value)
    return;
  m_state = value;
  emit stateChanged();
}

void AppController::showStatusNotification(const QString &title, const QString &text,
                                           const QString &iconName, bool persistent) {
  if (!m_desktopIntegration)
    return;
  m_desktopServices->closeStatusNotification();
  m_desktopServices->showNotification(DesktopIntegration::NotificationKind::Information, title,
                                      text, iconName, persistent);
}

void AppController::setStatus(const QString &value) {
  m_status = value;
  emit statusChanged();
}

void AppController::saveSettings() {
  KConfigGroup group(&m_config, QStringLiteral("General"));
  group.writeEntry("ModelPath", m_modelPath);
  group.writeEntry("Language", m_language);
  group.writeEntry("AutoPaste", m_autoPaste);
  group.writeEntry("RecordingLimitMinutes", m_recordingLimitMinutes);
  group.sync();
}
