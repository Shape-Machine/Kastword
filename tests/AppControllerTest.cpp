// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AppController.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <QBuffer>
#include <QFile>
#include <QMutex>
#include <QScopeGuard>
#include <QSemaphore>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTest>
#include <atomic>
#include <memory>
#include <stdexcept>

class FakeAudioCapture final : public AudioCapture {
public:
  ~FakeAudioCapture() override {
    if (destroyed)
      *destroyed = true;
  }

  bool start(QString *error) override {
    if (!startResult) {
      *error = startError;
      return false;
    }
    recording = true;
    return true;
  }

  QByteArray stop() override {
    recording = false;
    return audio;
  }

  bool isRecording() const override { return recording; }
  void reportLevel(qreal level) { emit levelChanged(level); }
  void fail(const QString &error) {
    recording = false;
    emit captureFailed(error);
  }

  bool startResult = true;
  bool recording = false;
  QString startError = QStringLiteral("Microphone unavailable.");
  QByteArray audio = QByteArrayLiteral("captured audio");
  bool *destroyed = nullptr;
};

class FakeTextOutput final : public TextOutput {
public:
  ~FakeTextOutput() override {
    if (destroyed)
      *destroyed = true;
  }

  QString deliver(const QString &text, bool autoPaste) override {
    deliveredText = text;
    deliveredWithAutoPaste = autoPaste;
    return result;
  }
  void forget(const QString &text) override { forgottenText = text; }
  void reportDeliveryStatus(const QString &status) { emit deliveryStatus(status); }
  void reportDeliveryFailure(const QString &status) {
    emit deliveryStatus(status);
    emit deliveryFailed(status);
  }

  QString deliveredText;
  bool deliveredWithAutoPaste = false;
  QString result = QStringLiteral("Delivered.");
  QString forgottenText;
  bool *destroyed = nullptr;
};

class FakeDesktopIntegration final : public DesktopIntegration {
public:
  void configureShortcut(QAction *action, const QList<QKeySequence> &shortcuts) override {
    configuredAction = action;
    configuredShortcuts = shortcuts;
  }
  QList<QKeySequence> shortcuts(QAction *) const override { return currentShortcuts; }
  void setShortcuts(QAction *, const QList<QKeySequence> &shortcuts, bool autoload) override {
    migratedShortcuts = shortcuts;
    migratedWithAutoload = autoload;
  }
  void cleanShortcutComponent(const QString &component) override { cleanedComponent = component; }
  void showNotification(NotificationKind kind, const QString &title, const QString &text,
                        const QString &iconName, bool persistent) override {
    notifications.append({kind, title, text, iconName, persistent});
  }
  void closeStatusNotification() override { ++closeCount; }

  struct Notification {
    NotificationKind kind;
    QString title;
    QString text;
    QString iconName;
    bool persistent;
  };
  QAction *configuredAction = nullptr;
  QList<QKeySequence> configuredShortcuts;
  QList<QKeySequence> currentShortcuts;
  QList<QKeySequence> migratedShortcuts;
  bool migratedWithAutoload = true;
  QString cleanedComponent;
  QList<Notification> notifications;
  int closeCount = 0;
};

class FakeAudioBackend final : public AudioCaptureBackend {
public:
  explicit FakeAudioBackend(QIODevice *device) : m_device(device) {}
  QIODevice *start() override { return m_device; }
  void stop() override {}
  QAudio::Error error() const override { return currentError; }
  void fail(QAudio::Error error) {
    currentError = error;
    emit stateChanged(QAudio::StoppedState);
  }

  QIODevice *m_device;
  QAudio::Error currentError = QAudio::NoError;
};

class AppControllerTest final : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void init();
  void startsRecording();
  void reportsMicrophoneFailure();
  void recoversFromCaptureFailure();
  void completesDictationFlow();
  void recoversFromTranscriptionFailure();
  void ignoresEmptyTranscription();
  void recoversFromTranscriptionException();
  void serializesTranscriptionAndSnapshotsSettings();
  void ownsInjectedDependencies();
  void rejectsToggleWhileTranscribing();
  void forwardsAudioLevel();
  void emitsSettingChangesOnlyWhenValuesChange();
  void defaultsToPrivateOutputAndConfigurableRecordingLimit();
  void copiesTranscript();
  void forgetsTranscript();
  void clampsRecordingLimit();
  void reportsAsynchronousDeliveryStatus();
  void boundsCapturedAudioBuffer();
  void convertsModelUrlsToLocalPaths();
  void supportsMultilingualModels();
  void migratesUnsupportedLanguageSetting();
  void disablesDictationUntilModelIsReady();
  void restrictsLanguageForLocalEnglishOnlyModel();
  void stopsRecordingWhenActiveModelDisappears();
  void reportsModelVerificationAndReadiness();
  void configuresDesktopIntegrationAndReportsFailures();
  void reportsAutomaticPasteFailuresToDesktop_data();
  void reportsAutomaticPasteFailuresToDesktop();
  void presentsTrayStates_data();
  void presentsTrayStates();
  void reportsInjectedAudioDiscoveryAndBackendErrors_data();
  void reportsInjectedAudioDiscoveryAndBackendErrors();
  void activatesAndTogglesApplicationWindow();
};

