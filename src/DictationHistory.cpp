// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "DictationHistory.h"

#include <KLocalizedString>
#include <KWallet>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFuture>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <QWindow>
#include <QtConcurrentRun>
#include <algorithm>
#include <limits>
#include <sodium.h>

namespace {
constexpr auto historyFileName = "history.enc";
constexpr auto walletFolder = "Kastword";
constexpr auto walletEntry = "dictation-history-key-v1";
constexpr qint64 maximumHistoryFileSize = 64LL * 1024 * 1024;
const QByteArray magic = QByteArrayLiteral("KWHIST01");

class KWalletHistoryKeyProvider final : public HistoryKeyProvider {
public:
  void loadOrCreate(QObject *context, LoadCallback callback) override {
    KWallet::Wallet *wallet = openWallet();
    if (!wallet) {
      callback(std::nullopt, i18n("Secure history requires an available, unlocked KDE Wallet."));
      return;
    }
    wallet->setParent(context);
    QObject::connect(wallet, &KWallet::Wallet::walletOpened, context,
                     [wallet, callback = std::move(callback)](bool success) mutable {
                       if (!success) {
                         callback(
                             std::nullopt,
                             i18n("Secure history requires an available, unlocked KDE Wallet."));
                         wallet->deleteLater();
                         return;
                       }
                       QString error;
                       std::optional<QByteArray> key = loadKey(wallet, &error);
                       callback(std::move(key), error);
                       wallet->deleteLater();
                     });
  }

  void remove(QObject *context, RemoveCallback callback) override {
    KWallet::Wallet *wallet = openWallet();
    if (!wallet) {
      callback(false, i18n("The secure history key could not be removed from KDE Wallet."));
      return;
    }
    wallet->setParent(context);
    QObject::connect(wallet, &KWallet::Wallet::walletOpened, context,
                     [wallet, callback = std::move(callback)](bool success) mutable {
                       QString error;
                       const bool removed = success && removeKey(wallet, &error);
                       if (!success)
                         error =
                             i18n("The secure history key could not be removed from KDE Wallet.");
                       callback(removed, error);
                       wallet->deleteLater();
                     });
  }

private:
  static WId parentWindowId() {
    QWindow *window = QGuiApplication::focusWindow();
    if (!window)
      window = QGuiApplication::allWindows().value(0, nullptr);
    return window ? window->winId() : 0;
  }

  static KWallet::Wallet *openWallet() {
    return KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(), parentWindowId(),
                                       KWallet::Wallet::Asynchronous);
  }

  static std::optional<QByteArray> loadKey(KWallet::Wallet *wallet, QString *error) {
    if (!wallet->hasFolder(QString::fromLatin1(walletFolder)) &&
        !wallet->createFolder(QString::fromLatin1(walletFolder))) {
      *error = i18n("Kastword could not create its secure wallet folder.");
      return std::nullopt;
    }
    if (!wallet->setFolder(QString::fromLatin1(walletFolder))) {
      *error = i18n("Kastword could not open its secure wallet folder.");
      return std::nullopt;
    }
    QByteArray key;
    if (wallet->hasEntry(QString::fromLatin1(walletEntry))) {
      if (wallet->readEntry(QString::fromLatin1(walletEntry), key) != 0 ||
          key.size() != crypto_aead_xchacha20poly1305_ietf_KEYBYTES) {
        *error = i18n("The secure history key could not be read.");
        return std::nullopt;
      }
      return key;
    }
    key.resize(crypto_aead_xchacha20poly1305_ietf_KEYBYTES);
    randombytes_buf(key.data(), size_t(key.size()));
    if (wallet->writeEntry(QString::fromLatin1(walletEntry), key) != 0) {
      sodium_memzero(key.data(), size_t(key.size()));
      *error = i18n("The secure history key could not be stored.");
      return std::nullopt;
    }
    return key;
  }

