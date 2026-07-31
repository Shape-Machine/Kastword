// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AppController.h"

#include "ModelLocator.h"
#include "WhisperEngine.h"
#include <KConfigGroup>
#include <KGlobalAccel>
#include <KNotification>
#include <QCoreApplication>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QStandardPaths>
#include <QTimer>
#include <QtConcurrent>

namespace {
QString findDefaultModel() {
  const QString fileName = QStringLiteral("ggml-base.en.bin");
  const QString applicationDirectory = QCoreApplication::applicationDirPath();
  const QStringList candidates = {
      applicationDirectory + QStringLiteral("/models/") + fileName,
      applicationDirectory + QStringLiteral("/../share/kastword/models/") + fileName,
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
          QStringLiteral("/models/") + fileName,
      QStringLiteral("/usr/share/kastword/models/") + fileName,
      QStringLiteral("/usr/local/share/kastword/models/") + fileName,
  };
  return firstReadableModel(candidates);
}
} // namespace

AppController::AppController(QObject *parent)
    : AppController(
          new AudioCapture, new TextOutput,
          [](const QByteArray &audio, const QString &model, const QString &language) {
            QString error;
            const QString text = WhisperEngine::transcribe(audio, model, language, &error);
            return qMakePair(text, error);
          },
          true, parent) {}

AppController::AppController(AudioCapture *audio, TextOutput *output, TranscribeFunction transcribe,
                             bool desktopIntegration, QObject *parent)
    : QObject(parent), m_audio(audio), m_output(output), m_transcribe(std::move(transcribe)),
      m_desktopIntegration(desktopIntegration), m_config(QStringLiteral("kastwordrc")),
      m_shortcut(tr("Toggle dictation"), this) {
  Q_ASSERT(m_audio);
  Q_ASSERT(m_output);
  Q_ASSERT(m_transcribe);
  if (!m_audio->parent())
    m_audio->setParent(this);
  if (!m_output->parent())
    m_output->setParent(this);
  initialize();
}

void AppController::initialize() {
  const KConfigGroup group(&m_config, QStringLiteral("General"));
  m_modelPath = group.readEntry("ModelPath", QString());
  if (m_modelPath.isEmpty() || !QFileInfo(m_modelPath).isFile())
    m_modelPath = findDefaultModel();
  m_language = group.readEntry("Language", QStringLiteral("en"));
  m_autoPaste = group.readEntry("AutoPaste", true);

  if (m_desktopIntegration) {
    m_shortcut.setObjectName(QStringLiteral("toggle-dictation"));
    const QList<QKeySequence> shortcut = {QKeySequence(QStringLiteral("Meta+Z"))};
    if (!group.readEntry("GlobalAccelComponentIdentityV2", false)) {
      // An earlier build used the display name as the component ID, creating a second registration
      // that competed with the executable's stable lowercase ID. Remove that duplicate once.
      KGlobalAccel::cleanComponent(QStringLiteral("Kastword"));
      KConfigGroup writableGroup(&m_config, QStringLiteral("General"));
      writableGroup.writeEntry("GlobalAccelComponentIdentityV2", true);
      writableGroup.sync();
    }
    KGlobalAccel::self()->setDefaultShortcut(&m_shortcut, shortcut);
    KGlobalAccel::self()->setShortcut(&m_shortcut, shortcut);
    if (!group.readEntry("ShortcutMigratedToMetaZ", false)) {
      const QList<QKeySequence> currentShortcut = KGlobalAccel::self()->shortcut(&m_shortcut);
      const QList<QKeySequence> previousDefault = {QKeySequence(QStringLiteral("Meta+Shift+D"))};
      const QList<QKeySequence> originalDefault = {QKeySequence(QStringLiteral("Meta+D"))};
      // Update only Kastword's former defaults. An empty or different shortcut is a user choice.
      if (currentShortcut == previousDefault || currentShortcut == originalDefault)
        KGlobalAccel::self()->setShortcut(&m_shortcut, shortcut, KGlobalAccel::NoAutoloading);
      KConfigGroup writableGroup(&m_config, QStringLiteral("General"));
      writableGroup.writeEntry("ShortcutMigratedToMetaZ", true);
      writableGroup.sync();
    }
    connect(&m_shortcut, &QAction::triggered, this, &AppController::toggle);
  }
  connect(m_audio, &AudioCapture::levelChanged, this, [this](qreal value) {
    m_level = value;
    emit levelChanged();
  });
}