void AppControllerTest::initTestCase() {
  KLocalizedString::setApplicationDomain("kastword");
  QStandardPaths::setTestModeEnabled(true);
}

void AppControllerTest::init() {
  QFile::remove(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) +
                QStringLiteral("/kastwordrc"));
}

void AppControllerTest::startsRecording() {
  auto audio = std::make_unique<FakeAudioCapture>();
  auto output = std::make_unique<FakeTextOutput>();
  auto *audioPtr = audio.get();
  AppController controller(
      std::move(audio), std::move(output),
      [](const QByteArray &, const QString &, const QString &) {
        return QPair<QString, QString>();
      },
      false);
  QSignalSpy stateChanged(&controller, &AppController::stateChanged);

  controller.toggle();

  QCOMPARE(controller.state(), AppController::State::Recording);
  QVERIFY(audioPtr->recording);
  QCOMPARE(stateChanged.count(), 1);
  QVERIFY(controller.status().contains(controller.shortcutText()));
}

void AppControllerTest::reportsMicrophoneFailure() {
  auto audio = std::make_unique<FakeAudioCapture>();
  audio->startResult = false;
  auto output = std::make_unique<FakeTextOutput>();
  auto *audioPtr = audio.get();
  AppController controller(
      std::move(audio), std::move(output),
      [](const QByteArray &, const QString &, const QString &) {
        return QPair<QString, QString>();
      },
      false);

  controller.toggle();

  QCOMPARE(controller.state(), AppController::State::Idle);
  QCOMPARE(controller.status(), audioPtr->startError);
  QVERIFY(!audioPtr->recording);
}

void AppControllerTest::recoversFromCaptureFailure() {
  auto audio = std::make_unique<FakeAudioCapture>();
  auto output = std::make_unique<FakeTextOutput>();
  auto *audioPtr = audio.get();
  AppController controller(
      std::move(audio), std::move(output),
      [](const QByteArray &, const QString &, const QString &) {
        return QPair<QString, QString>();
      },
      false);

  controller.toggle();
  audioPtr->reportLevel(0.75);
  audioPtr->fail(QStringLiteral("The microphone was disconnected."));

  QCOMPARE(controller.state(), AppController::State::Idle);
  QCOMPARE(controller.status(), QStringLiteral("The microphone was disconnected."));
  QCOMPARE(controller.level(), 0.0);
}

void AppControllerTest::completesDictationFlow() {
  auto audio = std::make_unique<FakeAudioCapture>();
  auto output = std::make_unique<FakeTextOutput>();
  auto *outputPtr = output.get();
  AppController controller(
      std::move(audio), std::move(output),
      [](const QByteArray &audioData, const QString &, const QString &) {
        if (audioData != QByteArrayLiteral("captured audio"))
          return qMakePair(QString(), QStringLiteral("Unexpected test audio."));
        return qMakePair(QStringLiteral("hello world"), QString());
      },
      false);
  controller.setAutoPaste(true);
  QSignalSpy transcriptChanged(&controller, &AppController::transcriptChanged);

  controller.toggle();
  controller.toggle();

  QTRY_COMPARE(controller.state(), AppController::State::Success);
  QCOMPARE(controller.transcript(), QStringLiteral("hello world"));
  QCOMPARE(outputPtr->deliveredText, QStringLiteral("hello world"));
  QVERIFY(outputPtr->deliveredWithAutoPaste);
  QCOMPARE(controller.status(), outputPtr->result);
  QCOMPARE(transcriptChanged.count(), 1);
  QTRY_COMPARE_WITH_TIMEOUT(controller.state(), AppController::State::Idle, 2000);
}

void AppControllerTest::recoversFromTranscriptionFailure() {
  auto audio = std::make_unique<FakeAudioCapture>();
  auto output = std::make_unique<FakeTextOutput>();
  auto *outputPtr = output.get();
  AppController controller(
      std::move(audio), std::move(output),
      [](const QByteArray &, const QString &, const QString &) {
        return qMakePair(QString(), QStringLiteral("Transcription failed deterministically."));
      },
      false);

  controller.toggle();
  controller.toggle();

  QTRY_COMPARE(controller.state(), AppController::State::Idle);
  QCOMPARE(controller.status(), QStringLiteral("Transcription failed deterministically."));
  QVERIFY(outputPtr->deliveredText.isEmpty());
}

