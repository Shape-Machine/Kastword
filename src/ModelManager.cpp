// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ModelManager.h"

#include <KLocalizedString>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QtConcurrentRun>
#include <whisper.h>

namespace {
QString localizedModelName(const QString &id) {
  if (id == QStringLiteral("tiny.en"))
    return i18n("Tiny English");
  if (id == QStringLiteral("base.en"))
    return i18n("Base English");
  if (id == QStringLiteral("small.en"))
    return i18n("Small English");
  if (id == QStringLiteral("medium.en"))
    return i18n("Medium English");
  if (id == QStringLiteral("tiny"))
    return i18n("Tiny Multilingual");
  if (id == QStringLiteral("base"))
    return i18n("Base Multilingual");
  if (id == QStringLiteral("small"))
    return i18n("Small Multilingual");
  if (id == QStringLiteral("medium"))
    return i18n("Medium Multilingual");
  if (id == QStringLiteral("large-v1"))
    return i18n("Large v1 Multilingual");
  if (id == QStringLiteral("large-v2"))
    return i18n("Large v2 Multilingual");
  if (id == QStringLiteral("large-v3"))
    return i18n("Large v3 Multilingual");
  if (id == QStringLiteral("large-v3-turbo"))
    return i18n("Large v3 Turbo Multilingual");
  return i18n("Unknown model");
}

QString localizedSpeed(const QString &speed) {
  if (speed == QStringLiteral("Fastest"))
    return i18n("Fastest");
  if (speed == QStringLiteral("Fast"))
    return i18n("Fast");
  if (speed == QStringLiteral("Moderate"))
    return i18n("Moderate");
  if (speed == QStringLiteral("Slow"))
    return i18n("Slow");
  if (speed == QStringLiteral("Slowest"))
    return i18n("Slowest");
  return i18n("Unknown speed");
}

QString localizedAccuracy(const QString &accuracy) {
  if (accuracy == QStringLiteral("Basic"))
    return i18n("Basic accuracy");
  if (accuracy == QStringLiteral("Good"))
    return i18n("Good accuracy");
  if (accuracy == QStringLiteral("Better"))
    return i18n("Better accuracy");
  if (accuracy == QStringLiteral("High"))
    return i18n("High accuracy");
  if (accuracy == QStringLiteral("Highest"))
    return i18n("Highest accuracy");
  return i18n("Unknown accuracy");
}
} // namespace

ModelManager::ModelManager(QObject *parent)
    : ModelManager(whisperModelCatalog(),
                   QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
                       QStringLiteral("/models"),
                   nullptr, parent) {}

ModelManager::ModelManager(QList<ModelCatalogEntry> catalog, QString storagePath,
                           QNetworkAccessManager *network, QObject *parent,
                           ModelValidationFunction validator, HashFunction hasher,
                           FileRemoveFunction removeFile)
    : QObject(parent), m_catalog(std::move(catalog)), m_storagePath(std::move(storagePath)),
      m_network(network), m_validator(std::move(validator)), m_hasher(std::move(hasher)),
      m_removeFile(std::move(removeFile)) {
  Q_ASSERT(validateModelCatalog(m_catalog).isEmpty());
  if (!m_network) {
    m_ownedNetwork = std::make_unique<QNetworkAccessManager>();
    m_network = m_ownedNetwork.get();
  }
  if (!m_validator) {
    m_validator = [](const QString &path) {
      whisper_context *context = whisper_init_from_file_with_params(
          path.toUtf8().constData(), whisper_context_default_params());
      if (!context)
        return ModelValidationResult{};
      const bool englishOnly = !whisper_is_multilingual(context);
      whisper_free(context);
      return ModelValidationResult{true, englishOnly};
    };
  }
  if (!m_hasher)
    m_hasher = &ModelManager::hashFile;
  if (!m_removeFile)
    m_removeFile = [](const QString &path) { return QFile::remove(path); };
  connect(&m_hashWatcher, &QFutureWatcher<HashResult>::finished, this,
          &ModelManager::finishVerification);
  connect(&m_modelValidationWatcher, &QFutureWatcher<ModelValidationResult>::finished, this,
          &ModelManager::finishLocalModelValidation);
  connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &ModelManager::revalidateActiveModel);
}

