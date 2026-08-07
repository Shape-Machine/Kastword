// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ModelManager.h"
#include "ModelCatalog.h"

#include <KLocalizedString>
#include <QCryptographicHash>
#include <QNetworkReply>
#include <QScopeGuard>
#include <QSemaphore>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <atomic>
#include <cstring>

class FakeReply final : public QNetworkReply {
  Q_OBJECT

public:
  FakeReply(const QNetworkRequest &request, QByteArray payload, int delay, bool fail,
            QObject *parent)
      : QNetworkReply(parent), m_payload(std::move(payload)), m_fail(fail) {
    setRequest(request);
    setUrl(request.url());
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute,
                 request.hasRawHeader("Range") ? 206 : 200);
    setHeader(QNetworkRequest::ContentLengthHeader, m_payload.size());
    open(QIODevice::ReadOnly);
    QTimer::singleShot(delay, this, [this] {
      if (m_aborted)
        return;
      if (m_fail) {
        setError(ConnectionRefusedError, QStringLiteral("connection refused"));
        setFinished(true);
        emit finished();
        return;
      }
      emit downloadProgress(m_payload.size(), m_payload.size());
      emit readyRead();
      setFinished(true);
      emit finished();
    });
  }

  void abort() override {
    if (isFinished())
      return;
    m_aborted = true;
    setError(OperationCanceledError, QStringLiteral("cancelled"));
    setFinished(true);
    emit finished();
  }
  qint64 bytesAvailable() const override {
    return m_payload.size() - m_offset + QIODevice::bytesAvailable();
  }

protected:
  qint64 readData(char *data, qint64 maxSize) override {
    const qint64 count = qMin(maxSize, m_payload.size() - m_offset);
    if (count <= 0)
      return -1;
    std::memcpy(data, m_payload.constData() + m_offset, size_t(count));
    m_offset += count;
    return count;
  }

private:
  QByteArray m_payload;
  qint64 m_offset = 0;
  bool m_aborted = false;
  bool m_fail = false;
};

class FakeNetworkAccessManager final : public QNetworkAccessManager {
  Q_OBJECT

public:
  QByteArray payload;
  int delay = 0;
  bool fail = false;
  QByteArray receivedRange;
  int requestCount = 0;

protected:
  QNetworkReply *createRequest(Operation operation, const QNetworkRequest &request,
                               QIODevice *outgoingData) override {
    Q_UNUSED(operation)
    Q_UNUSED(outgoingData)
    ++requestCount;
    receivedRange = request.rawHeader("Range");
    QByteArray response = payload;
    if (!receivedRange.isEmpty()) {
      const qsizetype equals = receivedRange.indexOf('=');
      const qsizetype dash = receivedRange.indexOf('-');
      const qint64 offset = receivedRange.mid(equals + 1, dash - equals - 1).toLongLong();
      response = response.mid(offset);
    }
    return new FakeReply(request, response, delay, fail, this);
  }
};

class ModelManagerTest final : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void catalogIsCompleteAndValid();
  void rejectsUnknownDisplayMetadata();
  void labelsUnknownModelIdsExplicitly();
  void downloadsVerifiesAndActivates();
  void resumesPartialDownload();
  void reusesCompletePartialDownload();
  void rejectsChecksumFailure();
  void rejectsTruncatedDownload();
  void reportsNetworkFailure();
  void cancelsWithoutActivating();
  void cancelsVerificationWithoutDiscardingPartial();
  void removesActiveModel();
  void restoresVerifiedManagedModel();
  void acceptsValidLocalModel();
  void preservesEnglishOnlyCapabilityForLocalModel();
  void rejectsStructurallyValidButUnparseableLocalModel();
  void rejectsInvalidLocalModel();
  void reportsRestorationWhileValidatingLocalModel();
  void keepsActiveModelWhenRemovalFails();
  void distinguishesDownloadFromVerification();
  void retainsModelOnHashReadFailure();
  void invalidatesChangedManagedModel();
  void invalidatesChangedLocalModel();
  void resetsOversizedPartialBeforeRequest();
  void discardsPartialDownload();
};