  static bool removeKey(KWallet::Wallet *wallet, QString *error) {
    if (!wallet->hasFolder(QString::fromLatin1(walletFolder)))
      return true;
    if (!wallet->setFolder(QString::fromLatin1(walletFolder)) ||
        (wallet->hasEntry(QString::fromLatin1(walletEntry)) &&
         wallet->removeEntry(QString::fromLatin1(walletEntry)) != 0)) {
      *error = i18n("The secure history key could not be removed from KDE Wallet.");
      return false;
    }
    return true;
  }
};
} // namespace

std::unique_ptr<HistoryKeyProvider> createHistoryKeyProvider() {
  return std::make_unique<KWalletHistoryKeyProvider>();
}

DictationHistory::DictationHistory(QObject *parent)
    : DictationHistory(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
                           QStringLiteral("/") + QString::fromLatin1(historyFileName),
                       createHistoryKeyProvider(), {}, {}, parent, true) {}

DictationHistory::DictationHistory(QString storagePath,
                                   std::unique_ptr<HistoryKeyProvider> keyProvider, Clock clock,
                                   CommitFunction commit, QObject *parent,
                                   bool asynchronousPersistence)
    : QAbstractListModel(parent), m_storagePath(std::move(storagePath)),
      m_keyProvider(std::move(keyProvider)),
      m_clock(clock ? std::move(clock) : [] { return QDateTime::currentDateTimeUtc(); }),
      m_commit(commit ? std::move(commit) : &DictationHistory::commitAtomically),
      m_asynchronousPersistence(asynchronousPersistence) {
  Q_ASSERT(m_keyProvider);
  m_expiryTimer.setSingleShot(true);
  connect(&m_expiryTimer, &QTimer::timeout, this, &DictationHistory::expireEntries);
  connect(&m_saveWatcher, &QFutureWatcherBase::finished, this, &DictationHistory::finishAsyncSave);
  if (sodium_init() < 0) {
    m_cryptoAvailable = false;
    fail(i18n("Secure history encryption is unavailable."));
  }
}

DictationHistory::~DictationHistory() {
  if (m_saving) {
    m_saveWatcher.waitForFinished();
    persistEntries(m_entries, m_key, m_storagePath, m_commit);
  }
  if (!m_key.isEmpty())
    sodium_memzero(m_key.data(), size_t(m_key.size()));
}

QVariantMap DictationHistory::entryMap(const Entry &entry) const {
  return {{QStringLiteral("id"), entry.id},
          {QStringLiteral("createdAt"), entry.createdAt},
          {QStringLiteral("createdText"),
           QLocale().toString(entry.createdAt.toLocalTime(), QLocale::ShortFormat)},
          {QStringLiteral("text"), entry.text}};
}

QVariantList DictationHistory::entries() const {
  QVariantList result;
  for (const Entry &entry : m_entries)
    result.append(entryMap(entry));
  return result;
}

int DictationHistory::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : int(m_entries.size());
}

QVariant DictationHistory::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
    return {};
  const Entry &entry = m_entries.at(index.row());
  switch (role) {
  case EntryIdRole:
    return entry.id;
  case CreatedAtRole:
    return entry.createdAt;
  case CreatedTextRole:
    return QLocale().toString(entry.createdAt.toLocalTime(), QLocale::ShortFormat);
  case TextRole:
    return entry.text;
  default:
    return {};
  }
}

QHash<int, QByteArray> DictationHistory::roleNames() const {
  return {{EntryIdRole, "entryId"},
          {CreatedAtRole, "createdAt"},
          {CreatedTextRole, "createdText"},
          {TextRole, "text"}};
}

void DictationHistory::replaceEntries(QList<Entry> entries) {
  beginResetModel();
  m_entries = std::move(entries);
  endResetModel();
}

QVariantList DictationHistory::recentEntries() const {
  QVariantList result;
  for (qsizetype i = 0; i < qMin<qsizetype>(3, m_entries.size()); ++i)
    result.append(entryMap(m_entries.at(i)));
  return result;
}

bool DictationHistory::deletionPending() const { return QFileInfo::exists(deletionMarkerPath()); }

