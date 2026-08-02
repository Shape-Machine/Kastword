// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AudioConversion.h"
#include "ModelLocator.h"
#include "RuntimeSecurity.h"
#include "WhisperEngine.h"

#include <KLocalizedString>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTest>
#include <cstring>
#include <limits>

namespace {
QAudioFormat audioFormat(int sampleRate, int channels, QAudioFormat::SampleFormat sampleFormat) {
  QAudioFormat format;
  format.setSampleRate(sampleRate);
  format.setChannelCount(channels);
  format.setSampleFormat(sampleFormat);
  return format;
}

QByteArray floatBytes(std::initializer_list<float> samples) {
  QByteArray bytes(qsizetype(samples.size() * sizeof(float)), Qt::Uninitialized);
  std::memcpy(bytes.data(), samples.begin(), size_t(bytes.size()));
  return bytes;
}

template <typename T> QByteArray sampleBytes(T sample) {
  QByteArray bytes(sizeof(T), Qt::Uninitialized);
  std::memcpy(bytes.data(), &sample, sizeof(T));
  return bytes;
}

float outputSample(const QByteArray &audio, qsizetype index) {
  float sample;
  std::memcpy(&sample, audio.constData() + index * qsizetype(sizeof(float)), sizeof(float));
  return sample;
}
} // namespace

class CoreTest final : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void rejectsMissingWhisperModel();
  void rejectsRecordingThatIsTooShort();
  void rejectsMisalignedAudioData();
  void rejectsInvalidWhisperModel();
  void rejectsOversizedWhisperModel();
  void rejectsNonRegularWhisperModel();
  void rejectsWhisperSampleCountOverflow();
  void reusesAndReloadsWhisperModels();
  void convertsEmptyAudio();
  void calculatesCaptureLimit();
  void enforcesCaptureAppendBoundary();
  void rejectsResamplingArithmeticOverflow();
  void convertsSampleFormats_data();
  void convertsSampleFormats();
  void downmixesStereoAudio();
  void downmixesMultipleChannels();
  void resamplesAudioTo16kHz();
  void downsamplesAudioTo16kHz();
  void ignoresIncompleteFrames();
  void measuresNormalizedAudioPeak();
  void clampsAudioPeak();
  void selectsFirstReadableModel();
  void returnsEmptyWhenNoModelExists();
  void detectsElevatedExecution_data();
  void detectsElevatedExecution();
};

void CoreTest::initTestCase() { KLocalizedString::setApplicationDomain("kastword"); }

void CoreTest::rejectsMissingWhisperModel() {
  WhisperEngine engine;
  QString error;
  const QString text = engine.transcribe(QByteArray(1600 * sizeof(float), '\0'),
                                         QStringLiteral("/missing/kastword-model.bin"),
                                         QStringLiteral("en"), &error);

  QVERIFY(text.isEmpty());
  QCOMPARE(error, QStringLiteral("Select a valid Whisper model file."));
}

void CoreTest::rejectsRecordingThatIsTooShort() {
  WhisperEngine engine;
  QTemporaryFile model;
  QVERIFY(model.open());
  QString error;

  const QString text = engine.transcribe(QByteArray(1599 * sizeof(float), '\0'), model.fileName(),
                                         QStringLiteral("en"), &error);

  QVERIFY(text.isEmpty());
  QCOMPARE(error, QStringLiteral("The recording was too short."));
}

void CoreTest::rejectsMisalignedAudioData() {
  WhisperEngine engine;
  QTemporaryFile model;
  QVERIFY(model.open());
  QString error;

  const QString text = engine.transcribe(QByteArray(1600 * int(sizeof(float)) + 1, '\0'),
                                         model.fileName(), QStringLiteral("en"), &error);

  QVERIFY(text.isEmpty());
  QCOMPARE(error, QStringLiteral("The recording contains invalid audio data."));
}