namespace {
QByteArray validPayload() {
  return QByteArray::fromHex("6c6d6767") + QByteArrayLiteral("test-model");
}

ModelCatalogEntry testEntry(const QByteArray &payload) {
  return {QStringLiteral("test"),
          QStringLiteral("ggml-test.bin"),
          QUrl(QStringLiteral("https://example.test/ggml-test.bin")),
          QCryptographicHash::hash(payload, QCryptographicHash::Sha256),
          payload.size(),
          false,
          true,
          QStringLiteral("Fast"),
          QStringLiteral("Good")};
}
} // namespace

void ModelManagerTest::initTestCase() { KLocalizedString::setApplicationDomain("kastword"); }

void ModelManagerTest::catalogIsCompleteAndValid() {
  const QList<ModelCatalogEntry> catalog = whisperModelCatalog();
  QCOMPARE(catalog.size(), 12);
  QVERIFY(validateModelCatalog(catalog).isEmpty());
  QCOMPARE(std::count_if(catalog.cbegin(), catalog.cend(),
                         [](const ModelCatalogEntry &entry) { return entry.englishOnly; }),
           4);
  QVERIFY(std::all_of(catalog.cbegin(), catalog.cend(), [](const ModelCatalogEntry &entry) {
    return entry.url.path().contains(QStringLiteral("5359861c739e955e79d9a303bcbc70fb988958b1")) &&
           entry.sha256.size() == 32;
  }));
}

void ModelManagerTest::rejectsUnknownDisplayMetadata() {
  ModelCatalogEntry item = testEntry(validPayload());
  item.speed = QStringLiteral("Immediate");
  QVERIFY(validateModelCatalog({item}).contains(QStringLiteral("display metadata")));
  item.speed = QStringLiteral("Fast");
  item.accuracy = QStringLiteral("Perfect");
  QVERIFY(validateModelCatalog({item}).contains(QStringLiteral("display metadata")));
}

void ModelManagerTest::labelsUnknownModelIdsExplicitly() {
  QTemporaryDir directory;
  FakeNetworkAccessManager network;
  ModelManager manager({testEntry(validPayload())}, directory.path(), &network);

  const QVariantMap model = manager.models().constFirst().toMap();

  QCOMPARE(model.value(QStringLiteral("name")).toString(), QStringLiteral("Unknown model"));
  QCOMPARE(model.value(QStringLiteral("url")).toString(),
           QStringLiteral("https://example.test/ggml-test.bin"));
}

void ModelManagerTest::downloadsVerifiesAndActivates() {
  QTemporaryDir directory;
  FakeNetworkAccessManager network;
  network.payload = validPayload();
  ModelManager manager({testEntry(network.payload)}, directory.path(), &network);
  QSignalSpy active(&manager, &ModelManager::activeModelPathChanged);

  manager.download(QStringLiteral("test"));

  QTRY_VERIFY_WITH_TIMEOUT(manager.modelReady(), 3000);
  QCOMPARE(active.count(), 1);
  QCOMPARE(QFileInfo(manager.activeModelPath()).size(), network.payload.size());
  QVERIFY(!QFileInfo::exists(manager.activeModelPath() + QStringLiteral(".part")));
}

void ModelManagerTest::resumesPartialDownload() {
  QTemporaryDir directory;
  FakeNetworkAccessManager network;
  network.payload = validPayload();
  const ModelCatalogEntry item = testEntry(network.payload);
  QFile partial(directory.filePath(item.fileName + QStringLiteral(".part")));
  QVERIFY(partial.open(QIODevice::WriteOnly));
  QCOMPARE(partial.write(network.payload.first(5)), 5);
  partial.close();
  ModelManager manager({item}, directory.path(), &network);

  manager.download(item.id);

  QTRY_VERIFY_WITH_TIMEOUT(manager.modelReady(), 3000);
  QCOMPARE(network.receivedRange, QByteArrayLiteral("bytes=5-"));
}

void ModelManagerTest::reusesCompletePartialDownload() {
  QTemporaryDir directory;
  const QByteArray payload = validPayload();
  const ModelCatalogEntry item = testEntry(payload);
  QFile partial(directory.filePath(item.fileName + QStringLiteral(".part")));
  QVERIFY(partial.open(QIODevice::WriteOnly));
  QCOMPARE(partial.write(payload), payload.size());
  partial.close();
  FakeNetworkAccessManager network;
  network.payload = payload;
  ModelManager manager({item}, directory.path(), &network);

  manager.download(item.id);

  QTRY_VERIFY_WITH_TIMEOUT(manager.modelReady(), 3000);
  QCOMPARE(network.requestCount, 0);
  QVERIFY(!QFileInfo::exists(partial.fileName()));
  QVERIFY(QFileInfo::exists(directory.filePath(item.fileName)));
}

