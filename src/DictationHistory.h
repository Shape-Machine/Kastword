// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <functional>
#include <memory>
#include <optional>

class HistoryKeyProvider {
public:
  using LoadCallback = std::function<void(std::optional<QByteArray>, const QString &)>;
  using RemoveCallback = std::function<void(bool, const QString &)>;
  virtual ~HistoryKeyProvider() = default;
  virtual void loadOrCreate(QObject *context, LoadCallback callback) = 0;
  virtual void remove(QObject *context, RemoveCallback callback) = 0;
};

std::unique_ptr<HistoryKeyProvider> createHistoryKeyProvider();

class DictationHistory final : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(bool deletionPending READ deletionPending NOTIFY changed)
  Q_PROPERTY(bool available READ available NOTIFY changed)
  Q_PROPERTY(bool resetRequired READ resetRequired NOTIFY changed)
  Q_PROPERTY(QString status READ status NOTIFY changed)
  Q_PROPERTY(QString storagePath READ storagePath CONSTANT)
  Q_PROPERTY(QAbstractItemModel *entryModel READ entryModel CONSTANT)
  Q_PROPERTY(QVariantList recentEntries READ recentEntries NOTIFY changed)
  Q_PROPERTY(int maximumEntries READ maximumEntries WRITE setMaximumEntries NOTIFY changed)
  Q_PROPERTY(int maximumAgeDays READ maximumAgeDays WRITE setMaximumAgeDays NOTIFY changed)

public:
  enum Role { EntryIdRole = Qt::UserRole + 1, CreatedAtRole, CreatedTextRole, TextRole };

  struct Entry {
    QString id;
    QDateTime createdAt;
    QString text;
  };
  using Clock = std::function<QDateTime()>;
  using CommitFunction = std::function<bool(const QString &, const QByteArray &, QString *)>;

  explicit DictationHistory(QObject *parent = nullptr);
  DictationHistory(QString storagePath, std::unique_ptr<HistoryKeyProvider> keyProvider,
                   Clock clock = {}, CommitFunction commit = {}, QObject *parent = nullptr);
  ~DictationHistory() override;

  bool enabled() const { return m_enabled; }
  bool busy() const { return m_busy; }
  bool deletionPending() const;
  bool available() const { return m_available; }
  bool resetRequired() const { return m_resetRequired; }
  QString status() const { return m_status; }
  QString storagePath() const { return m_storagePath; }
  QVariantList entries() const;
  QAbstractItemModel *entryModel() { return this; }
  QVariantList recentEntries() const;
  int maximumEntries() const { return m_maximumEntries; }
  int maximumAgeDays() const { return m_maximumAgeDays; }

  int rowCount(const QModelIndex &parent = {}) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  void enable();
  void resumePendingDeletion();
  void disable(bool deleteData);
  bool add(const QString &text);
  Q_INVOKABLE bool removeEntry(const QString &id);
  Q_INVOKABLE bool clear();
  void setMaximumEntries(int value);
  void setMaximumAgeDays(int value);

signals:
  void changed();
  void settingsChanged();

private:
  Q_SLOT void expireEntries();
  bool load();
  bool saveEntries(const QList<Entry> &entries);
  bool pruneEntries(QList<Entry> &entries, int maximumEntries, int maximumAgeDays) const;
  void scheduleExpiry();
  QString deletionMarkerPath() const;
  void removePendingKey();
  void replaceEntries(QList<Entry> entries);
  void fail(const QString &message, bool resetRequired = false);
  static bool commitAtomically(const QString &path, const QByteArray &data, QString *error);
  QVariantMap entryMap(const Entry &entry) const;

  QString m_storagePath;
  std::unique_ptr<HistoryKeyProvider> m_keyProvider;
  Clock m_clock;
  CommitFunction m_commit;
  QTimer m_expiryTimer;
  QByteArray m_key;
  QList<Entry> m_entries;
  bool m_enabled = false;
  bool m_busy = false;
  bool m_available = true;
  bool m_resetRequired = false;
  bool m_cryptoAvailable = true;
  QString m_status;
  int m_maximumEntries = 100;
  int m_maximumAgeDays = 30;
};
