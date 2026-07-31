// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AudioConversion.h"
#include "WhisperEngine.h"

#include <QTemporaryFile>
#include <QTest>
#include <cstring>

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
  void rejectsMissingWhisperModel();
  void rejectsRecordingThatIsTooShort();
  void convertsEmptyAudio();
  void convertsSampleFormats_data();
  void convertsSampleFormats();
  void downmixesStereoAudio();
  void resamplesAudioTo16kHz();
};

void CoreTest::rejectsMissingWhisperModel() {
  QString error;
  const QString text = WhisperEngine::transcribe(QByteArray(1600 * sizeof(float), '\0'),
                                                 QStringLiteral("/missing/kastword-model.bin"),
                                                 QStringLiteral("en"), &error);

  QVERIFY(text.isEmpty());
  QCOMPARE(error, QStringLiteral("Select a valid Whisper model file."));
}

void CoreTest::rejectsRecordingThatIsTooShort() {
  QTemporaryFile model;
  QVERIFY(model.open());
  QString error;

  const QString text = WhisperEngine::transcribe(QByteArray(1599 * sizeof(float), '\0'),
                                                 model.fileName(), QStringLiteral("en"), &error);

  QVERIFY(text.isEmpty());
  QCOMPARE(error, QStringLiteral("The recording was too short."));
}

void CoreTest::convertsEmptyAudio() {
  QVERIFY(convertAudioForWhisper({}, audioFormat(48000, 1, QAudioFormat::Float)).isEmpty());
  QVERIFY(convertAudioForWhisper(floatBytes({1.0F}), {}).isEmpty());
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

void CoreTest::resamplesAudioTo16kHz() {
  const QByteArray output = convertAudioForWhisper(floatBytes({0.0F, 1.0F, 0.0F, -1.0F}),
                                                   audioFormat(8000, 1, QAudioFormat::Float));

  QCOMPARE(output.size(), qsizetype(8 * sizeof(float)));
  QCOMPARE(outputSample(output, 0), 0.0F);
  QCOMPARE(outputSample(output, 1), 0.5F);
  QCOMPARE(outputSample(output, 2), 1.0F);
  QCOMPARE(outputSample(output, 3), 0.5F);
}

QTEST_APPLESS_MAIN(CoreTest)
#include "CoreTest.moc"