void AppController::setModelPath(const QString &value) {
  if (m_modelPath == value)
    return;
  m_modelPath = value;
  saveSettings();
  emit modelPathChanged();
}

void AppController::setLanguage(const QString &value) {
  if (m_language == value)
    return;
  m_language = value;
  saveSettings();
  emit languageChanged();
}

void AppController::setAutoPaste(bool value) {
  if (m_autoPaste == value)
    return;
  m_autoPaste = value;
  saveSettings();
  emit autoPasteChanged();
}

void AppController::toggle() {
  if (m_state == QStringLiteral("transcribing")) {
    setStatus(tr("Transcription is already in progress."));
    return;
  }
  if (m_audio->isRecording()) {
    QByteArray audio = m_audio->stop();
    setState(QStringLiteral("transcribing"));
    setStatus(tr("Transcribing locally…"));
    showStatusNotification(tr("Kastword"), tr("Transcribing locally…"),
                           QStringLiteral("view-refresh"), true);
    transcribe(std::move(audio));
    return;
  }

  QString error;
  if (!m_audio->start(&error)) {
    setStatus(error);
    if (m_desktopIntegration)
      KNotification::event(KNotification::Error, tr("Kastword"), error);
    return;
  }
  setState(QStringLiteral("recording"));
  setStatus(tr("Listening — press Meta+Z to finish."));
  showStatusNotification(tr("Dictation started"), tr("Press Meta+Z again to stop."),
                         QStringLiteral("media-record"), true);
}

void AppController::transcribe(QByteArray audio) {
  auto *watcher = new QFutureWatcher<QPair<QString, QString>>(this);
  connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
    const auto [text, error] = watcher->result();
    watcher->deleteLater();
    if (!error.isEmpty()) {
      setState(QStringLiteral("idle"));
      setStatus(error);
      if (m_statusNotification)
        m_statusNotification->close();
      if (m_desktopIntegration)
        KNotification::event(KNotification::Error, tr("Transcription failed"), error);
      return;
    }
    m_transcript = text;
    emit transcriptChanged();
    const QString delivery = m_output->deliver(text, m_autoPaste);
    setStatus(delivery);
    setState(QStringLiteral("success"));
    showStatusNotification(tr("Dictation complete"), delivery, QStringLiteral("dialog-ok-apply"));
    QTimer::singleShot(1200, this, [this] {
      if (m_state == QStringLiteral("success"))
        setState(QStringLiteral("idle"));
    });
  });
  const QString model = m_modelPath;
  const QString languageCode = m_language;
  const TranscribeFunction transcribeFunction = m_transcribe;
  watcher->setFuture(
      QtConcurrent::run([audio = std::move(audio), model, languageCode, transcribeFunction] {
        return transcribeFunction(audio, model, languageCode);
      }));
}

void AppController::setState(const QString &value) {
  if (m_state == value)
    return;
  m_state = value;
  emit stateChanged();
}

void AppController::showStatusNotification(const QString &title, const QString &text,
                                           const QString &iconName, bool persistent) {
  if (!m_desktopIntegration)
    return;
  if (m_statusNotification)
    m_statusNotification->close();
  const auto flags = persistent ? KNotification::Persistent : KNotification::CloseOnTimeout;
  m_statusNotification =
      KNotification::event(KNotification::Notification, title, text, iconName, flags);
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
  group.sync();
}