QString DictationHistory::deletionMarkerPath() const {
  return m_storagePath + QStringLiteral(".delete-pending");
}

void DictationHistory::enable() {
  if (m_enabled)
    return;
  if (!m_cryptoAvailable || m_busy)
    return;
  m_available = true;
  m_resetRequired = false;
  m_busy = true;
  m_status = i18n("Opening KDE Wallet…");
  emit changed();
  m_keyProvider->loadOrCreate(this, [this](std::optional<QByteArray> key, const QString &error) {
    m_busy = false;
    if (!key || key->size() != crypto_aead_xchacha20poly1305_ietf_KEYBYTES) {
      fail(error.isEmpty() ? i18n("The secure history key is unavailable.") : error);
      return;
    }
    m_key = *key;
    sodium_memzero(key->data(), size_t(key->size()));
    key->clear();
    QString directoryError;
    if (!secureStorageDirectory(m_storagePath, &directoryError)) {
      sodium_memzero(m_key.data(), size_t(m_key.size()));
      m_key.clear();
      fail(directoryError);
      return;
    }
    m_enabled = true;
    if (!load()) {
      m_enabled = false;
      sodium_memzero(m_key.data(), size_t(m_key.size()));
      m_key.clear();
      emit changed();
      return;
    }
    m_status = i18n("History is encrypted locally. The key is stored in KDE Wallet.");
    scheduleExpiry();
    emit settingsChanged();
    emit changed();
  });
}

void DictationHistory::disable(bool deleteData) {
  if (busy())
    return;
  m_expiryTimer.stop();
  if (deleteData) {
    QString error;
    if (!deletionPending() &&
        !commitAtomically(deletionMarkerPath(), QByteArrayLiteral("pending\n"), &error)) {
      fail(error);
      return;
    }
    resumePendingDeletion();
    return;
  }
  replaceEntries({});
  m_persistedEntries.clear();
  if (!m_key.isEmpty())
    sodium_memzero(m_key.data(), size_t(m_key.size()));
  m_key.clear();
  m_enabled = false;
  m_available = true;
  m_resetRequired = false;
  m_status = i18n("History is disabled. Existing encrypted history was kept.");
  emit settingsChanged();
  emit changed();
}

void DictationHistory::resumePendingDeletion() {
  if (!deletionPending() || busy())
    return;
  m_expiryTimer.stop();
  if (QFileInfo::exists(m_storagePath) && !QFile::remove(m_storagePath)) {
    fail(i18n("The encrypted history file could not be deleted."));
    return;
  }
  replaceEntries({});
  m_persistedEntries.clear();
  if (!m_key.isEmpty())
    sodium_memzero(m_key.data(), size_t(m_key.size()));
  m_key.clear();
  m_enabled = false;
  m_busy = true;
  m_status = i18n("Removing the secure history key…");
  emit settingsChanged();
  emit changed();
  removePendingKey();
}

void DictationHistory::removePendingKey() {
  m_keyProvider->remove(this, [this](bool removed, const QString &error) {
    m_busy = false;
    if (!removed) {
      fail(error.isEmpty() ? i18n("The encrypted history could not be deleted completely.")
                           : error);
      return;
    }
    if (QFileInfo::exists(deletionMarkerPath()) && !QFile::remove(deletionMarkerPath())) {
      fail(i18n("The secure history deletion marker could not be removed."));
      return;
    }
    m_available = true;
    m_resetRequired = false;
    m_status = i18n("Encrypted history was deleted.");
    emit changed();
  });
}

bool DictationHistory::add(const QString &text) {
  const QString trimmed = text.trimmed();
  if (!m_enabled || trimmed.isEmpty())
    return false;
  QList<Entry> entries = m_entries;
  entries.prepend({QUuid::createUuid().toString(QUuid::WithoutBraces), m_clock(), trimmed});
  pruneEntries(entries, m_maximumEntries, m_maximumAgeDays);
  if (!saveEntries(entries))
    return false;
  replaceEntries(std::move(entries));
  scheduleExpiry();
  emit changed();
  return true;
}