void AppControllerTest::ignoresEmptyTranscription() {
  auto audio = std::make_unique<FakeAudioCapture>();
  auto output = std::make_unique<FakeTextOutput>();
  auto *outputPtr = output.get();
  AppController controller(
      std::move(audio), std::move(output),
      [](const QByteArray &, const QString &, const QString &) {
        return qMakePair(QStringLiteral("   "), QString());
      },
      false);

  controller.toggle();
  controller.toggle();

  QTRY_COMPARE(controller.state(), AppController::State::Idle);
  QCOMPARE(controller.status(), QStringLiteral("No speech detected."));
  QVERIFY(controller.transcript().isEmpty());
  QVERIFY(outputPtr->deliveredText.isEmpty());
}

void AppControllerTest::recoversFromTranscriptionException() {
  auto audio = std::make_unique<FakeAudioCapture>();
  auto output = std::make_unique<FakeTextOutput>();
  auto *outputPtr = output.get();
  AppController controller(
      std::move(audio), std::move(output),
      [](const QByteArray &, const QString &, const QString &) -> QPair<QString, QString> {
        throw std::runtime_error("deterministic worker failure");
      },
      false);

  controller.toggle();
  controller.toggle();

  QTRY_COMPARE(controller.state(), AppController::State::Idle);
  QVERIFY(controller.status().contains(QStringLiteral("deterministic worker failure")));
  QVERIFY(outputPtr->deliveredText.isEmpty());
}

void AppControllerTest::serializesTranscriptionAndSnapshotsSettings() {
  std::atomic_bool firstStarted = false;
  QSemaphore allowFirstCompletion;
  QMutex resultsMutex;
  QStringList models;
  QList<QThread *> threads;
  auto audio = std::make_unique<FakeAudioCapture>();
  auto output = std::make_unique<FakeTextOutput>();
  AppController controller(
      std::move(audio), std::move(output),
      [&firstStarted, &allowFirstCompletion, &resultsMutex, &models,
       &threads](const QByteArray &, const QString &model, const QString &) {
        QMutexLocker locker(&resultsMutex);
        models.append(model);
        threads.append(QThread::currentThread());
        const bool firstCall = models.size() == 1;
        locker.unlock();
        if (firstCall) {
          firstStarted.store(true);
          allowFirstCompletion.acquire();
        }
        return qMakePair(QStringLiteral("dictation"), QString());
      },
      false);
  auto unblockWorker = qScopeGuard([&allowFirstCompletion] { allowFirstCompletion.release(); });
  controller.setModelPath(QStringLiteral("first-model.bin"));

  controller.toggle();
  controller.toggle();
  QTRY_VERIFY_WITH_TIMEOUT(firstStarted.load(), 5000);
  controller.setModelPath(QStringLiteral("second-model.bin"));
  allowFirstCompletion.release();
  unblockWorker.dismiss();
  QTRY_COMPARE(controller.state(), AppController::State::Success);

  controller.toggle();
  controller.toggle();
  QTRY_COMPARE(controller.state(), AppController::State::Success);

  QMutexLocker locker(&resultsMutex);
  QCOMPARE(models.size(), 2);
  QCOMPARE(models,
           QStringList({QStringLiteral("first-model.bin"), QStringLiteral("second-model.bin")}));
  QCOMPARE(threads.at(0), threads.at(1));
  QVERIFY(threads.at(0) != QThread::currentThread());
}

void AppControllerTest::ownsInjectedDependencies() {
  bool audioDestroyed = false;
  bool outputDestroyed = false;
  {
    auto audio = std::make_unique<FakeAudioCapture>();
    auto output = std::make_unique<FakeTextOutput>();
    audio->destroyed = &audioDestroyed;
    output->destroyed = &outputDestroyed;
    AppController controller(
        std::move(audio), std::move(output),
        [](const QByteArray &, const QString &, const QString &) {
          return QPair<QString, QString>();
        },
        false);
  }

  QVERIFY(audioDestroyed);
  QVERIFY(outputDestroyed);
}

void AppControllerTest::rejectsToggleWhileTranscribing() {
  QSemaphore transcriptionStarted;
  QSemaphore allowCompletion;
  auto audio = std::make_unique<FakeAudioCapture>();
  auto output = std::make_unique<FakeTextOutput>();
  AppController controller(
      std::move(audio), std::move(output),
      [&transcriptionStarted, &allowCompletion](const QByteArray &, const QString &,
                                                const QString &) {
        transcriptionStarted.release();
        allowCompletion.acquire();
        return qMakePair(QStringLiteral("done"), QString());
      },
      false);

  controller.toggle();
  controller.toggle();
  QTRY_VERIFY(transcriptionStarted.available() > 0);
  transcriptionStarted.acquire();
  controller.toggle();

  QCOMPARE(controller.state(), AppController::State::Transcribing);
  QCOMPARE(controller.status(), QStringLiteral("Transcription is already in progress."));
  allowCompletion.release();
  QTRY_COMPARE(controller.state(), AppController::State::Success);
}

