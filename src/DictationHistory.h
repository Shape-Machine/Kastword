// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QVariantList>
#include <functional>
#include <memory>
#include <optional>

class HistoryKeyProvider {
public:
  virtual ~HistoryKeyProvider() = default;
  virtual std::optional<QByteArray> loadOrCreate(QString *error) = 0;
  virtual bool remove(QString *error) = 0;
};

std::unique_ptr<HistoryKeyProvider> createHistoryKeyProvider();

class DictationHistory final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
  Q_PROPERTY(bool available READ available NOTIFY changed)
  Q_PROPERTY(QString status READ status NOTIFY changed)
  Q_PROPERTY(QString storagePath READ storagePath CONSTANT)
  Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)
  Q_PROPERTY(QVariantList recentEntries READ recentEntries NOTIFY changed)
  Q_PROPERTY(int maximumEntries READ maximumEntries WRITE setMaximumEntries NOTIFY changed)
  Q_PROPERTY(int maximumAgeDays READ maximumAgeDays WRITE setMaximumAgeDays NOTIFY changed)

public:
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
  bool available() const { return m_available; }
  QString status() const { return m_status; }
  QString storagePath() const { return m_storagePath; }
  QVariantList entries() const;
  QVariantList recentEntries() const;
  int maximumEntries() const { return m_maximumEntries; }
  int maximumAgeDays() const { return m_maximumAgeDays; }

  bool enable();
  void disable(bool deleteData);
  bool add(const QString &text);
  Q_INVOKABLE bool removeEntry(const QString &id);
  Q_INVOKABLE bool clear();
  void setMaximumEntries(int value);
  void setMaximumAgeDays(int value);

signals:
  void changed();

private:
  bool load();
  bool save();
  bool saveEntries(const QList<Entry> &entries);
  bool prune();
  bool pruneEntries(QList<Entry> &entries, int maximumEntries, int maximumAgeDays) const;
  void fail(const QString &message);
  static bool commitAtomically(const QString &path, const QByteArray &data, QString *error);
  QVariantMap entryMap(const Entry &entry) const;

  QString m_storagePath;
  std::unique_ptr<HistoryKeyProvider> m_keyProvider;
  Clock m_clock;
  CommitFunction m_commit;
  QByteArray m_key;
  QList<Entry> m_entries;
  bool m_enabled = false;
  bool m_available = true;
  bool m_cryptoAvailable = true;
  QString m_status;
  int m_maximumEntries = 100;
  int m_maximumAgeDays = 30;
};
