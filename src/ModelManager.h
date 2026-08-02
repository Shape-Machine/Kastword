// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ModelCatalog.h"

#include <QFile>
#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>

class ModelManager final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList models READ models NOTIFY changed)
  Q_PROPERTY(QString activeModelPath READ activeModelPath NOTIFY activeModelPathChanged)
  Q_PROPERTY(bool modelReady READ modelReady NOTIFY activeModelPathChanged)
  Q_PROPERTY(bool activeModelEnglishOnly READ activeModelEnglishOnly NOTIFY activeModelPathChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(QString currentModelId READ currentModelId NOTIFY changed)
  Q_PROPERTY(qreal progress READ progress NOTIFY changed)
  Q_PROPERTY(QString status READ status NOTIFY changed)
  Q_PROPERTY(QString error READ error NOTIFY changed)
  Q_PROPERTY(QString storagePath READ storagePath CONSTANT)
  Q_PROPERTY(bool restoringActiveModel READ restoringActiveModel NOTIFY changed)
  Q_PROPERTY(bool verificationPending READ verificationPending NOTIFY changed)

public:
  struct ModelValidationResult {
    bool valid = false;
    bool englishOnly = false;
  };
  using ModelValidationFunction = std::function<ModelValidationResult(const QString &)>;
  using HashResult = std::optional<QByteArray>;
  using CancellationFlag = std::shared_ptr<std::atomic_bool>;
  using HashFunction = std::function<HashResult(const QString &, const CancellationFlag &)>;
  using FileRemoveFunction = std::function<bool(const QString &)>;

  explicit ModelManager(QObject *parent = nullptr);
  ModelManager(QList<ModelCatalogEntry> catalog, QString storagePath,
               QNetworkAccessManager *network, QObject *parent = nullptr,
               ModelValidationFunction validator = {}, HashFunction hasher = {},
               FileRemoveFunction removeFile = {});

  QVariantList models() const;
  QString activeModelPath() const { return m_activeModelPath; }
  bool modelReady() const { return !m_activeModelPath.isEmpty(); }
  bool activeModelEnglishOnly() const { return m_activeModelEnglishOnly; }
  bool busy() const {
    return m_reply || m_hashWatcher.isRunning() || m_modelValidationWatcher.isRunning();
  }
  QString currentModelId() const { return m_currentModelId; }
  qreal progress() const { return m_progress; }
  QString status() const { return m_status; }
  QString error() const { return m_error; }
  QString storagePath() const { return m_storagePath; }
  bool restoringActiveModel() const { return m_restoringActiveModel; }
  bool verificationPending() const {
    return m_hashWatcher.isRunning() || m_modelValidationWatcher.isRunning();
  }

  static bool isStructurallyValidModel(const QString &path);

  Q_INVOKABLE void download(const QString &id);
  Q_INVOKABLE void cancel();
  Q_INVOKABLE void selectModel(const QString &id);
  Q_INVOKABLE bool selectLocalModel(const QUrl &url);
  Q_INVOKABLE bool removeModel(const QString &id);
  void restoreActiveModel(const QString &path);

signals:
  void changed();
  void activeModelPathChanged();
  void setupRequired();

private:
  const ModelCatalogEntry *entry(const QString &id) const;
  const ModelCatalogEntry *entryForPath(const QString &path) const;
  QString modelPath(const ModelCatalogEntry &entry) const;
  QString partialPath(const ModelCatalogEntry &entry) const;
  void beginRequest(const ModelCatalogEntry &entry);
  void consumeReplyData();
  void finishRequest();
  void verifyFile(const ModelCatalogEntry &entry, const QString &path, bool installAfterVerify);
  void finishVerification();
  void validateLocalModel(const QString &path, bool restoring);
  void finishLocalModelValidation();
  void activatePath(const QString &path, bool englishOnly);
  void clearActiveModel();
  void setFailure(const QString &message);
  void watchActiveModel();
  void revalidateActiveModel();
  static HashResult hashFile(const QString &path, const CancellationFlag &cancelled);

  QList<ModelCatalogEntry> m_catalog;
  QString m_storagePath;
  std::unique_ptr<QNetworkAccessManager> m_ownedNetwork;
  QNetworkAccessManager *m_network = nullptr;
  QPointer<QNetworkReply> m_reply;
  QFile m_partialFile;
  qint64 m_resumeOffset = 0;
  QString m_currentModelId;
  qreal m_progress = 0.0;
  QString m_status;
  QString m_error;
  QString m_activeModelPath;
  bool m_activeModelEnglishOnly = false;
  QString m_verificationPath;
  bool m_installAfterVerify = false;
  QFutureWatcher<HashResult> m_hashWatcher;
  QFutureWatcher<ModelValidationResult> m_modelValidationWatcher;
  QFileSystemWatcher m_watcher;
  ModelValidationFunction m_validator;
  HashFunction m_hasher;
  FileRemoveFunction m_removeFile;
  CancellationFlag m_hashCancelled;
  QString m_pendingLocalModelPath;
  bool m_restoringActiveModel = false;
};
