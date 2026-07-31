// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AppController.h"

#include <QFile>
#include <QSemaphore>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

class FakeAudioCapture final : public AudioCapture {
public:
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

  bool startResult = true;
  bool recording = false;
  QString startError = QStringLiteral("Microphone unavailable.");
  QByteArray audio = QByteArrayLiteral("captured audio");
};

class FakeTextOutput final : public TextOutput {
public:
  QString deliver(const QString &text, bool autoPaste) override {
    deliveredText = text;
    deliveredWithAutoPaste = autoPaste;
    return result;
  }

  QString deliveredText;
  bool deliveredWithAutoPaste = false;
  QString result = QStringLiteral("Delivered.");
};

class AppControllerTest final : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void init();
  void startsRecording();
  void reportsMicrophoneFailure();
  void completesDictationFlow();
  void recoversFromTranscriptionFailure();
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
  auto *audio = new FakeAudioCapture;
  auto *output = new FakeTextOutput;
  AppController controller(
      audio, output,
      [](const QByteArray &, const QString &, const QString &) {
        return QPair<QString, QString>();
      },
      false);
  QSignalSpy stateChanged(&controller, &AppController::stateChanged);

  controller.toggle();

  QCOMPARE(controller.state(), QStringLiteral("recording"));
  QVERIFY(audio->recording);
  QCOMPARE(stateChanged.count(), 1);
  QVERIFY(controller.status().contains(QStringLiteral("Meta+Z")));
}

void AppControllerTest::reportsMicrophoneFailure() {
  auto *audio = new FakeAudioCapture;
  audio->startResult = false;
  auto *output = new FakeTextOutput;
  AppController controller(
      audio, output,
      [](const QByteArray &, const QString &, const QString &) {
        return QPair<QString, QString>();
      },
      false);

  controller.toggle();

  QCOMPARE(controller.state(), QStringLiteral("idle"));
  QCOMPARE(controller.status(), audio->startError);
  QVERIFY(!audio->recording);
}

void AppControllerTest::completesDictationFlow() {
  auto *audio = new FakeAudioCapture;
  auto *output = new FakeTextOutput;
  AppController controller(
      audio, output,
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

  QTRY_COMPARE(controller.state(), QStringLiteral("success"));
  QCOMPARE(controller.transcript(), QStringLiteral("hello world"));
  QCOMPARE(output->deliveredText, QStringLiteral("hello world"));
  QVERIFY(output->deliveredWithAutoPaste);
  QCOMPARE(controller.status(), output->result);
  QCOMPARE(transcriptChanged.count(), 1);
  QTRY_COMPARE_WITH_TIMEOUT(controller.state(), QStringLiteral("idle"), 2000);
}

void AppControllerTest::recoversFromTranscriptionFailure() {
  auto *audio = new FakeAudioCapture;
  auto *output = new FakeTextOutput;
  AppController controller(
      audio, output,
      [](const QByteArray &, const QString &, const QString &) {
        return qMakePair(QString(), QStringLiteral("Transcription failed deterministically."));
      },
      false);

  controller.toggle();
  controller.toggle();

  QTRY_COMPARE(controller.state(), QStringLiteral("idle"));
  QCOMPARE(controller.status(), QStringLiteral("Transcription failed deterministically."));
  QVERIFY(output->deliveredText.isEmpty());
}

void AppControllerTest::rejectsToggleWhileTranscribing() {
  QSemaphore transcriptionStarted;
  QSemaphore allowCompletion;
  auto *audio = new FakeAudioCapture;
  auto *output = new FakeTextOutput;
  AppController controller(
      audio, output,
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

  QCOMPARE(controller.state(), QStringLiteral("transcribing"));
  QCOMPARE(controller.status(), QStringLiteral("Transcription is already in progress."));
  allowCompletion.release();
  QTRY_COMPARE(controller.state(), QStringLiteral("success"));
}

void AppControllerTest::forwardsAudioLevel() {
  auto *audio = new FakeAudioCapture;
  auto *output = new FakeTextOutput;
  AppController controller(
      audio, output,
      [](const QByteArray &, const QString &, const QString &) {
        return QPair<QString, QString>();
      },
      false);
  QSignalSpy levelChanged(&controller, &AppController::levelChanged);

  audio->reportLevel(0.625);

  QCOMPARE(controller.level(), 0.625);
  QCOMPARE(levelChanged.count(), 1);
}

void AppControllerTest::emitsSettingChangesOnlyWhenValuesChange() {
  auto *audio = new FakeAudioCapture;
  auto *output = new FakeTextOutput;
  AppController controller(
      audio, output,
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
