// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "DictationHistory.h"

#include <KLocalizedString>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTest>
#include <sodium.h>

namespace {
class FakeKeyProvider final : public HistoryKeyProvider {
public:
  QByteArray key = QByteArray(crypto_aead_xchacha20poly1305_ietf_KEYBYTES, 'k');
  bool failLoad = false;
  bool failRemove = false;
  int loads = 0;
  int removals = 0;

  std::optional<QByteArray> loadOrCreate(QString *error) override {
    ++loads;
    if (failLoad) {
      *error = QStringLiteral("Wallet unavailable");
      return std::nullopt;
    }
    return key;
  }
  bool remove(QString *error) override {
    ++removals;
    if (failRemove) {
      *error = QStringLiteral("Key removal failed");
      return false;
    }
    return true;
  }
};

std::unique_ptr<FakeKeyProvider> provider(QByteArray key = {}) {
  auto result = std::make_unique<FakeKeyProvider>();
  if (!key.isEmpty())
    result->key = std::move(key);
  return result;
}
} // namespace

class DictationHistoryTest final : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { KLocalizedString::setApplicationDomain("kastword"); }

  void remainsAbsentUntilEnabled() {
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("private/history.enc"));
    auto keys = provider();
    auto *keysPtr = keys.get();
    DictationHistory history(path, std::move(keys));

    QVERIFY(!history.enabled());
    QVERIFY(history.entries().isEmpty());
    QVERIFY(!history.add(QStringLiteral("not retained")));
    QVERIFY(!QFileInfo::exists(path));
    QCOMPARE(keysPtr->loads, 0);
  }

  void encryptsAndReloadsWithoutPlaintext() {
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("private/history.enc"));
    const QByteArray key(crypto_aead_xchacha20poly1305_ietf_KEYBYTES, 's');
    const QDateTime now(QDate(2026, 8, 8), QTime(12, 0), QTimeZone::UTC);
    {
      DictationHistory history(path, provider(key), [now] { return now; });
      QVERIFY(history.enable());
      QVERIFY(history.add(QStringLiteral("sensitive dictation")));
      QCOMPARE(history.entries().size(), 1);
    }
    QFile encrypted(path);
    QVERIFY(encrypted.open(QIODevice::ReadOnly));
    const QByteArray bytes = encrypted.readAll();
    QVERIFY(!bytes.contains("sensitive dictation"));
    QCOMPARE(QFileInfo(path).permissions() & QFileDevice::Permissions(0x077),
             QFileDevice::Permissions{});
    QCOMPARE(QFileInfo(QFileInfo(path).absolutePath()).permissions() &
                 QFileDevice::Permissions(0x077),
             QFileDevice::Permissions{});

    DictationHistory restored(path, provider(key), [now] { return now; });
    QVERIFY(restored.enable());
    QCOMPARE(restored.entries().size(), 1);
    QCOMPARE(restored.entries().constFirst().toMap().value(QStringLiteral("text")).toString(),
             QStringLiteral("sensitive dictation"));
  }

  void rejectsTamperingAndWrongKeys() {
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("history.enc"));
    const QByteArray key(crypto_aead_xchacha20poly1305_ietf_KEYBYTES, 'a');
    DictationHistory history(path, provider(key));
    QVERIFY(history.enable());
    QVERIFY(history.add(QStringLiteral("private")));

    DictationHistory wrongKey(
        path, provider(QByteArray(crypto_aead_xchacha20poly1305_ietf_KEYBYTES, 'b')));
    QVERIFY(!wrongKey.enable());
    QVERIFY(!wrongKey.available());
    QVERIFY(wrongKey.entries().isEmpty());

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadWrite));
    QVERIFY(file.seek(file.size() - 1));
    char byte = 0;
    QCOMPARE(file.read(&byte, 1), 1);
    QVERIFY(file.seek(file.size() - 1));
    byte ^= 1;
    QCOMPARE(file.write(&byte, 1), 1);
    file.close();
    DictationHistory tampered(path, provider(key));
    QVERIFY(!tampered.enable());
    QVERIFY(tampered.entries().isEmpty());
  }

  void failsClosedWhenKeyOrCommitUnavailable() {
    QTemporaryDir directory;
    auto failingKeys = provider();
    failingKeys->failLoad = true;
    DictationHistory noKey(directory.filePath(QStringLiteral("history.enc")),
                           std::move(failingKeys));
    QVERIFY(!noKey.enable());
    QVERIFY(!noKey.enabled());
    QVERIFY(!noKey.available());
    QVERIFY(!QFileInfo::exists(noKey.storagePath()));

    DictationHistory noWrite(directory.filePath(QStringLiteral("failed.enc")), provider(), {},
                             [](const QString &, const QByteArray &, QString *error) {
                               *error = QStringLiteral("Atomic write failed");
                               return false;
                             });
    QVERIFY(noWrite.enable());
    QVERIFY(!noWrite.add(QStringLiteral("must not leak")));
    QVERIFY(!noWrite.available());
    QVERIFY(!QFileInfo::exists(noWrite.storagePath()));
  }

  void failedAtomicUpdatePreservesFileAndVisibleEntries() {
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("history.enc"));
    bool rejectWrites = false;
    DictationHistory history(
        path, provider(), {},
        [&rejectWrites](const QString &target, const QByteArray &data, QString *error) {
          if (rejectWrites) {
            *error = QStringLiteral("Injected failure");
            return false;
          }
          QSaveFile file(target);
          if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
            return false;
          return true;
        });
    QVERIFY(history.enable());
    QVERIFY(history.add(QStringLiteral("persisted")));
    QFile beforeFile(path);
    QVERIFY(beforeFile.open(QIODevice::ReadOnly));
    const QByteArray before = beforeFile.readAll();
    beforeFile.close();

    rejectWrites = true;
    QVERIFY(!history.add(QStringLiteral("not persisted")));
    QCOMPARE(history.entries().size(), 1);
    QCOMPARE(history.entries().constFirst().toMap().value(QStringLiteral("text")),
             QStringLiteral("persisted"));
    QFile afterFile(path);
    QVERIFY(afterFile.open(QIODevice::ReadOnly));
    QCOMPARE(afterFile.readAll(), before);
  }

  void enforcesOrderingAndRetention() {
    QTemporaryDir directory;
    QDateTime now(QDate(2026, 8, 8), QTime(12, 0), QTimeZone::UTC);
    DictationHistory history(directory.filePath(QStringLiteral("history.enc")), provider(),
                             [&now] { return now; });
    QVERIFY(history.enable());
    history.setMaximumEntries(2);
    history.setMaximumAgeDays(2);
    QVERIFY(history.add(QStringLiteral("first")));
    now = now.addDays(1);
    QVERIFY(history.add(QStringLiteral("second")));
    now = now.addDays(1);
    QVERIFY(history.add(QStringLiteral("third")));
    QCOMPARE(history.entries().size(), 2);
    QCOMPARE(history.entries().at(0).toMap().value(QStringLiteral("text")),
             QStringLiteral("third"));
    QCOMPARE(history.entries().at(1).toMap().value(QStringLiteral("text")),
             QStringLiteral("second"));
    now = now.addDays(2);
    history.setMaximumAgeDays(1);
    QVERIFY(history.entries().isEmpty());
  }

  void deletesEntriesAndStoredData() {
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("history.enc"));
    auto keys = provider();
    auto *keysPtr = keys.get();
    DictationHistory history(path, std::move(keys));
    QVERIFY(history.enable());
    QVERIFY(history.add(QStringLiteral("one")));
    QVERIFY(history.add(QStringLiteral("two")));
    const QString id =
        history.entries().constFirst().toMap().value(QStringLiteral("id")).toString();
    QVERIFY(history.removeEntry(id));
    QCOMPARE(history.entries().size(), 1);
    QVERIFY(history.clear());
    QVERIFY(history.entries().isEmpty());
    QVERIFY(QFileInfo::exists(path));
    history.disable(false);
    QVERIFY(history.entries().isEmpty());
    QVERIFY(QFileInfo::exists(path));
    QVERIFY(history.enable());
    history.disable(true);
    QVERIFY(!QFileInfo::exists(path));
    QCOMPARE(keysPtr->removals, 1);
  }
};

QTEST_GUILESS_MAIN(DictationHistoryTest)
#include "DictationHistoryTest.moc"