bool DictationHistory::removeEntry(const QString &id) {
  if (!m_enabled)
    return false;
  QList<Entry> entries = m_entries;
  const qsizetype removed = entries.removeIf([&id](const Entry &entry) { return entry.id == id; });
  if (removed == 0)
    return false;
  if (!saveEntries(entries))
    return false;
  replaceEntries(std::move(entries));
  scheduleExpiry();
  emit changed();
  return true;
}

bool DictationHistory::clear() {
  if (!m_enabled)
    return false;
  if (!saveEntries({}))
    return false;
  replaceEntries({});
  scheduleExpiry();
  emit changed();
  return true;
}

void DictationHistory::setMaximumEntries(int value) {
  value = qBound(1, value, 10000);
  if (m_maximumEntries == value)
    return;
  if (m_enabled) {
    QList<Entry> entries = m_entries;
    if (pruneEntries(entries, value, m_maximumAgeDays)) {
      if (!saveEntries(entries))
        return;
      replaceEntries(std::move(entries));
    }
  }
  m_maximumEntries = value;
  scheduleExpiry();
  emit settingsChanged();
  emit changed();
}

void DictationHistory::setMaximumAgeDays(int value) {
  value = qBound(1, value, 3650);
  if (m_maximumAgeDays == value)
    return;
  if (m_enabled) {
    QList<Entry> entries = m_entries;
    if (pruneEntries(entries, m_maximumEntries, value)) {
      if (!saveEntries(entries))
        return;
      replaceEntries(std::move(entries));
    }
  }
  m_maximumAgeDays = value;
  scheduleExpiry();
  emit settingsChanged();
  emit changed();
}

void DictationHistory::scheduleExpiry() {
  m_expiryTimer.stop();
  if (!m_enabled || m_entries.isEmpty())
    return;
  QDateTime nextExpiry;
  for (const Entry &entry : m_entries) {
    const QDateTime expiry = entry.createdAt.addDays(m_maximumAgeDays);
    if (!nextExpiry.isValid() || expiry < nextExpiry)
      nextExpiry = expiry;
  }
  const qint64 delay = qMax<qint64>(1, m_clock().msecsTo(nextExpiry) + 1);
  m_expiryTimer.start(int(qMin<qint64>(delay, std::numeric_limits<int>::max())));
}

void DictationHistory::expireEntries() {
  QList<Entry> entries = m_entries;
  if (!pruneEntries(entries, m_maximumEntries, m_maximumAgeDays)) {
    scheduleExpiry();
    return;
  }
  if (!saveEntries(entries)) {
    m_expiryTimer.start(60000);
    return;
  }
  replaceEntries(std::move(entries));
  emit changed();
  scheduleExpiry();
}

bool DictationHistory::pruneEntries(QList<Entry> &entries, int maximumEntries,
                                    int maximumAgeDays) const {
  const QDateTime oldest = m_clock().addDays(-maximumAgeDays);
  const qsizetype before = entries.size();
  entries.removeIf([&oldest](const Entry &entry) { return entry.createdAt < oldest; });
  while (entries.size() > maximumEntries)
    entries.removeLast();
  return before != entries.size();
}