QVariantList ModelManager::models() const {
  QVariantList result;
  for (const ModelCatalogEntry &item : m_catalog) {
    const QString path = modelPath(item);
    const QString partial = partialPath(item);
    const bool installed = QFileInfo(path).isFile() && QFileInfo(path).size() == item.size;
    const qint64 partialSize = QFileInfo(partial).isFile() ? QFileInfo(partial).size() : 0;
    result.append(QVariantMap{
        {QStringLiteral("id"), item.id},
        {QStringLiteral("name"), localizedModelName(item.id)},
        {QStringLiteral("fileName"), item.fileName},
        {QStringLiteral("url"), item.url.toString(QUrl::FullyEncoded)},
        {QStringLiteral("size"), item.size},
        {QStringLiteral("sizeText"), QLocale().formattedDataSize(item.size)},
        {QStringLiteral("englishOnly"), item.englishOnly},
        {QStringLiteral("languageText"),
         item.englishOnly ? i18n("English only") : i18n("Multilingual")},
        {QStringLiteral("recommended"), item.recommended},
        {QStringLiteral("speed"), localizedSpeed(item.speed)},
        {QStringLiteral("accuracy"), localizedAccuracy(item.accuracy)},
        {QStringLiteral("installed"), installed},
        {QStringLiteral("partial"), partialSize > 0},
        {QStringLiteral("partialSize"), partialSize},
        {QStringLiteral("partialSizeText"), QLocale().formattedDataSize(partialSize)},
        {QStringLiteral("active"), path == m_activeModelPath},
        {QStringLiteral("downloading"), item.id == m_currentModelId && m_reply},
        {QStringLiteral("verifying"), item.id == m_currentModelId && m_hashWatcher.isRunning()},
    });
  }
  return result;
}

bool ModelManager::isStructurallyValidModel(const QString &path) {
  QFile file(path);
  const QFileInfo info(file);
  return info.isFile() && info.size() > 4 && info.size() <= 4LL * 1024 * 1024 * 1024 &&
         file.open(QIODevice::ReadOnly) && file.read(4) == QByteArray::fromHex("6c6d6767");
}

const ModelCatalogEntry *ModelManager::entry(const QString &id) const {
  for (const ModelCatalogEntry &item : m_catalog) {
    if (item.id == id)
      return &item;
  }
  return nullptr;
}

const ModelCatalogEntry *ModelManager::entryForPath(const QString &path) const {
  const QString canonical = QFileInfo(path).absoluteFilePath();
  for (const ModelCatalogEntry &item : m_catalog) {
    if (QFileInfo(modelPath(item)).absoluteFilePath() == canonical)
      return &item;
  }
  return nullptr;
}

QString ModelManager::modelPath(const ModelCatalogEntry &item) const {
  return QDir(m_storagePath).filePath(item.fileName);
}

QString ModelManager::partialPath(const ModelCatalogEntry &item) const {
  return modelPath(item) + QStringLiteral(".part");
}

void ModelManager::download(const QString &id) {
  if (busy())
    return;
  const ModelCatalogEntry *item = entry(id);
  if (!item) {
    setFailure(i18n("The selected model is not available."));
    return;
  }
  QDir().mkpath(m_storagePath);
  const QString partial = partialPath(*item);
  qint64 partialSize = QFileInfo(partial).size();
  if (partialSize < 0 || partialSize > item->size) {
    QFile::remove(partial);
    partialSize = 0;
  }
  const QStorageInfo storage(m_storagePath);
  if (storage.isValid() && storage.bytesAvailable() < item->size - qMax<qint64>(0, partialSize)) {
    setFailure(i18n("There is not enough free disk space for this model."));
    return;
  }
  m_error.clear();
  m_currentModelId = id;
  if (partialSize == item->size) {
    verifyFile(*item, partial, true);
    return;
  }
  const QString installedPath = modelPath(*item);
  if (QFileInfo(installedPath).size() == item->size) {
    verifyFile(*item, installedPath, false);
    return;
  }
  beginRequest(*item);
}

void ModelManager::beginRequest(const ModelCatalogEntry &item) {
  m_partialFile.setFileName(partialPath(item));
  m_resumeOffset = QFileInfo(m_partialFile).size();
  if (m_resumeOffset < 0 || m_resumeOffset >= item.size) {
    QFile::remove(m_partialFile.fileName());
    m_resumeOffset = 0;
  }
  if (!m_partialFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
    setFailure(i18n("The model download file could not be created."));
    return;
  }
  QNetworkRequest request(item.url);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);
  if (m_resumeOffset > 0)
    request.setRawHeader("Range", QByteArrayLiteral("bytes=") + QByteArray::number(m_resumeOffset) +
                                      QByteArrayLiteral("-"));
  m_status = i18n("Downloading %1…", localizedModelName(item.id));
  m_progress = qreal(m_resumeOffset) / qreal(item.size);
  m_reply = m_network->get(request);
  connect(m_reply, &QNetworkReply::metaDataChanged, this, [this] {
    if (!m_reply || m_resumeOffset == 0)
      return;
    const int status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status != 206) {
      m_partialFile.resize(0);
      m_partialFile.seek(0);
      m_resumeOffset = 0;
    }
  });
  connect(m_reply, &QIODevice::readyRead, this, &ModelManager::consumeReplyData);
  connect(m_reply, &QNetworkReply::downloadProgress, this,
          [this, size = item.size](qint64 received, qint64) {
            m_progress = qBound(0.0, qreal(m_resumeOffset + received) / qreal(size), 1.0);
            emit changed();
          });
  connect(m_reply, &QNetworkReply::finished, this, &ModelManager::finishRequest);
  emit changed();
}