void AppControllerTest::forwardsAudioLevel() {
  auto audio = std::make_unique<FakeAudioCapture>();
  auto output = std::make_unique<FakeTextOutput>();
  auto *audioPtr = audio.get();
  AppController controller(
      std::move(audio), std::move(output),
      [](const QByteArray &, const QString &, const QString &) {
        return QPair<QString, QString>();
      },
      false);
  QSignalSpy levelChanged(&controller, &AppController::levelChanged);

  audioPtr->reportLevel(0.625);

  QCOMPARE(controller.level(), 0.625);
  QCOMPARE(levelChanged.count(), 1);
}

void AppControllerTest::emitsSettingChangesOnlyWhenValuesChange() {
  auto audio = std::make_unique<FakeAudioCapture>();
  auto output = std::make_unique<FakeTextOutput>();
  AppController controller(
      std::move(audio), std::move(output),
      [](const QByteArray &, const QString &, const QString &) {
        return QPair<QString, QString>();
      },
      false);
  QSignalSpy languageChanged(&controller, &AppController::languageChanged);
  QSignalSpy autoPasteChanged(&controller, &AppController::autoPasteChanged);

  controller.setLanguage(QStringLiteral("nl"));
  controller.setLanguage(QStringLiteral("nl"));
  controller.setAutoPaste(!controller.autoPaste());

  QCOMPARE(controller.language(), QStringLiteral("nl"));
  QCOMPARE(languageChanged.count(), 1);
  QCOMPARE(autoPasteChanged.count(), 1);
}

void AppControllerTest::defaultsToPrivateOutputAndConfigurableRecordingLimit() {
  auto audio = std::make_unique<FakeAudioCapture>();
  auto output = std::make_unique<FakeTextOutput>();
  AppController controller(
      std::move(audio), std::move(output),
      [](const QByteArray &, const QString &, const QString &) {
        return QPair<QString, QString>();
      },
      false);
  QSignalSpy limitChanged(&controller, &AppController::recordingLimitMinutesChanged);

  QVERIFY(!controller.autoPaste());
  QCOMPARE(controller.recordingLimitMinutes(), 5);
  controller.setRecordingLimitMinutes(12);
  QCOMPARE(controller.recordingLimitMinutes(), 12);
  QCOMPARE(limitChanged.count(), 1);
}

void AppControllerTest::forgetsTranscript() {
  auto audio = std::make_unique<FakeAudioCapture>();
  auto output = std::make_unique<FakeTextOutput>();
  auto *outputPtr = output.get();
  AppController controller(
      std::move(audio), std::move(output),
      [](const QByteArray &, const QString &, const QString &) {
        return qMakePair(QStringLiteral("sensitive text"), QString());
      },
      false);
  controller.toggle();
  controller.toggle();
  QTRY_COMPARE(controller.transcript(), QStringLiteral("sensitive text"));

  controller.forgetTranscript();

  QCOMPARE(outputPtr->forgottenText, QStringLiteral("sensitive text"));
  QVERIFY(controller.transcript().isEmpty());
  QCOMPARE(controller.status(), QStringLiteral("Cleared from Kastword and matching current "
                                               "clipboards. Clipboard history may retain it."));
}

void AppControllerTest::copiesTranscript() {
  auto audio = std::make_unique<FakeAudioCapture>();
  auto output = std::make_unique<FakeTextOutput>();
  auto *outputPtr = output.get();
  AppController controller(
      std::move(audio), std::move(output),
      [](const QByteArray &, const QString &, const QString &) {
        return qMakePair(QStringLiteral("copy me"), QString());
      },
      false);
  controller.toggle();
  controller.toggle();
  QTRY_COMPARE(controller.transcript(), QStringLiteral("copy me"));
  outputPtr->deliveredText.clear();

  controller.copyTranscript();

  QCOMPARE(outputPtr->deliveredText, QStringLiteral("copy me"));
  QVERIFY(!outputPtr->deliveredWithAutoPaste);
  QCOMPARE(controller.status(), QStringLiteral("Delivered."));
}

void AppControllerTest::clampsRecordingLimit() {
  auto audio = std::make_unique<FakeAudioCapture>();
  auto output = std::make_unique<FakeTextOutput>();
  AppController controller(
      std::move(audio), std::move(output),
      [](const QByteArray &, const QString &, const QString &) {
        return QPair<QString, QString>();
      },
      false);
  QSignalSpy changed(&controller, &AppController::recordingLimitMinutesChanged);

  controller.setRecordingLimitMinutes(0);
  QCOMPARE(controller.recordingLimitMinutes(), 1);
  controller.setRecordingLimitMinutes(100);
  QCOMPARE(controller.recordingLimitMinutes(), 60);
  controller.setRecordingLimitMinutes(60);
  QCOMPARE(changed.count(), 2);
}