void CoreTest::rejectsInvalidWhisperModel() {
  WhisperEngine engine;
  QTemporaryFile model;
  QVERIFY(model.open());
  model.write("not a whisper model");
  model.flush();
  QString error;

  const QString text = engine.transcribe(QByteArray(1600 * sizeof(float), '\0'), model.fileName(),
                                         QStringLiteral("en"), &error);

  QVERIFY(text.isEmpty());
  QCOMPARE(error, QStringLiteral("The selected file is not a supported Whisper model."));
}

void CoreTest::rejectsOversizedWhisperModel() {
  QTemporaryFile model;
  QVERIFY(model.open());
  QVERIFY(model.resize(WhisperEngine::maximumModelBytes() + 1));
  model.flush();
  WhisperEngine engine;
  QString error;

  QVERIFY(!engine.loadModel(model.fileName(), &error));
  QCOMPARE(error, QStringLiteral("The selected Whisper model is too large."));
}

void CoreTest::rejectsNonRegularWhisperModel() {
  WhisperEngine engine;
  QString error;

  QVERIFY(!engine.loadModel(QStringLiteral("/dev/null"), &error));
  QCOMPARE(error, QStringLiteral("Select a regular Whisper model file."));
}

void CoreTest::rejectsWhisperSampleCountOverflow() {
  QString error;
  const qsizetype excessive =
      (qsizetype(std::numeric_limits<int>::max()) + 1) * qsizetype(sizeof(float));

  QVERIFY(!WhisperEngine::validateAudioSize(excessive, &error));
  QCOMPARE(error, QStringLiteral("The recording is too long to transcribe safely."));
}

void CoreTest::reusesAndReloadsWhisperModels() {
  QTemporaryFile firstModel;
  QTemporaryFile secondModel;
  QTemporaryFile invalidModel;
  QVERIFY(firstModel.open());
  QVERIFY(secondModel.open());
  QVERIFY(invalidModel.open());
  QCOMPARE(firstModel.write(QByteArray::fromHex("6c6d6767")), 4);
  QCOMPARE(secondModel.write(QByteArray::fromHex("6c6d6767")), 4);
  QCOMPARE(invalidModel.write(QByteArray::fromHex("6c6d6767")), 4);
  firstModel.flush();
  secondModel.flush();
  invalidModel.flush();
  int loads = 0;
  int frees = 0;
  {
    WhisperEngine engine(
        [&loads](const QString &path) {
          ++loads;
          if (loads == 2)
            return static_cast<whisper_context *>(nullptr);
          QFile validatedModel(path);
          if (!validatedModel.open(QIODevice::ReadOnly) ||
              validatedModel.read(4) != QByteArray::fromHex("6c6d6767"))
            return static_cast<whisper_context *>(nullptr);
          return reinterpret_cast<whisper_context *>(quintptr(loads));
        },
        [&frees](whisper_context *) { ++frees; });
    QString error;

    QVERIFY(engine.loadModel(firstModel.fileName(), &error));
    QVERIFY(engine.loadModel(firstModel.fileName(), &error));
    QCOMPARE(loads, 1);
    QCOMPARE(frees, 0);

    QVERIFY(!engine.loadModel(invalidModel.fileName(), &error));
    QCOMPARE(loads, 2);
    QCOMPARE(frees, 0);
    QVERIFY(engine.loadModel(firstModel.fileName(), &error));
    QCOMPARE(loads, 2);

    QVERIFY(engine.loadModel(secondModel.fileName(), &error));
    QCOMPARE(loads, 3);
    QCOMPARE(frees, 1);
  }
  QCOMPARE(frees, 2);
}

void CoreTest::convertsEmptyAudio() {
  QVERIFY(convertAudioForWhisper({}, audioFormat(48000, 1, QAudioFormat::Float)).isEmpty());
  QVERIFY(convertAudioForWhisper(floatBytes({1.0F}), {}).isEmpty());
}