void ModelManager::consumeReplyData() {
  if (!m_reply)
    return;
  const ModelCatalogEntry *item = entry(m_currentModelId);
  const QByteArray data = m_reply->readAll();
  if (!item || m_partialFile.size() + data.size() > item->size ||
      m_partialFile.write(data) != data.size()) {
    m_reply->abort();
    setFailure(i18n("The model download could not be saved safely."));
  }
}

void ModelManager::finishRequest() {
  if (!m_reply)
    return;
  consumeReplyData();
  QNetworkReply *reply = m_reply;
  m_reply = nullptr;
  m_partialFile.close();
  const auto error = reply->error();
  reply->deleteLater();
  if (error == QNetworkReply::OperationCanceledError) {
    if (m_error.isEmpty()) {
      m_status = i18n("Download cancelled. You can resume it later.");
      m_currentModelId.clear();
      m_progress = 0.0;
      emit changed();
    }
    return;
  }
  if (error != QNetworkReply::NoError) {
    setFailure(i18n("The model download failed. Check the connection and try again."));
    return;
  }
  const ModelCatalogEntry *item = entry(m_currentModelId);
  if (!item || QFileInfo(partialPath(*item)).size() != item->size) {
    setFailure(i18n("The downloaded model has an unexpected size."));
    return;
  }
  verifyFile(*item, partialPath(*item), true);
}

void ModelManager::verifyFile(const ModelCatalogEntry &item, const QString &path,
                              bool installAfterVerify) {
  m_currentModelId = item.id;
  m_verificationPath = path;
  m_installAfterVerify = installAfterVerify;
  m_status = i18n("Verifying %1…", localizedModelName(item.id));
  m_progress = 1.0;
  m_hashCancelled = std::make_shared<std::atomic_bool>(false);
  m_hashWatcher.setFuture(QtConcurrent::run(m_hasher, path, m_hashCancelled));
  emit changed();
}

void ModelManager::finishVerification() {
  const ModelCatalogEntry *item = entry(m_currentModelId);
  const HashResult result = m_hashWatcher.result();
  if (m_hashCancelled && m_hashCancelled->load()) {
    const bool restoring = m_restoringActiveModel;
    m_restoringActiveModel = false;
    m_hashCancelled.reset();
    m_status = i18n("Model verification cancelled. You can resume it later.");
    m_error.clear();
    m_currentModelId.clear();
    m_progress = 0.0;
    emit changed();
    if (restoring)
      emit setupRequired();
    return;
  }
  m_hashCancelled.reset();
  if (!item || !result) {
    const bool restoring = m_restoringActiveModel;
    m_restoringActiveModel = false;
    setFailure(i18n("The model could not be read for verification."));
    if (restoring)
      emit setupRequired();
    return;
  }
  if (*result != item->sha256 || !isStructurallyValidModel(m_verificationPath)) {
    QFile::remove(m_verificationPath);
    const bool restoring = m_restoringActiveModel;
    m_restoringActiveModel = false;
    setFailure(i18n("The downloaded model failed verification and was removed."));
    if (restoring)
      emit setupRequired();
    return;
  }
  QString path = m_verificationPath;
  if (m_installAfterVerify) {
    const QString destination = modelPath(*item);
    QFile::remove(destination);
    if (!QFile::rename(path, destination)) {
      setFailure(i18n("The verified model could not be installed."));
      return;
    }
    path = destination;
  }
  m_restoringActiveModel = false;
  activatePath(path, item->englishOnly);
  m_status = i18n("%1 is ready for offline dictation.", localizedModelName(item->id));
  m_error.clear();
  m_currentModelId.clear();
  m_progress = 0.0;
  emit changed();
}

void ModelManager::cancel() {
  if (m_reply)
    m_reply->abort();
  if (m_hashCancelled)
    m_hashCancelled->store(true);
}

void ModelManager::selectModel(const QString &id) {
  if (busy())
    return;
  const ModelCatalogEntry *item = entry(id);
  if (!item || QFileInfo(modelPath(*item)).size() != item->size) {
    setFailure(i18n("Download this model before selecting it."));
    return;
  }
  verifyFile(*item, modelPath(*item), false);
}

bool ModelManager::selectLocalModel(const QUrl &url) {
  if (busy() || !url.isLocalFile() || !isStructurallyValidModel(url.toLocalFile())) {
    setFailure(i18n("Select a valid local Whisper model file."));
    return false;
  }
  validateLocalModel(url.toLocalFile(), false);
  return true;
}