void ModelManagerTest::rejectsChecksumFailure() {
  QTemporaryDir directory;
  FakeNetworkAccessManager network;
  network.payload = validPayload();
  ModelCatalogEntry item = testEntry(network.payload);
  item.sha256 = QByteArray(32, 'x');
  ModelManager manager({item}, directory.path(), &network);

  manager.download(item.id);

  QTRY_VERIFY_WITH_TIMEOUT(!manager.busy(), 3000);
  QVERIFY(!manager.modelReady());
  QVERIFY(!manager.error().isEmpty());
  QVERIFY(!QFileInfo::exists(directory.filePath(item.fileName)));
  QVERIFY(!QFileInfo::exists(directory.filePath(item.fileName + QStringLiteral(".part"))));
}

void ModelManagerTest::rejectsTruncatedDownload() {
  QTemporaryDir directory;
  FakeNetworkAccessManager network;
  network.payload = validPayload();
  ModelCatalogEntry item = testEntry(network.payload);
  network.payload.chop(2);
  ModelManager manager({item}, directory.path(), &network);

  manager.download(item.id);

  QTRY_VERIFY(!manager.busy());
  QVERIFY(!manager.modelReady());
  QVERIFY(manager.error().contains(QStringLiteral("unexpected size")));
}

void ModelManagerTest::reportsNetworkFailure() {
  QTemporaryDir directory;
  FakeNetworkAccessManager network;
  network.payload = validPayload();
  network.fail = true;
  const ModelCatalogEntry item = testEntry(network.payload);
  ModelManager manager({item}, directory.path(), &network);

  manager.download(item.id);

  QTRY_VERIFY(!manager.busy());
  QVERIFY(!manager.modelReady());
  QVERIFY(manager.error().contains(QStringLiteral("connection")));
}

void ModelManagerTest::cancelsWithoutActivating() {
  QTemporaryDir directory;
  FakeNetworkAccessManager network;
  network.payload = validPayload();
  network.delay = 100;
  const ModelCatalogEntry item = testEntry(network.payload);
  ModelManager manager({item}, directory.path(), &network);

  manager.download(item.id);
  manager.cancel();

  QTRY_VERIFY(!manager.busy());
  QVERIFY(!manager.modelReady());
  QVERIFY(!QFileInfo::exists(directory.filePath(item.fileName)));
}

void ModelManagerTest::cancelsVerificationWithoutDiscardingPartial() {
  QTemporaryDir directory;
  const QByteArray payload = validPayload();
  const ModelCatalogEntry item = testEntry(payload);
  const QString partialPath = directory.filePath(item.fileName + QStringLiteral(".part"));
  QFile partial(partialPath);
  QVERIFY(partial.open(QIODevice::WriteOnly));
  QCOMPARE(partial.write(payload), payload.size());
  partial.close();
  QSemaphore hashStarted;
  QSemaphore hashGate;
  FakeNetworkAccessManager network;
  ModelManager manager({item}, directory.path(), &network, nullptr, {},
                       [&hashStarted, &hashGate,
                        payload](const QString &, const ModelManager::CancellationFlag &cancelled)
                           -> ModelManager::HashResult {
                         hashStarted.release();
                         hashGate.acquire();
                         if (cancelled->load())
                           return std::nullopt;
                         return QCryptographicHash::hash(payload, QCryptographicHash::Sha256);
                       });
  const auto releaseHash = qScopeGuard([&hashGate] { hashGate.release(); });

  manager.download(item.id);
  QVERIFY(hashStarted.tryAcquire(1, 3000));
  manager.cancel();
  hashGate.release();

  QTRY_VERIFY_WITH_TIMEOUT(manager.status().contains(QStringLiteral("cancelled")), 3000);
  QVERIFY(!manager.busy());
  QVERIFY(!manager.modelReady());
  QVERIFY(QFileInfo::exists(partialPath));
  QCOMPARE(network.requestCount, 0);
}

void ModelManagerTest::removesActiveModel() {
  QTemporaryDir directory;
  FakeNetworkAccessManager network;
  network.payload = validPayload();
  const ModelCatalogEntry item = testEntry(network.payload);
  ModelManager manager({item}, directory.path(), &network);
  manager.download(item.id);
  QTRY_VERIFY_WITH_TIMEOUT(manager.modelReady(), 3000);

  QVERIFY(manager.removeModel(item.id));

  QVERIFY(!manager.modelReady());
  QVERIFY(!QFileInfo::exists(directory.filePath(item.fileName)));
}