void AppControllerTest::reportsAsynchronousDeliveryStatus() {
  auto audio = std::make_unique<FakeAudioCapture>();
  auto output = std::make_unique<FakeTextOutput>();
  auto *outputPtr = output.get();
  AppController controller(
      std::move(audio), std::move(output),
      [](const QByteArray &, const QString &, const QString &) {
        return QPair<QString, QString>();
      },
      false);
  QSignalSpy changed(&controller, &AppController::statusChanged);

  outputPtr->reportDeliveryStatus(QStringLiteral("Automatic paste helper crashed."));

  QCOMPARE(controller.status(), QStringLiteral("Automatic paste helper crashed."));
  QCOMPARE(changed.count(), 1);
}

void AppControllerTest::boundsCapturedAudioBuffer() {
  QAudioFormat format;
  format.setSampleRate(16000);
  format.setChannelCount(1);
  format.setSampleFormat(QAudioFormat::Int16);
  CapturedAudioBuffer buffer;
  buffer.configure(format, 1);
  const qsizetype limit = 16000 * qsizetype(sizeof(qint16));

  QVERIFY(buffer.append(QByteArray(limit, '\0')));
  QCOMPARE(buffer.size(), limit);
  QVERIFY(!buffer.append(QByteArray(1, '\0')));
  QCOMPARE(buffer.size(), qsizetype(0));

  QVERIFY(buffer.append(QByteArray(1600 * qsizetype(sizeof(qint16)), '\0')));
  QVERIFY(!buffer.takeForWhisper().isEmpty());
  QCOMPARE(buffer.size(), qsizetype(0));
}

void AppControllerTest::convertsModelUrlsToLocalPaths() {
  auto audio = std::make_unique<FakeAudioCapture>();
  auto output = std::make_unique<FakeTextOutput>();
  AppController controller(
      std::move(audio), std::move(output),
      [](const QByteArray &, const QString &, const QString &) {
        return QPair<QString, QString>();
      },
      false);

  const QStringList paths = {QStringLiteral("/tmp/My Model.bin"),
                             QStringLiteral("/tmp/100% model.bin"),
                             QStringLiteral("/tmp/日本語モデル.bin")};
  for (const QString &path : paths) {
    controller.setModelUrl(QUrl::fromLocalFile(path));
    QCOMPARE(controller.modelPath(), path);
  }

  controller.setModelUrl(QUrl(QStringLiteral("https://example.test/model.bin")));
  QCOMPARE(controller.modelPath(), paths.constLast());
}

void AppControllerTest::supportsMultilingualModels() {
  auto audio = std::make_unique<FakeAudioCapture>();
  auto output = std::make_unique<FakeTextOutput>();
  AppController controller(
      std::move(audio), std::move(output),
      [](const QByteArray &, const QString &, const QString &) {
        return QPair<QString, QString>();
      },
      false);

  const QVariantList languages = controller.availableLanguages();
  QCOMPARE(languages.size(), 8);
  QCOMPARE(languages.constFirst().toMap().value(QStringLiteral("code")).toString(),
           QStringLiteral("en"));
  QCOMPARE(languages.constFirst().toMap().value(QStringLiteral("name")).toString(),
           QStringLiteral("English"));

  controller.setLanguage(QStringLiteral("nl"));
  QCOMPARE(controller.language(), QStringLiteral("nl"));
  controller.setLanguage(QStringLiteral("unsupported"));
  QCOMPARE(controller.language(), QStringLiteral("en"));
}

void AppControllerTest::migratesUnsupportedLanguageSetting() {
  {
    KConfig config(QStringLiteral("kastwordrc"));
    KConfigGroup group(&config, QStringLiteral("General"));
    group.writeEntry("Language", QStringLiteral("unsupported"));
    group.sync();
  }

  auto audio = std::make_unique<FakeAudioCapture>();
  auto output = std::make_unique<FakeTextOutput>();
  AppController controller(
      std::move(audio), std::move(output),
      [](const QByteArray &, const QString &, const QString &) {
        return QPair<QString, QString>();
      },
      false);

  QCOMPARE(controller.language(), QStringLiteral("en"));
  KConfig config(QStringLiteral("kastwordrc"));
  const KConfigGroup group(&config, QStringLiteral("General"));
  QCOMPARE(group.readEntry("Language", QString()), QStringLiteral("en"));
}

