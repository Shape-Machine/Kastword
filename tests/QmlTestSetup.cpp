// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <QQmlContext>
#include <QQmlEngine>
#include <QUrl>
#include <QVariantList>
#include <QtQuickTest/quicktest.h>

class FakeModelManager final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList models READ models CONSTANT)
  Q_PROPERTY(bool busy READ busy CONSTANT)
  Q_PROPERTY(qreal progress READ progress CONSTANT)
  Q_PROPERTY(QString status READ status CONSTANT)
  Q_PROPERTY(QString error READ error CONSTANT)
  Q_PROPERTY(QString storagePath READ storagePath CONSTANT)

public:
  QVariantList models() const {
    return {QVariantMap{{QStringLiteral("id"), QStringLiteral("base.en")},
                        {QStringLiteral("name"), QStringLiteral("Base English")},
                        {QStringLiteral("sizeText"), QStringLiteral("141 MiB")},
                        {QStringLiteral("englishOnly"), true},
                        {QStringLiteral("languageText"), QStringLiteral("English only")},
                        {QStringLiteral("recommended"), true},
                        {QStringLiteral("speed"), QStringLiteral("Fast")},
                        {QStringLiteral("accuracy"), QStringLiteral("Good accuracy")},
                        {QStringLiteral("installed"), false},
                        {QStringLiteral("active"), false},
                        {QStringLiteral("downloading"), false},
                        {QStringLiteral("verifying"), false}}};
  }
  bool busy() const { return false; }
  qreal progress() const { return 0.0; }
  QString status() const { return {}; }
  QString error() const { return {}; }
  QString storagePath() const { return QStringLiteral("/tmp/models"); }
  Q_INVOKABLE void download(const QString &) {}
  Q_INVOKABLE void cancel() {}
  Q_INVOKABLE void selectModel(const QString &) {}
};

class FakeAppController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool idle READ idle NOTIFY stateChanged)
  Q_PROPERTY(bool recording READ recording NOTIFY stateChanged)
  Q_PROPERTY(bool transcribing READ transcribing NOTIFY stateChanged)
  Q_PROPERTY(QString status READ status NOTIFY statusChanged)
  Q_PROPERTY(QString transcript READ transcript NOTIFY transcriptChanged)
  Q_PROPERTY(QString modelPath READ modelPath WRITE setModelPath NOTIFY modelPathChanged)
  Q_PROPERTY(bool modelReady READ modelReady NOTIFY modelReadyChanged)
  Q_PROPERTY(bool modelSetupRequired READ modelSetupRequired NOTIFY modelReadyChanged)
  Q_PROPERTY(QObject *modelManager READ modelManager CONSTANT)
  Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
  Q_PROPERTY(QVariantList availableLanguages READ availableLanguages CONSTANT)
  Q_PROPERTY(bool autoPaste READ autoPaste WRITE setAutoPaste NOTIFY autoPasteChanged)
  Q_PROPERTY(int recordingLimitMinutes READ recordingLimitMinutes WRITE setRecordingLimitMinutes
                 NOTIFY recordingLimitMinutesChanged)
  Q_PROPERTY(qreal level READ level NOTIFY levelChanged)
  Q_PROPERTY(QString shortcutText READ shortcutText CONSTANT)
  Q_PROPERTY(int toggleCount READ toggleCount NOTIFY toggleCountChanged)

public:
  bool idle() const { return !m_recording && !m_transcribing; }
  bool recording() const { return m_recording; }
  bool transcribing() const { return m_transcribing; }
  QString status() const { return m_status; }
  QString transcript() const { return QStringLiteral("Test transcription"); }
  QString modelPath() const { return m_modelPath; }
  bool modelReady() const { return m_modelReady; }
  bool modelSetupRequired() const { return !m_modelReady && !m_restoringModel; }
  QObject *modelManager() { return &m_modelManager; }
  QString language() const { return QStringLiteral("en"); }
  QVariantList availableLanguages() const {
    return {QVariantMap{{QStringLiteral("code"), QStringLiteral("en")},
                        {QStringLiteral("name"), i18n("English")}}};
  }
  bool autoPaste() const { return false; }
  int recordingLimitMinutes() const { return 5; }
  qreal level() const { return 0.5; }
  QString shortcutText() const { return QStringLiteral("Meta+Z"); }
  int toggleCount() const { return m_toggleCount; }

  void setModelPath(const QString &path) {
    if (m_modelPath == path)
      return;
    m_modelPath = path;
    emit modelPathChanged();
  }
  void setLanguage(const QString &) {}
  void setAutoPaste(bool) {}
  void setRecordingLimitMinutes(int) {}

  Q_INVOKABLE void setModelUrl(const QUrl &url) { setModelPath(url.toLocalFile()); }
  Q_INVOKABLE void toggle() {
    ++m_toggleCount;
    emit toggleCountChanged();
  }
  Q_INVOKABLE void forgetTranscript() {}
  Q_INVOKABLE bool removeModel(const QString &) { return true; }
  Q_INVOKABLE void setTestState(bool recording, bool transcribing) {
    m_recording = recording;
    m_transcribing = transcribing;
    m_status = recording      ? QStringLiteral("Listening")
               : transcribing ? QStringLiteral("Transcribing")
                              : QStringLiteral("Ready");
    emit stateChanged();
    emit statusChanged();
  }
  Q_INVOKABLE void setModelReady(bool ready) {
    if (m_modelReady == ready)
      return;
    m_modelReady = ready;
    emit modelReadyChanged();
    if (!ready && !m_restoringModel)
      emit modelSetupRequested();
  }
  Q_INVOKABLE void setRestoringModel(bool restoring) {
    if (m_restoringModel == restoring)
      return;
    m_restoringModel = restoring;
    emit modelReadyChanged();
    if (!restoring && !m_modelReady)
      emit modelSetupRequested();
  }

signals:
  void stateChanged();
  void statusChanged();
  void transcriptChanged();
  void modelPathChanged();
  void languageChanged();
  void autoPasteChanged();
  void recordingLimitMinutesChanged();
  void levelChanged();
  void toggleCountChanged();
  void modelSetupRequested();
  void modelReadyChanged();

private:
  bool m_recording = false;
  bool m_transcribing = false;
  QString m_status = QStringLiteral("Ready");
  QString m_modelPath;
  int m_toggleCount = 0;
  FakeModelManager m_modelManager;
  bool m_modelReady = true;
  bool m_restoringModel = false;
};

class QmlTestSetup final : public QObject {
  Q_OBJECT

public slots:
  void qmlEngineAvailable(QQmlEngine *engine) {
    KLocalizedString::setApplicationDomain("kastword");
    engine->rootContext()->setContextProperty(QStringLiteral("appController"), &m_controller);
    KLocalization::setupLocalizedContext(engine);
  }

private:
  FakeAppController m_controller;
};

QUICK_TEST_MAIN_WITH_SETUP(kastword_qml, QmlTestSetup)

#include "QmlTestSetup.moc"