void ModelManagerTest::restoresVerifiedManagedModel() {
  QTemporaryDir directory;
  const QByteArray payload = validPayload();
  const ModelCatalogEntry item = testEntry(payload);
  QFile model(directory.filePath(item.fileName));
  QVERIFY(model.open(QIODevice::WriteOnly));
  QCOMPARE(model.write(payload), payload.size());
  model.close();
  FakeNetworkAccessManager network;
  ModelManager manager({item}, directory.path(), &network);

  manager.restoreActiveModel(model.fileName());

  QTRY_VERIFY_WITH_TIMEOUT(manager.modelReady(), 3000);
  QCOMPARE(manager.activeModelPath(), model.fileName());
}

void ModelManagerTest::acceptsValidLocalModel() {
  QTemporaryDir directory;
  const QByteArray payload = validPayload();
  QFile model(directory.filePath(QStringLiteral("custom.bin")));
  QVERIFY(model.open(QIODevice::WriteOnly));
  QCOMPARE(model.write(payload), payload.size());
  model.close();
  FakeNetworkAccessManager network;
  ModelManager manager(
      {testEntry(payload)}, directory.filePath(QStringLiteral("managed")), &network, nullptr,
      [](const QString &) { return ModelManager::ModelValidationResult{true, false}; });

  QVERIFY(manager.selectLocalModel(QUrl::fromLocalFile(model.fileName())));
  QTRY_VERIFY(manager.modelReady());
  QCOMPARE(manager.activeModelPath(), model.fileName());
  QVERIFY(!manager.activeModelEnglishOnly());
}

void ModelManagerTest::preservesEnglishOnlyCapabilityForLocalModel() {
  QTemporaryDir directory;
  QFile model(directory.filePath(QStringLiteral("custom.en.bin")));
  QVERIFY(model.open(QIODevice::WriteOnly));
  QCOMPARE(model.write(validPayload()), validPayload().size());
  model.close();
  FakeNetworkAccessManager network;
  ModelManager manager(
      {testEntry(validPayload())}, directory.filePath(QStringLiteral("managed")), &network, nullptr,
      [](const QString &) { return ModelManager::ModelValidationResult{true, true}; });

  QVERIFY(manager.selectLocalModel(QUrl::fromLocalFile(model.fileName())));

  QTRY_VERIFY(manager.modelReady());
  QVERIFY(manager.activeModelEnglishOnly());
}

void ModelManagerTest::rejectsStructurallyValidButUnparseableLocalModel() {
  QTemporaryDir directory;
  QFile model(directory.filePath(QStringLiteral("invalid.bin")));
  QVERIFY(model.open(QIODevice::WriteOnly));
  QCOMPARE(model.write(validPayload()), validPayload().size());
  model.close();
  FakeNetworkAccessManager network;
  ModelManager manager({testEntry(validPayload())}, directory.filePath(QStringLiteral("managed")),
                       &network, nullptr,
                       [](const QString &) { return ModelManager::ModelValidationResult{}; });

  QVERIFY(manager.selectLocalModel(QUrl::fromLocalFile(model.fileName())));

  QTRY_VERIFY(manager.error().contains(QStringLiteral("not a compatible")));
  QVERIFY(!manager.busy());
  QVERIFY(!manager.modelReady());
}

void ModelManagerTest::rejectsInvalidLocalModel() {
  QTemporaryDir directory;
  QFile invalid(directory.filePath(QStringLiteral("invalid.bin")));
  QVERIFY(invalid.open(QIODevice::WriteOnly));
  invalid.write("invalid");
  invalid.close();
  FakeNetworkAccessManager network;
  ModelManager manager({testEntry(validPayload())}, directory.path(), &network);

  QVERIFY(!manager.selectLocalModel(QUrl::fromLocalFile(invalid.fileName())));
  QVERIFY(!manager.modelReady());
}