void CoreTest::calculatesCaptureLimit() {
  const QAudioFormat format = audioFormat(48000, 2, QAudioFormat::Int16);
  QCOMPARE(maximumCaptureBytes(format, 300), qsizetype(48000 * 2 * 2 * 300));
  const QAudioFormat largeFormat = audioFormat(192000, 8, QAudioFormat::Float);
  QCOMPARE(maximumCaptureBytes(largeFormat, 300), maximumCapturedAudioBytes());
  QCOMPARE(maximumCaptureBytes(format, 0), qsizetype(0));
  QCOMPARE(maximumCaptureBytes({}, 300), qsizetype(0));
}

void CoreTest::enforcesCaptureAppendBoundary() {
  constexpr qsizetype limit = 100;
  QVERIFY(audioAppendFitsLimit(90, 10, limit));
  QVERIFY(!audioAppendFitsLimit(90, 11, limit));
  QVERIFY(!audioAppendFitsLimit(101, 0, limit));
  QVERIFY(!audioAppendFitsLimit(0, 1, 0));
  QVERIFY(!audioAppendFitsLimit(-1, 1, limit));
  QVERIFY(!audioAppendFitsLimit(1, -1, limit));
}

void CoreTest::rejectsResamplingArithmeticOverflow() {
  qsizetype outputFrames = 0;
  QVERIFY(resampledFrameCount(48000, 48000, &outputFrames));
  QCOMPARE(outputFrames, qsizetype(16000));
  QVERIFY(!resampledFrameCount(std::numeric_limits<qsizetype>::max(), 1, &outputFrames));
  QVERIFY(!resampledFrameCount(0, 16000, &outputFrames));
  QVERIFY(!resampledFrameCount(1, 0, &outputFrames));
  QVERIFY(!resampledFrameCount(1, 16000, nullptr));
}

void CoreTest::convertsSampleFormats_data() {
  QTest::addColumn<QByteArray>("input");
  QTest::addColumn<QAudioFormat::SampleFormat>("sampleFormat");
  QTest::addColumn<float>("expected");

  QTest::newRow("unsigned 8-bit") << sampleBytes<quint8>(192) << QAudioFormat::UInt8 << 0.5F;
  QTest::newRow("signed 16-bit") << sampleBytes<qint16>(16384) << QAudioFormat::Int16 << 0.5F;
  QTest::newRow("signed 32-bit") << sampleBytes<qint32>(1073741824) << QAudioFormat::Int32 << 0.5F;
  QTest::newRow("float") << sampleBytes<float>(0.5F) << QAudioFormat::Float << 0.5F;
}

void CoreTest::convertsSampleFormats() {
  QFETCH(QByteArray, input);
  QFETCH(QAudioFormat::SampleFormat, sampleFormat);
  QFETCH(float, expected);

  const QByteArray output = convertAudioForWhisper(input, audioFormat(16000, 1, sampleFormat));

  QCOMPARE(output.size(), qsizetype(sizeof(float)));
  QCOMPARE(outputSample(output, 0), expected);
}

void CoreTest::downmixesStereoAudio() {
  const QByteArray output = convertAudioForWhisper(floatBytes({1.0F, -1.0F, 0.5F, 0.25F}),
                                                   audioFormat(16000, 2, QAudioFormat::Float));

  QCOMPARE(output.size(), qsizetype(2 * sizeof(float)));
  QCOMPARE(outputSample(output, 0), 0.0F);
  QCOMPARE(outputSample(output, 1), 0.375F);
}

void CoreTest::downmixesMultipleChannels() {
  const QByteArray output = convertAudioForWhisper(floatBytes({1.0F, 0.5F, -0.5F, -1.0F}),
                                                   audioFormat(16000, 4, QAudioFormat::Float));

  QCOMPARE(output.size(), qsizetype(sizeof(float)));
  QCOMPARE(outputSample(output, 0), 0.0F);
}