void ModelManager::validateLocalModel(const QString &path, bool restoring) {
  m_pendingLocalModelPath = path;
  m_restoringActiveModel = restoring;
  m_error.clear();
  m_status = restoring ? i18n("Verifying the saved speech model…")
                       : i18n("Verifying the selected speech model…");
  m_modelValidationWatcher.setFuture(QtConcurrent::run(m_validator, path));
  emit changed();
}

void ModelManager::finishLocalModelValidation() {
  const QString path = m_pendingLocalModelPath;
  const bool restoring = m_restoringActiveModel;
  const ModelValidationResult result = m_modelValidationWatcher.result();
  m_pendingLocalModelPath.clear();
  m_restoringActiveModel = false;
  if (!result.valid || !isStructurallyValidModel(path)) {
    setFailure(i18n("The selected file is not a compatible Whisper model."));
    if (restoring)
      emit setupRequired();
    return;
  }
  activatePath(path, result.englishOnly);
  m_status = i18n("The local model is ready for offline dictation.");
  m_error.clear();
  emit changed();
}

bool ModelManager::removeModel(const QString &id) {
  if (busy())
    return false;
  const ModelCatalogEntry *item = entry(id);
  if (!item)
    return false;
  const QString path = modelPath(*item);
  const QString partial = partialPath(*item);
  if (QFileInfo::exists(partial) && !m_removeFile(partial)) {
    setFailure(i18n("The model could not be removed."));
    return false;
  }
  const bool removed = !QFileInfo::exists(path) || m_removeFile(path);
  if (removed) {
    if (path == m_activeModelPath)
      clearActiveModel();
    m_status = i18n("Model removed.");
    emit changed();
    if (!modelReady())
      emit setupRequired();
  } else {
    setFailure(i18n("The model could not be removed."));
  }
  return removed;
}

void ModelManager::restoreActiveModel(const QString &path) {
  if (path.isEmpty() || !isStructurallyValidModel(path)) {
    clearActiveModel();
    emit setupRequired();
    return;
  }
  if (const ModelCatalogEntry *item = entryForPath(path)) {
    if (QFileInfo(path).size() == item->size) {
      m_restoringActiveModel = true;
      verifyFile(*item, path, false);
      return;
    }
    clearActiveModel();
    emit setupRequired();
    return;
  }
  validateLocalModel(path, true);
}

void ModelManager::activatePath(const QString &path, bool englishOnly) {
  if (m_activeModelPath == path && m_activeModelEnglishOnly == englishOnly)
    return;
  const bool pathChanged = m_activeModelPath != path;
  m_activeModelPath = path;
  m_activeModelEnglishOnly = englishOnly;
  if (pathChanged)
    watchActiveModel();
  emit activeModelPathChanged();
  emit changed();
}

void ModelManager::clearActiveModel() {
  if (m_activeModelPath.isEmpty())
    return;
  m_activeModelPath.clear();
  m_activeModelEnglishOnly = false;
  watchActiveModel();
  emit activeModelPathChanged();
  emit changed();
}

void ModelManager::setFailure(const QString &message) {
  m_error = message;
  m_status.clear();
  m_currentModelId.clear();
  m_progress = 0.0;
  emit changed();
}

void ModelManager::watchActiveModel() {
  const QStringList watched = m_watcher.files();
  if (!watched.isEmpty())
    m_watcher.removePaths(watched);
  if (!m_activeModelPath.isEmpty() && QFileInfo::exists(m_activeModelPath))
    m_watcher.addPath(m_activeModelPath);
}

void ModelManager::revalidateActiveModel() {
  if (m_activeModelPath.isEmpty())
    return;
  if (busy()) {
    clearActiveModel();
    emit setupRequired();
    return;
  }
  const QString path = m_activeModelPath;
  const ModelCatalogEntry *item = entryForPath(path);
  m_restoringActiveModel = true;
  clearActiveModel();
  if (!isStructurallyValidModel(path) || (item && QFileInfo(path).size() != item->size)) {
    m_restoringActiveModel = false;
    setFailure(i18n("The active speech model is no longer valid."));
    emit setupRequired();
    return;
  }
  if (item)
    verifyFile(*item, path, false);
  else
    validateLocalModel(path, true);
}

ModelManager::HashResult ModelManager::hashFile(const QString &path,
                                                const CancellationFlag &cancelled) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return std::nullopt;
  QCryptographicHash hash(QCryptographicHash::Sha256);
  while (!file.atEnd()) {
    if (cancelled->load())
      return std::nullopt;
    const QByteArray data = file.read(1024 * 1024);
    if (data.isEmpty() && file.error() != QFileDevice::NoError)
      return std::nullopt;
    hash.addData(data);
  }
  return hash.result();
}
