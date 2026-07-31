// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AppController.h"

#include <QFile>
#include <QMutex>
#include <QScopeGuard>
#include <QSemaphore>
#include <QSignalSpy>
#include <QStandardPaths>
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

  QString deliveredText;
  bool deliveredWithAutoPaste = false;
  QString result = QStringLiteral("Delivered.");
  bool *destroyed = nullptr;
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
};

void AppControllerTest::initTestCase() { QStandardPaths::setTestModeEnabled(true); }

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
  QVERIFY(transcriptionStarted.tryAcquire(1, 1000));
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

  QCOMPARE(languageChanged.count(), 1);
  QCOMPARE(autoPasteChanged.count(), 1);
}

QTEST_MAIN(AppControllerTest)
#include "AppControllerTest.moc"
