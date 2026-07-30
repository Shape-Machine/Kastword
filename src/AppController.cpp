// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AppController.h"

#include "WhisperEngine.h"
#include <KConfigGroup>
#include <KGlobalAccel>
#include <KNotification>
#include <QCoreApplication>
#include <QCursor>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QScreen>
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
  for (const QString &candidate : candidates) {
    const QFileInfo info(candidate);
    if (info.isFile() && info.isReadable())
      return info.canonicalFilePath();
  }
  return {};
}
} // namespace

AppController::AppController(QObject *parent)
    : QObject(parent), m_audio(this), m_output(this), m_config(QStringLiteral("kastwordrc")),
      m_shortcut(tr("Toggle dictation"), this) {
  const KConfigGroup group(&m_config, QStringLiteral("General"));
  m_modelPath = group.readEntry("ModelPath", QString());
  if (m_modelPath.isEmpty() || !QFileInfo(m_modelPath).isFile())
    m_modelPath = findDefaultModel();
  m_language = group.readEntry("Language", QStringLiteral("en"));
  m_autoPaste = group.readEntry("AutoPaste", true);

  m_shortcut.setObjectName(QStringLiteral("toggle-dictation"));
  const QList<QKeySequence> shortcut = {QKeySequence(QStringLiteral("Meta+Shift+D"))};
  KGlobalAccel::self()->setDefaultShortcut(&m_shortcut, shortcut);
  if (!group.readEntry("ShortcutMigratedToMetaShiftD", false)) {
    // Migrate the original conflicting Meta+D default exactly once, then preserve user changes.
    KGlobalAccel::self()->setShortcut(&m_shortcut, shortcut, KGlobalAccel::NoAutoloading);
    KConfigGroup writableGroup(&m_config, QStringLiteral("General"));
    writableGroup.writeEntry("ShortcutMigratedToMetaShiftD", true);
    writableGroup.sync();
  } else {
    KGlobalAccel::self()->setShortcut(&m_shortcut, shortcut);
  }
  connect(&m_shortcut, &QAction::triggered, this, &AppController::toggle);
  connect(&m_audio, &AudioCapture::levelChanged, this, [this](qreal value) {
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
  if (m_audio.isRecording()) {
    QByteArray audio = m_audio.stop();
    setState(QStringLiteral("transcribing"));
    setStatus(tr("Transcribing locally…"));
    transcribe(std::move(audio));
    return;
  }

  QString error;
  if (!m_audio.start(&error)) {
    setStatus(error);
    KNotification::event(KNotification::Error, tr("Kastword"), error);
    return;
  }
  setState(QStringLiteral("recording"));
  setStatus(tr("Listening — press Meta+Shift+D to finish."));
}

void AppController::transcribe(QByteArray audio) {
  auto *watcher = new QFutureWatcher<QPair<QString, QString>>(this);
  connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
    const auto [text, error] = watcher->result();
    watcher->deleteLater();
    if (!error.isEmpty()) {
      setState(QStringLiteral("idle"));
      setStatus(error);
      KNotification::event(KNotification::Error, tr("Transcription failed"), error);
      return;
    }
    m_transcript = text;
    emit transcriptChanged();
    const QString delivery = m_output.deliver(text, m_autoPaste);
    setStatus(delivery);
    setState(QStringLiteral("success"));
    QTimer::singleShot(1200, this, [this] {
      if (m_state == QStringLiteral("success"))
        setState(QStringLiteral("idle"));
    });
  });
  const QString model = m_modelPath;
  const QString languageCode = m_language;
  watcher->setFuture(QtConcurrent::run([audio = std::move(audio), model, languageCode] {
    QString error;
    QString text = WhisperEngine::transcribe(audio, model, languageCode, &error);
    return qMakePair(text, error);
  }));
}

void AppController::setState(const QString &value) {
  if (m_state == value)
    return;
  m_state = value;
  if (value != QStringLiteral("idle"))
    updateOverlayPosition();
  emit stateChanged();
}

void AppController::updateOverlayPosition() {
  QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
  if (!screen)
    screen = QGuiApplication::primaryScreen();
  if (!screen)
    return;

  constexpr int overlayWidth = 300;
  constexpr int overlayHeight = 68;
  constexpr int bottomMargin = 48;
  const QRect available = screen->availableGeometry();
  m_overlayX = available.x() + (available.width() - overlayWidth) / 2;
  m_overlayY = available.y() + available.height() - overlayHeight - bottomMargin;
  emit overlayPositionChanged();
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