void AppControllerTest::disablesDictationUntilModelIsReady() {
  QTemporaryDir directory;
  auto network = std::make_unique<QNetworkAccessManager>();
  auto manager = std::make_unique<ModelManager>(
      QList<ModelCatalogEntry>{}, directory.filePath(QStringLiteral("managed")), network.get(),
      nullptr, [](const QString &) { return ModelManager::ModelValidationResult{true, false}; });
  auto audio = std::make_unique<FakeAudioCapture>();
  auto *audioPtr = audio.get();
  auto output = std::make_unique<FakeTextOutput>();
  AppController controller(
      std::move(audio), std::move(output),
      [](const QByteArray &, const QString &, const QString &) {
        return QPair<QString, QString>();
      },
      false, true, nullptr, std::move(manager));

  QVERIFY(!controller.modelReady());
  QVERIFY(controller.modelSetupRequired());
  QVERIFY(!controller.shortcutAction()->isEnabled());
  controller.toggle();
  QVERIFY(!audioPtr->recording);
  QCOMPARE(controller.status(), QStringLiteral("Choose a speech model before starting dictation."));

  QTemporaryFile model;
  QVERIFY(model.open());
  QCOMPARE(model.write(QByteArray::fromHex("6c6d6767") + QByteArrayLiteral("test-model")), 14);
  model.flush();
  controller.setModelUrl(QUrl::fromLocalFile(model.fileName()));

  QTRY_VERIFY(controller.modelReady());
  QVERIFY(controller.shortcutAction()->isEnabled());
  controller.toggle();
  QVERIFY(audioPtr->recording);
}

void AppControllerTest::restrictsLanguageForLocalEnglishOnlyModel() {
  QTemporaryDir directory;
  auto network = std::make_unique<QNetworkAccessManager>();
  auto manager = std::make_unique<ModelManager>(
      QList<ModelCatalogEntry>{}, directory.filePath(QStringLiteral("managed")), network.get(),
      nullptr, [](const QString &) { return ModelManager::ModelValidationResult{true, true}; });
  auto audio = std::make_unique<FakeAudioCapture>();
  auto output = std::make_unique<FakeTextOutput>();
  AppController controller(
      std::move(audio), std::move(output),
      [](const QByteArray &, const QString &, const QString &) {
        return QPair<QString, QString>();
      },
      false, true, nullptr, std::move(manager));
  controller.setLanguage(QStringLiteral("de"));
  QFile model(directory.filePath(QStringLiteral("custom.en.bin")));
  QVERIFY(model.open(QIODevice::WriteOnly));
  QCOMPARE(model.write(QByteArray::fromHex("6c6d6767") + QByteArrayLiteral("test-model")), 14);
  model.close();

  controller.setModelUrl(QUrl::fromLocalFile(model.fileName()));

  QTRY_VERIFY(controller.modelReady());
  QCOMPARE(controller.language(), QStringLiteral("en"));
  QCOMPARE(controller.availableLanguages().size(), 1);
}

void AppControllerTest::stopsRecordingWhenActiveModelDisappears() {
  QTemporaryDir directory;
  const QString modelPath = directory.filePath(QStringLiteral("custom.bin"));
  QFile model(modelPath);
  QVERIFY(model.open(QIODevice::WriteOnly));
  QCOMPARE(model.write(QByteArray::fromHex("6c6d6767") + QByteArrayLiteral("test-model")), 14);
  model.close();
  {
    KConfig config(QStringLiteral("kastwordrc"));
    KConfigGroup group(&config, QStringLiteral("General"));
    group.writeEntry("ModelPath", modelPath);
    group.sync();
  }
  auto network = std::make_unique<QNetworkAccessManager>();
  auto manager = std::make_unique<ModelManager>(
      QList<ModelCatalogEntry>{}, directory.filePath(QStringLiteral("managed")), network.get(),
      nullptr, [](const QString &) { return ModelManager::ModelValidationResult{true, false}; });
  auto audio = std::make_unique<FakeAudioCapture>();
  auto *audioPtr = audio.get();
  auto output = std::make_unique<FakeTextOutput>();
  AppController controller(
      std::move(audio), std::move(output),
      [](const QByteArray &, const QString &, const QString &) {
        return QPair<QString, QString>();
      },
      false, true, nullptr, std::move(manager));
  QTRY_VERIFY(controller.modelReady());
  controller.toggle();
  QVERIFY(audioPtr->recording);

  QVERIFY(QFile::remove(modelPath));

  QTRY_VERIFY(!audioPtr->recording);
  QTRY_VERIFY(controller.isIdle());
  QVERIFY(!controller.modelReady());
}

void AppControllerTest::reportsModelVerificationAndReadiness() {
  QTemporaryDir directory;
  const QString modelPath = directory.filePath(QStringLiteral("custom.bin"));
  QFile model(modelPath);
  QVERIFY(model.open(QIODevice::WriteOnly));
  QCOMPARE(model.write(QByteArray::fromHex("6c6d6767") + QByteArrayLiteral("test-model")), 14);
  model.close();
  {
    KConfig config(QStringLiteral("kastwordrc"));
    KConfigGroup group(&config, QStringLiteral("General"));
    group.writeEntry("ModelPath", modelPath);
    group.sync();
  }
  QSemaphore validationGate;
  auto network = std::make_unique<QNetworkAccessManager>();
  auto manager = std::make_unique<ModelManager>(
      QList<ModelCatalogEntry>{}, directory.filePath(QStringLiteral("managed")), network.get(),
      nullptr, [&validationGate](const QString &) {
        validationGate.acquire();
        return ModelManager::ModelValidationResult{true, false};
      });
  auto audio = std::make_unique<FakeAudioCapture>();
  auto output = std::make_unique<FakeTextOutput>();
  AppController controller(
      std::move(audio), std::move(output),
      [](const QByteArray &, const QString &, const QString &) {
        return QPair<QString, QString>();
      },
      false, true, nullptr, std::move(manager));
  const auto releaseValidation = qScopeGuard([&validationGate] { validationGate.release(); });

  QCOMPARE(controller.status(), QStringLiteral("Verifying the speech model…"));
  QVERIFY(!controller.modelReady());
  validationGate.release();
  QTRY_VERIFY(controller.modelReady());
  QCOMPARE(controller.status(), QStringLiteral("Ready"));
}