bool DictationHistory::load() {
  m_entries.clear();
  QFile file(m_storagePath);
  if (!file.exists()) {
    m_persistedEntries.clear();
    return true;
  }
  if (!file.open(QIODevice::ReadOnly)) {
    fail(i18n("The encrypted history file could not be opened."));
    return false;
  }
  if (file.size() > maximumHistoryFileSize) {
    fail(i18n("The encrypted history file is too large to open safely."));
    return false;
  }
  const QByteArray stored = file.read(maximumHistoryFileSize + 1);
  if (stored.size() > maximumHistoryFileSize || !file.atEnd()) {
    fail(i18n("The encrypted history file is too large to open safely."));
    return false;
  }
  const qsizetype overhead = magic.size() + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES +
                             crypto_aead_xchacha20poly1305_ietf_ABYTES;
  if (stored.size() < overhead || !stored.startsWith(magic)) {
    fail(i18n("The encrypted history file is damaged or unsupported."), true);
    return false;
  }
  const QByteArray nonce = stored.mid(magic.size(), crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
  const QByteArray cipher = stored.mid(magic.size() + nonce.size());
  QByteArray plain(cipher.size() - crypto_aead_xchacha20poly1305_ietf_ABYTES, Qt::Uninitialized);
  unsigned long long plainLength = 0;
  if (crypto_aead_xchacha20poly1305_ietf_decrypt(
          reinterpret_cast<unsigned char *>(plain.data()), &plainLength, nullptr,
          reinterpret_cast<const unsigned char *>(cipher.constData()),
          static_cast<unsigned long long>(cipher.size()),
          reinterpret_cast<const unsigned char *>(magic.constData()),
          static_cast<unsigned long long>(magic.size()),
          reinterpret_cast<const unsigned char *>(nonce.constData()),
          reinterpret_cast<const unsigned char *>(m_key.constData())) != 0) {
    fail(i18n("The encrypted history could not be authenticated or decrypted."), true);
    return false;
  }
  plain.resize(qsizetype(plainLength));
  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(plain, &parseError);
  sodium_memzero(plain.data(), size_t(plain.size()));
  const QJsonObject root = document.object();
  if (parseError.error != QJsonParseError::NoError ||
      root.value(QStringLiteral("version")).toInt() != 1) {
    fail(i18n("The decrypted history format is damaged or unsupported."), true);
    return false;
  }
  const QJsonArray storedEntries = root.value(QStringLiteral("entries")).toArray();
  if (storedEntries.size() > 10000) {
    fail(i18n("The decrypted history contains too many entries."), true);
    return false;
  }
  QList<Entry> loadedEntries;
  for (const QJsonValue &value : storedEntries) {
    const QJsonObject object = value.toObject();
    const QDateTime createdAt = QDateTime::fromString(
        object.value(QStringLiteral("createdAt")).toString(), Qt::ISODateWithMs);
    const QString id = object.value(QStringLiteral("id")).toString();
    const QString text = object.value(QStringLiteral("text")).toString();
    if (!createdAt.isValid() || id.isEmpty() || text.isEmpty()) {
      fail(i18n("The decrypted history contains an invalid entry."), true);
      return false;
    }
    loadedEntries.append({id, createdAt, text});
  }
  std::stable_sort(
      loadedEntries.begin(), loadedEntries.end(),
      [](const Entry &left, const Entry &right) { return left.createdAt > right.createdAt; });
  if (pruneEntries(loadedEntries, m_maximumEntries, m_maximumAgeDays) &&
      !saveEntries(loadedEntries))
    return false;
  replaceEntries(std::move(loadedEntries));
  m_persistedEntries = m_entries;
  return true;
}

bool DictationHistory::saveEntries(const QList<Entry> &historyEntries) {
  if (!m_enabled || m_key.size() != crypto_aead_xchacha20poly1305_ietf_KEYBYTES)
    return false;
  if (m_asynchronousPersistence) {
    if (m_saving)
      m_pendingSave = historyEntries;
    else
      startAsyncSave(historyEntries);
    return true;
  }

  SaveResult result = persistEntries(historyEntries, m_key, m_storagePath, m_commit);
  if (!result.success) {
    fail(result.error);
    return false;
  }
  m_persistedEntries = historyEntries;
  m_available = true;
  m_resetRequired = false;
  m_status = i18n("History is encrypted locally. The key is stored in KDE Wallet.");
  return true;
}

void DictationHistory::startAsyncSave(QList<Entry> entries) {
  m_saving = true;
  emit changed();
  QByteArray key(m_key.constData(), m_key.size());
  const QString path = m_storagePath;
  const CommitFunction commit = m_commit;
  m_saveWatcher.setFuture(QtConcurrent::run(
      [entries = std::move(entries), key = std::move(key), path, commit]() mutable {
        return persistEntries(std::move(entries), std::move(key), path, commit);
      }));
}

void DictationHistory::finishAsyncSave() {
  SaveResult result = m_saveWatcher.result();
  if (!result.success) {
    m_pendingSave.reset();
    m_saving = false;
    replaceEntries(m_persistedEntries);
    fail(result.error);
    return;
  }
  m_persistedEntries = std::move(result.entries);
  if (m_pendingSave) {
    QList<Entry> pending = std::move(*m_pendingSave);
    m_pendingSave.reset();
    startAsyncSave(std::move(pending));
    return;
  }
  m_saving = false;
  m_available = true;
  m_resetRequired = false;
  m_status = i18n("History is encrypted locally. The key is stored in KDE Wallet.");
  emit changed();
}

DictationHistory::SaveResult DictationHistory::persistEntries(QList<Entry> historyEntries,
                                                              QByteArray key, const QString &path,
                                                              const CommitFunction &commit) {
  SaveResult outcome{std::move(historyEntries), false, {}};
  QJsonArray entries;
  for (const Entry &entry : outcome.entries) {
    entries.append(QJsonObject{
        {QStringLiteral("id"), entry.id},
        {QStringLiteral("createdAt"), entry.createdAt.toUTC().toString(Qt::ISODateWithMs)},
        {QStringLiteral("text"), entry.text}});
  }
  QByteArray plain = QJsonDocument(QJsonObject{{QStringLiteral("version"), 1},
                                               {QStringLiteral("entries"), entries}})
                         .toJson(QJsonDocument::Compact);
  QByteArray nonce(crypto_aead_xchacha20poly1305_ietf_NPUBBYTES, Qt::Uninitialized);
  randombytes_buf(nonce.data(), size_t(nonce.size()));
  QByteArray cipher(plain.size() + crypto_aead_xchacha20poly1305_ietf_ABYTES, Qt::Uninitialized);
  unsigned long long cipherLength = 0;
  const int result = crypto_aead_xchacha20poly1305_ietf_encrypt(
      reinterpret_cast<unsigned char *>(cipher.data()), &cipherLength,
      reinterpret_cast<const unsigned char *>(plain.constData()),
      static_cast<unsigned long long>(plain.size()),
      reinterpret_cast<const unsigned char *>(magic.constData()),
      static_cast<unsigned long long>(magic.size()), nullptr,
      reinterpret_cast<const unsigned char *>(nonce.constData()),
      reinterpret_cast<const unsigned char *>(key.constData()));
  sodium_memzero(plain.data(), size_t(plain.size()));
  sodium_memzero(key.data(), size_t(key.size()));
  if (result != 0) {
    outcome.error = i18n("The history could not be encrypted.");
    return outcome;
  }
  cipher.resize(qsizetype(cipherLength));
  QString error;
  if (!commit(path, magic + nonce + cipher, &error)) {
    outcome.error = error.isEmpty() ? i18n("The encrypted history could not be saved.") : error;
    return outcome;
  }
  outcome.success = true;
  return outcome;
}

bool DictationHistory::commitAtomically(const QString &path, const QByteArray &data,
                                        QString *error) {
  if (!secureStorageDirectory(path, error))
    return false;
  QSaveFile file(path);
  file.setDirectWriteFallback(false);
  if (!file.open(QIODevice::WriteOnly) ||
      !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
      file.write(data) != data.size() || !file.commit()) {
    file.cancelWriting();
    *error = i18n("The encrypted history could not be written atomically.");
    return false;
  }
  return true;
}

bool DictationHistory::secureStorageDirectory(const QString &path, QString *error) {
  const QString directoryPath = QFileInfo(path).absolutePath();
  if (QDir().mkpath(directoryPath) &&
      QFile::setPermissions(directoryPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                               QFileDevice::ExeOwner))
    return true;
  *error = i18n("The private history directory could not be secured.");
  return false;
}

void DictationHistory::fail(const QString &message, bool resetRequired) {
  m_available = false;
  m_resetRequired = resetRequired;
  m_status = message;
  emit changed();
}
