// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <QKeySequence>
#include <QQmlContext>
#include <QQmlEngine>
#include <QUrl>
#include <QVariantList>
#include <QtQuickTest/quicktest.h>

class FakeModelManager final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList models READ models NOTIFY changed)
  Q_PROPERTY(bool busy READ busy CONSTANT)
  Q_PROPERTY(qreal progress READ progress CONSTANT)
  Q_PROPERTY(QString status READ status CONSTANT)
  Q_PROPERTY(QString error READ error CONSTANT)
  Q_PROPERTY(QString storagePath READ storagePath CONSTANT)
  Q_PROPERTY(bool verificationPending READ verificationPending NOTIFY changed)

public:
  QVariantList models() const {
    const auto model = [this](const QString &id, bool englishOnly, bool recommended) {
      return QVariantMap{{QStringLiteral("id"), id},
                         {QStringLiteral("name"), id},
                         {QStringLiteral("url"), QStringLiteral("https://example.test/") + id},
                         {QStringLiteral("sizeText"), QStringLiteral("141 MiB")},
                         {QStringLiteral("englishOnly"), englishOnly},
                         {QStringLiteral("languageText"), englishOnly
                                                              ? QStringLiteral("English only")
                                                              : QStringLiteral("Multilingual")},
                         {QStringLiteral("recommended"), recommended},
                         {QStringLiteral("speed"), QStringLiteral("Fast")},
                         {QStringLiteral("accuracy"), QStringLiteral("Good accuracy")},
                         {QStringLiteral("installed"), id == m_activeId || id == m_installedId},
                         {QStringLiteral("partial"), m_partialId == id},
                         {QStringLiteral("partialSizeText"), QStringLiteral("42 MiB")},
                         {QStringLiteral("active"), id == m_activeId},
                         {QStringLiteral("downloading"), false},
                         {QStringLiteral("verifying"), m_verifyingId == id}};
    };
    return {model(QStringLiteral("base.en"), true, true),
            model(QStringLiteral("small"), false, true),
            model(QStringLiteral("tiny"), false, false)};
  }
  bool busy() const { return false; }
  qreal progress() const { return 0.0; }
  QString status() const { return {}; }
  QString error() const { return {}; }
  QString storagePath() const { return QStringLiteral("/tmp/models"); }
  bool verificationPending() const { return m_verificationPending; }
  void setVerificationPending(bool pending) {
    if (m_verificationPending == pending)
      return;
    m_verificationPending = pending;
    emit changed();
  }
  Q_INVOKABLE void download(const QString &) {}
  Q_INVOKABLE void cancel() {}
  Q_INVOKABLE void selectModel(const QString &) {}
  Q_INVOKABLE void setVerifyingId(const QString &id) {
    if (m_verifyingId == id)
      return;
    m_verifyingId = id;
    emit changed();
  }
  Q_INVOKABLE void setPartialId(const QString &id) {
    if (m_partialId == id)
      return;
    m_partialId = id;
    emit changed();
  }
  Q_INVOKABLE void setModelStates(const QString &activeId, const QString &installedId) {
    if (m_activeId == activeId && m_installedId == installedId)
      return;
    m_activeId = activeId;
    m_installedId = installedId;
    emit changed();
  }

signals:
  void changed();