void ModelManagerTest::reportsRestorationWhileValidatingLocalModel() {
  QTemporaryDir directory;
  QFile model(directory.filePath(QStringLiteral("custom.bin")));
  QVERIFY(model.open(QIODevice::WriteOnly));
  QCOMPARE(model.write(validPayload()), validPayload().size());
  model.close();
  QSemaphore validationGate;
  FakeNetworkAccessManager network;
  ModelManager manager({testEntry(validPayload())}, directory.filePath(QStringLiteral("managed")),
                       &network, nullptr, [&validationGate](const QString &) {
                         validationGate.acquire();
                         return ModelManager::ModelValidationResult{true, false};
                       });
  const auto releaseValidation = qScopeGuard([&validationGate] { validationGate.release(); });

  manager.restoreActiveModel(model.fileName());

  QVERIFY(manager.restoringActiveModel());
  QVERIFY(!manager.modelReady());
  validationGate.release();
  QTRY_VERIFY(manager.modelReady());
  QVERIFY(!manager.restoringActiveModel());
}

void ModelManagerTest::distinguishesDownloadFromVerification() {
  QTemporaryDir directory;
  const QByteArray payload = validPayload();
  const ModelCatalogEntry item = testEntry(payload);
  FakeNetworkAccessManager network;
  network.payload = payload;
  network.delay = 100;
  ModelManager downloading({item}, directory.filePath(QStringLiteral("download")), &network);

  downloading.download(item.id);

  QVariantMap state = downloading.models().constFirst().toMap();
  QVERIFY(state.value(QStringLiteral("downloading")).toBool());
  QVERIFY(!state.value(QStringLiteral("verifying")).toBool());
  downloading.cancel();
  QTRY_VERIFY(!downloading.busy());

  QDir().mkpath(directory.filePath(QStringLiteral("verify")));
  QFile model(directory.filePath(QStringLiteral("verify/") + item.fileName));
  QVERIFY(model.open(QIODevice::WriteOnly));
  QCOMPARE(model.write(payload), payload.size());
  model.close();
  QSemaphore hashGate;
  FakeNetworkAccessManager verificationNetwork;
  ModelManager verifying(
      {item}, directory.filePath(QStringLiteral("verify")), &verificationNetwork, nullptr, {},
      [&hashGate, payload](const QString &,
                           const ModelManager::CancellationFlag &) -> ModelManager::HashResult {
        hashGate.acquire();
        return QCryptographicHash::hash(payload, QCryptographicHash::Sha256);
      });
  const auto releaseHash = qScopeGuard([&hashGate] { hashGate.release(); });

  verifying.selectModel(item.id);

  state = verifying.models().constFirst().toMap();
  QVERIFY(!state.value(QStringLiteral("downloading")).toBool());
  QVERIFY(state.value(QStringLiteral("verifying")).toBool());
  hashGate.release();
  QTRY_VERIFY(verifying.modelReady());
}

void ModelManagerTest::retainsModelOnHashReadFailure() {
  QTemporaryDir directory;
  const QByteArray payload = validPayload();
  const ModelCatalogEntry item = testEntry(payload);
  QFile model(directory.filePath(item.fileName));
  QVERIFY(model.open(QIODevice::WriteOnly));
  QCOMPARE(model.write(payload), payload.size());
  model.close();
  FakeNetworkAccessManager network;
  ModelManager manager(
      {item}, directory.path(), &network, nullptr, {},
      [](const QString &, const ModelManager::CancellationFlag &) -> ModelManager::HashResult {
        return std::nullopt;
      });

  manager.selectModel(item.id);

  QTRY_VERIFY(!manager.busy());
  QVERIFY(QFileInfo::exists(model.fileName()));
  QVERIFY(!manager.modelReady());
  QVERIFY(manager.error().contains(QStringLiteral("could not be read")));
}

void ModelManagerTest::invalidatesChangedManagedModel() {
  QTemporaryDir directory;
  FakeNetworkAccessManager network;
  network.payload = validPayload();
  const ModelCatalogEntry item = testEntry(network.payload);
  ModelManager manager({item}, directory.path(), &network);
  manager.download(item.id);
  QTRY_VERIFY(manager.modelReady());
  const QString path = manager.activeModelPath();
  QFile corrupt(path);
  QVERIFY(corrupt.open(QIODevice::WriteOnly | QIODevice::Truncate));
  const QByteArray replacement = QByteArray::fromHex("6c6d6767") + QByteArray(10, 'x');
  QCOMPARE(replacement.size(), network.payload.size());
  QCOMPARE(corrupt.write(replacement), replacement.size());
  corrupt.close();

  QTRY_VERIFY(!manager.modelReady());
  QTRY_VERIFY(!manager.busy());
  QVERIFY(!QFileInfo::exists(path));
}