void AppControllerTest::configuresDesktopIntegrationAndReportsFailures() {
  auto audio = std::make_unique<FakeAudioCapture>();
  auto *audioPtr = audio.get();
  auto output = std::make_unique<FakeTextOutput>();
  auto desktop = std::make_unique<FakeDesktopIntegration>();
  auto *desktopPtr = desktop.get();
  AppController controller(
      std::move(audio), std::move(output),
      [](const QByteArray &, const QString &, const QString &) {
        return QPair<QString, QString>();
      },
      true, false, nullptr, {}, std::move(desktop));

  QCOMPARE(desktopPtr->configuredAction, controller.shortcutAction());
  QCOMPARE(desktopPtr->configuredShortcuts,
           QList<QKeySequence>{QKeySequence(QStringLiteral("Meta+Z"))});
  QCOMPARE(desktopPtr->cleanedComponent, QStringLiteral("Kastword"));

  controller.toggle();
  QCOMPARE(desktopPtr->notifications.size(), 1);
  QCOMPARE(desktopPtr->notifications.constLast().title, QStringLiteral("Dictation started"));
  QVERIFY(desktopPtr->notifications.constLast().persistent);

  audioPtr->fail(QStringLiteral("Deterministic backend failure."));
  QCOMPARE(desktopPtr->closeCount, 2);
  QCOMPARE(desktopPtr->notifications.size(), 2);
  QCOMPARE(desktopPtr->notifications.constLast().kind, DesktopIntegration::NotificationKind::Error);
  QCOMPARE(desktopPtr->notifications.constLast().text,
           QStringLiteral("Deterministic backend failure."));
}

void AppControllerTest::reportsAutomaticPasteFailuresToDesktop_data() {
  QTest::addColumn<QString>("status");
  QTest::newRow("failed to start")
      << QStringLiteral("Automatic paste helper could not be started.");
  QTest::newRow("crashed") << QStringLiteral("Automatic paste helper crashed.");
  QTest::newRow("process error") << QStringLiteral("Automatic paste helper encountered an error.");
  QTest::newRow("nonzero exit") << QStringLiteral(
      "Automatic paste helper failed with exit code 9.");
}

void AppControllerTest::reportsAutomaticPasteFailuresToDesktop() {
  QFETCH(QString, status);
  auto audio = std::make_unique<FakeAudioCapture>();
  auto output = std::make_unique<FakeTextOutput>();
  auto *outputPtr = output.get();
  auto desktop = std::make_unique<FakeDesktopIntegration>();
  auto *desktopPtr = desktop.get();
  AppController controller(
      std::move(audio), std::move(output),
      [](const QByteArray &, const QString &, const QString &) {
        return QPair<QString, QString>();
      },
      true, false, nullptr, {}, std::move(desktop));

  outputPtr->reportDeliveryFailure(status);

  QCOMPARE(controller.status(), status);
  QCOMPARE(desktopPtr->notifications.size(), 1);
  QCOMPARE(desktopPtr->notifications.constFirst().kind,
           DesktopIntegration::NotificationKind::Error);
  QCOMPARE(desktopPtr->notifications.constFirst().title, QStringLiteral("Automatic paste failed"));
  QCOMPARE(desktopPtr->notifications.constFirst().text, status);
}

void AppControllerTest::presentsTrayStates_data() {
  QTest::addColumn<int>("state");
  QTest::addColumn<bool>("modelReady");
  QTest::addColumn<QString>("icon");
  QTest::addColumn<QString>("text");
  QTest::addColumn<bool>("enabled");

  using State = AppController::State;
  QTest::newRow("idle ready") << int(State::Idle) << true
                              << QStringLiteral("audio-input-microphone")
                              << QStringLiteral("Start Dictation") << true;
  QTest::newRow("idle unavailable")
      << int(State::Idle) << false << QStringLiteral("audio-input-microphone")
      << QStringLiteral("Start Dictation") << false;
  QTest::newRow("recording") << int(State::Recording) << true << QStringLiteral("media-record")
                             << QStringLiteral("Stop and Transcribe") << true;
  QTest::newRow("transcribing") << int(State::Transcribing) << true
                                << QStringLiteral("view-refresh") << QStringLiteral("Transcribing…")
                                << false;
  QTest::newRow("success") << int(State::Success) << true << QStringLiteral("dialog-ok-apply")
                           << QStringLiteral("Start Dictation") << true;
}