private:
  bool m_verificationPending = false;
  QString m_verifyingId;
  QString m_partialId;
  QString m_activeId;
  QString m_installedId;
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
  Q_PROPERTY(bool pasteCtrlV READ pasteCtrlV WRITE setPasteCtrlV NOTIFY pasteShortcutsChanged)
  Q_PROPERTY(bool pasteCtrlShiftV READ pasteCtrlShiftV WRITE setPasteCtrlShiftV NOTIFY
                 pasteShortcutsChanged)
  Q_PROPERTY(bool pasteShiftInsert READ pasteShiftInsert WRITE setPasteShiftInsert NOTIFY
                 pasteShortcutsChanged)
  Q_PROPERTY(int recordingLimitMinutes READ recordingLimitMinutes WRITE setRecordingLimitMinutes
                 NOTIFY recordingLimitMinutesChanged)
  Q_PROPERTY(qreal level READ level NOTIFY levelChanged)
  Q_PROPERTY(QVariantList audioInputs READ audioInputs NOTIFY audioInputsChanged)
  Q_PROPERTY(QString audioInputId READ audioInputId WRITE setAudioInputId NOTIFY audioInputsChanged)
  Q_PROPERTY(bool audioInputReady READ audioInputReady NOTIFY audioInputsChanged)
  Q_PROPERTY(QString audioInputStatus READ audioInputStatus NOTIFY audioInputsChanged)
  Q_PROPERTY(bool audioInputSelectionEnabled READ audioInputSelectionEnabled NOTIFY stateChanged)
  Q_PROPERTY(
      bool dictationActionEnabled READ dictationActionEnabled NOTIFY dictationAvailabilityChanged)
  Q_PROPERTY(QKeySequence shortcut READ shortcut NOTIFY shortcutChanged)
  Q_PROPERTY(QString shortcutText READ shortcutText NOTIFY shortcutChanged)
  Q_PROPERTY(int toggleCount READ toggleCount NOTIFY toggleCountChanged)
  Q_PROPERTY(QString copiedText READ copiedText NOTIFY copiedTextChanged)

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
  bool autoPaste() const { return m_autoPaste; }
  bool pasteCtrlV() const { return m_pasteCtrlV; }
  bool pasteCtrlShiftV() const { return m_pasteCtrlShiftV; }
  bool pasteShiftInsert() const { return m_pasteShiftInsert; }
  int recordingLimitMinutes() const { return 5; }
  qreal level() const { return 0.5; }
  QVariantList audioInputs() const {
    QVariantList inputs = {
        QVariantMap{{QStringLiteral("id"), QString()},
                    {QStringLiteral("name"), QStringLiteral("System default (Built-in Mic)")},
                    {QStringLiteral("available"), true}},
        QVariantMap{
            {QStringLiteral("id"), QStringLiteral("usb")},
            {QStringLiteral("name"), !m_usbAvailable && m_audioInputId == QStringLiteral("usb")
                                         ? QStringLiteral("Unavailable selected microphone")
                                         : QStringLiteral("USB Headset")},
            {QStringLiteral("available"), m_usbAvailable}},
    };
    return inputs;
  }
  QString audioInputId() const { return m_audioInputId; }
  bool audioInputReady() const {
    return m_audioInputId.isEmpty() || (m_audioInputId == QStringLiteral("usb") && m_usbAvailable);
  }
  QString audioInputStatus() const {
    if (!audioInputReady())
      return QStringLiteral("The selected microphone is unavailable. Choose another audio input.");
    return m_audioInputId.isEmpty() ? QStringLiteral("Using Built-in Mic")
                                    : QStringLiteral("Using USB Headset");
  }
  bool audioInputSelectionEnabled() const { return !m_recording && !m_transcribing; }
  bool dictationActionEnabled() const {
    return m_recording || (m_modelReady && audioInputReady() && !m_transcribing);
  }
  QKeySequence shortcut() const { return m_shortcut; }
  QString shortcutText() const { return m_shortcut.toString(QKeySequence::NativeText); }
  int toggleCount() const { return m_toggleCount; }
  QString copiedText() const { return m_copiedText; }

  void setModelPath(const QString &path) {
    if (m_modelPath == path)
      return;
    m_modelPath = path;
    emit modelPathChanged();
  }
  void setLanguage(const QString &) {}
  void setAutoPaste(bool value) {
    if (m_autoPaste == value)
      return;
    m_autoPaste = value;
    emit autoPasteChanged();
  }
  void setPasteCtrlV(bool value) { setPasteShortcut(m_pasteCtrlV, value); }
  void setPasteCtrlShiftV(bool value) { setPasteShortcut(m_pasteCtrlShiftV, value); }
  void setPasteShiftInsert(bool value) { setPasteShortcut(m_pasteShiftInsert, value); }
  void setRecordingLimitMinutes(int) {}
  void setAudioInputId(const QString &id) {
    if (!idle() || m_audioInputId == id)
      return;
    m_audioInputId = id;
    emit audioInputsChanged();
    emit dictationAvailabilityChanged();
  }
  Q_INVOKABLE bool setShortcut(const QKeySequence &value) {
    if (!m_shortcutChangeAccepted)
      return false;
    if (m_shortcut == value)
      return true;
    m_shortcut = value;
    emit shortcutChanged();
    return true;
  }
  Q_INVOKABLE void setShortcutChangeAccepted(bool accepted) { m_shortcutChangeAccepted = accepted; }

  Q_INVOKABLE void setModelUrl(const QUrl &url) { setModelPath(url.toLocalFile()); }
  Q_INVOKABLE void toggle() {
    ++m_toggleCount;
    emit toggleCountChanged();
  }
  Q_INVOKABLE void copyText(const QString &text) {
    if (m_copiedText == text)
      return;
    m_copiedText = text;
    emit copiedTextChanged();
  }
  Q_INVOKABLE void forgetTranscript() {}
  Q_INVOKABLE void copyTranscript() {}
  Q_INVOKABLE bool removeModel(const QString &) { return true; }
  Q_INVOKABLE void setTestState(bool recording, bool transcribing) {
    m_recording = recording;
    m_transcribing = transcribing;
    m_status = recording      ? QStringLiteral("Listening")
               : transcribing ? QStringLiteral("Transcribing")
                              : QStringLiteral("Ready");
    emit stateChanged();
    emit dictationAvailabilityChanged();
    emit statusChanged();
  }
  Q_INVOKABLE void setModelReady(bool ready) {
    if (m_modelReady == ready)
      return;
    m_modelReady = ready;
    emit dictationAvailabilityChanged();
    emit modelReadyChanged();
    if (!ready && !m_restoringModel)
      emit modelSetupRequested();
  }
  Q_INVOKABLE void setRestoringModel(bool restoring) {
    if (m_restoringModel == restoring)
      return;
    m_restoringModel = restoring;
    m_modelManager.setVerificationPending(restoring);
    emit modelReadyChanged();
    if (!restoring && !m_modelReady)
      emit modelSetupRequested();
  }
  Q_INVOKABLE void setUsbAudioInputAvailable(bool available) {
    if (m_usbAvailable == available)
      return;
    m_usbAvailable = available;
    emit audioInputsChanged();
    emit dictationAvailabilityChanged();
  }