void ModelManagerTest::invalidatesChangedLocalModel() {
  QTemporaryDir directory;
  QFile model(directory.filePath(QStringLiteral("custom.bin")));
  QVERIFY(model.open(QIODevice::WriteOnly));
  QCOMPARE(model.write(validPayload()), validPayload().size());
  model.close();
  std::atomic_int validations = 0;
  FakeNetworkAccessManager network;
  ModelManager manager({testEntry(validPayload())}, directory.filePath(QStringLiteral("managed")),
                       &network, nullptr, [&validations](const QString &) {
                         return ModelManager::ModelValidationResult{validations.fetch_add(1) == 0,
                                                                    false};
                       });
  QVERIFY(manager.selectLocalModel(QUrl::fromLocalFile(model.fileName())));
  QTRY_VERIFY(manager.modelReady());
  QVERIFY(model.open(QIODevice::WriteOnly | QIODevice::Truncate));
  QCOMPARE(model.write(QByteArray::fromHex("6c6d6767") + QByteArray(10, 'x')), 14);
  model.close();

  QTRY_VERIFY(!manager.modelReady());
  QTRY_VERIFY(!manager.busy());
  QVERIFY(QFileInfo::exists(model.fileName()));
}

void ModelManagerTest::resetsOversizedPartialBeforeRequest() {
  QTemporaryDir directory;
  FakeNetworkAccessManager network;
  network.payload = validPayload();
  network.delay = 100;
  const ModelCatalogEntry item = testEntry(network.payload);
  QFile partial(directory.filePath(item.fileName + QStringLiteral(".part")));
  QVERIFY(partial.open(QIODevice::WriteOnly));
  QVERIFY(partial.resize(item.size + 1));
  partial.close();
  ModelManager manager({item}, directory.path(), &network);

  manager.download(item.id);

  QVERIFY(network.receivedRange.isEmpty());
  QCOMPARE(network.requestCount, 1);
  manager.cancel();
  QTRY_VERIFY(!manager.busy());
}

void ModelManagerTest::discardsPartialDownload() {
  QTemporaryDir directory;
  FakeNetworkAccessManager network;
  network.payload = validPayload();
  const ModelCatalogEntry item = testEntry(network.payload);
  const QString partialPath = directory.filePath(item.fileName + QStringLiteral(".part"));
  QFile partial(partialPath);
  QVERIFY(partial.open(QIODevice::WriteOnly));
  QCOMPARE(partial.write(network.payload.first(5)), 5);
  partial.close();
  ModelManager manager({item}, directory.path(), &network);

  const QVariantMap state = manager.models().constFirst().toMap();
  QVERIFY(state.value(QStringLiteral("partial")).toBool());
  QCOMPARE(state.value(QStringLiteral("partialSize")).toLongLong(), 5);
  QVERIFY(manager.removeModel(item.id));

  QVERIFY(!QFileInfo::exists(partialPath));
  QVERIFY(!manager.models().constFirst().toMap().value(QStringLiteral("partial")).toBool());
}

void ModelManagerTest::keepsActiveModelWhenRemovalFails() {
  QTemporaryDir directory;
  FakeNetworkAccessManager network;
  network.payload = validPayload();
  const ModelCatalogEntry item = testEntry(network.payload);
  ModelManager manager({item}, directory.path(), &network);
  manager.download(item.id);
  QTRY_VERIFY(manager.modelReady());
  const QString activePath = manager.activeModelPath();
  ModelManager managerWithRemovalFailure(
      {item}, directory.path(), &network, nullptr, {}, {},
      [activePath](const QString &path) { return path != activePath && QFile::remove(path); });
  managerWithRemovalFailure.restoreActiveModel(activePath);
  QTRY_VERIFY(managerWithRemovalFailure.modelReady());

  const bool removed = managerWithRemovalFailure.removeModel(item.id);

  QVERIFY(!removed);
  QVERIFY(managerWithRemovalFailure.modelReady());
  QVERIFY(QFileInfo::exists(managerWithRemovalFailure.activeModelPath()));
  QVERIFY(managerWithRemovalFailure.error().contains(QStringLiteral("could not be removed")));
}

QTEST_MAIN(ModelManagerTest)
#include "ModelManagerTest.moc"