void AppControllerTest::presentsTrayStates() {
  QFETCH(int, state);
  QFETCH(bool, modelReady);
  QFETCH(QString, icon);
  QFETCH(QString, text);
  QFETCH(bool, enabled);

  const TrayPresentation presentation = trayPresentation(state, modelReady);
  QCOMPARE(presentation.iconName, icon);
  QCOMPARE(presentation.actionText, text);
  QCOMPARE(presentation.actionEnabled, enabled);
}

void AppControllerTest::reportsInjectedAudioDiscoveryAndBackendErrors_data() {
  QTest::addColumn<QAudio::Error>("error");
  QTest::addColumn<QString>("message");
  QTest::newRow("open") << QAudio::OpenError
                        << QStringLiteral("The microphone could not be opened.");
  QTest::newRow("io") << QAudio::IOError << QStringLiteral("The microphone stopped responding.");
  QTest::newRow("fatal") << QAudio::FatalError << QStringLiteral("The microphone backend failed.");
  QTest::newRow("unknown") << QAudio::NoError
                           << QStringLiteral("Microphone capture stopped unexpectedly.");
}

void AppControllerTest::reportsInjectedAudioDiscoveryAndBackendErrors() {
  QFETCH(QAudio::Error, error);
  QFETCH(QString, message);
  QCOMPARE(AudioCapture::errorMessageFor(error), message);

  AudioCapture capture([] { return QAudioDevice(); });
  QString startError;
  QVERIFY(!capture.start(&startError));
  QCOMPARE(startError, QStringLiteral("No microphone is available."));

  AudioCapture invalidFormatCapture(QAudioFormat(), [](const QAudioDevice &, const QAudioFormat &) {
    return std::unique_ptr<AudioCaptureBackend>();
  });
  QVERIFY(!invalidFormatCapture.start(&startError));
  QCOMPARE(startError, QStringLiteral("The microphone reported an invalid audio format."));

  QAudioFormat format;
  format.setSampleRate(16000);
  format.setChannelCount(1);
  format.setSampleFormat(QAudioFormat::Int16);
  AudioCapture missingFactoryCapture(format, AudioCapture::BackendFactory());
  QVERIFY(!missingFactoryCapture.start(&startError));
  QCOMPARE(startError, QStringLiteral("Could not start microphone capture."));
  AudioCapture nullBackendCapture(format, [](const QAudioDevice &, const QAudioFormat &) {
    return std::unique_ptr<AudioCaptureBackend>();
  });
  QVERIFY(!nullBackendCapture.start(&startError));
  QCOMPARE(startError, QStringLiteral("Could not start microphone capture."));

  QBuffer input;
  FakeAudioBackend *backend = nullptr;
  AudioCapture backendCapture(format,
                              [&backend, &input](const QAudioDevice &, const QAudioFormat &) {
                                auto created = std::make_unique<FakeAudioBackend>(&input);
                                backend = created.get();
                                return created;
                              });
  QSignalSpy failure(&backendCapture, &AudioCapture::captureFailed);
  QVERIFY(backendCapture.start(&startError));
  backend->fail(error);
  if (error == QAudio::NoError) {
    QCoreApplication::processEvents();
    QCOMPARE(failure.count(), 0);
    QVERIFY(backendCapture.isRecording());
  } else {
    QTRY_COMPARE(failure.count(), 1);
    QCOMPARE(failure.constFirst().constFirst().toString(), message);
    QVERIFY(!backendCapture.isRecording());
  }
}

void AppControllerTest::activatesAndTogglesApplicationWindow() {
  bool visible = false;
  int showCount = 0;
  int hideCount = 0;
  int raiseCount = 0;
  int requestCount = 0;
  const WindowActivation window = {
      [&visible] { return visible; },
      [&visible, &hideCount] {
        visible = false;
        ++hideCount;
      },
      [&visible, &showCount] {
        visible = true;
        ++showCount;
      },
      [&raiseCount] { ++raiseCount; },
      [&requestCount] { ++requestCount; },
  };

  activateWindow(window, false);
  QVERIFY(visible);
  QCOMPARE(showCount, 1);
  QCOMPARE(raiseCount, 1);
  QCOMPARE(requestCount, 1);

  activateWindow(window, true);
  QVERIFY(!visible);
  QCOMPARE(hideCount, 1);

  activateWindow(window, true);
  QVERIFY(visible);
  QCOMPARE(showCount, 2);
  QCOMPARE(raiseCount, 2);
  QCOMPARE(requestCount, 2);
}

QTEST_MAIN(AppControllerTest)
#include "AppControllerTest.moc"