void CoreTest::resamplesAudioTo16kHz() {
  const QByteArray output = convertAudioForWhisper(floatBytes({0.0F, 1.0F, 0.0F, -1.0F}),
                                                   audioFormat(8000, 1, QAudioFormat::Float));

  QCOMPARE(output.size(), qsizetype(8 * sizeof(float)));
  QCOMPARE(outputSample(output, 0), 0.0F);
  QCOMPARE(outputSample(output, 1), 0.5F);
  QCOMPARE(outputSample(output, 2), 1.0F);
  QCOMPARE(outputSample(output, 3), 0.5F);
}

void CoreTest::downsamplesAudioTo16kHz() {
  const QByteArray output = convertAudioForWhisper(floatBytes({0.0F, 0.25F, 0.5F, 0.75F}),
                                                   audioFormat(32000, 1, QAudioFormat::Float));

  QCOMPARE(output.size(), qsizetype(2 * sizeof(float)));
  QCOMPARE(outputSample(output, 0), 0.0F);
  QCOMPARE(outputSample(output, 1), 0.5F);
}

void CoreTest::ignoresIncompleteFrames() {
  QByteArray input;
  const qint16 samples[] = {16384, -16384, 32767};
  input.append(reinterpret_cast<const char *>(samples), sizeof(samples));

  const QByteArray output =
      convertAudioForWhisper(input, audioFormat(16000, 2, QAudioFormat::Int16));

  QCOMPARE(output.size(), qsizetype(sizeof(float)));
  QCOMPARE(outputSample(output, 0), 0.0F);
}

void CoreTest::measuresNormalizedAudioPeak() {
  const QAudioFormat format = audioFormat(16000, 1, QAudioFormat::Float);

  QCOMPARE(normalizedAudioPeak(floatBytes({-0.25F, 0.75F, 0.5F}), format), 0.75);
  QCOMPARE(normalizedAudioPeak({}, format), 0.0);
  QCOMPARE(normalizedAudioPeak(floatBytes({1.0F}), {}), 0.0);
}

void CoreTest::clampsAudioPeak() {
  QCOMPARE(
      normalizedAudioPeak(floatBytes({-1.5F, 1.25F}), audioFormat(16000, 1, QAudioFormat::Float)),
      1.0);
}

void CoreTest::selectsFirstReadableModel() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString firstPath = directory.filePath(QStringLiteral("first.bin"));
  const QString secondPath = directory.filePath(QStringLiteral("second.bin"));
  QFile first(firstPath);
  QFile second(secondPath);
  QVERIFY(first.open(QIODevice::WriteOnly));
  QVERIFY(second.open(QIODevice::WriteOnly));
  first.close();
  second.close();

  QCOMPARE(firstReadableModel(
               {directory.filePath(QStringLiteral("missing.bin")), firstPath, secondPath}),
           QFileInfo(firstPath).canonicalFilePath());
}

void CoreTest::returnsEmptyWhenNoModelExists() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());

  QVERIFY(firstReadableModel({directory.filePath(QStringLiteral("missing.bin")), directory.path()})
              .isEmpty());
}

void CoreTest::detectsElevatedExecution_data() {
  QTest::addColumn<quint64>("realUserId");
  QTest::addColumn<quint64>("effectiveUserId");
  QTest::addColumn<bool>("refused");

  QTest::newRow("regular user") << quint64(1000) << quint64(1000) << false;
  QTest::newRow("setuid root") << quint64(1000) << quint64(0) << true;
  QTest::newRow("root") << quint64(0) << quint64(0) << true;
  QTest::newRow("changed identity") << quint64(1000) << quint64(1001) << true;
}

void CoreTest::detectsElevatedExecution() {
  QFETCH(quint64, realUserId);
  QFETCH(quint64, effectiveUserId);
  QFETCH(bool, refused);
  QCOMPARE(shouldRefuseElevatedExecution(realUserId, effectiveUserId), refused);
}

QTEST_APPLESS_MAIN(CoreTest)
#include "CoreTest.moc"