signals:
  void stateChanged();
  void statusChanged();
  void transcriptChanged();
  void modelPathChanged();
  void languageChanged();
  void autoPasteChanged();
  void pasteShortcutsChanged();
  void recordingLimitMinutesChanged();
  void levelChanged();
  void toggleCountChanged();
  void shortcutChanged();
  void modelSetupRequested();
  void modelReadyChanged();
  void copiedTextChanged();
  void audioInputsChanged();
  void dictationAvailabilityChanged();

private:
  void setPasteShortcut(bool &shortcut, bool value) {
    if (shortcut == value)
      return;
    if (!value && (int(m_pasteCtrlV) + int(m_pasteCtrlShiftV) + int(m_pasteShiftInsert) == 1)) {
      emit pasteShortcutsChanged();
      return;
    }
    shortcut = value;
    emit pasteShortcutsChanged();
  }

  bool m_autoPaste = false;
  bool m_pasteCtrlV = false;
  bool m_pasteCtrlShiftV = false;
  bool m_pasteShiftInsert = true;
  bool m_recording = false;
  bool m_transcribing = false;
  QString m_status = QStringLiteral("Ready");
  QString m_modelPath;
  int m_toggleCount = 0;
  QString m_copiedText;
  QKeySequence m_shortcut{QStringLiteral("Meta+Z")};
  FakeModelManager m_modelManager;
  bool m_modelReady = true;
  bool m_restoringModel = false;
  bool m_shortcutChangeAccepted = true;
  QString m_audioInputId;
  bool m_usbAvailable = true;
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
