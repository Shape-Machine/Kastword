// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "AudioCapture.h"
#include "TextOutput.h"

#include <KConfig>
#include <QAction>
#include <QObject>
#include <QPointer>
#include <QString>

class KNotification;

class AppController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString state READ state NOTIFY stateChanged)
  Q_PROPERTY(QString status READ status NOTIFY statusChanged)
  Q_PROPERTY(QString transcript READ transcript NOTIFY transcriptChanged)
  Q_PROPERTY(QString modelPath READ modelPath WRITE setModelPath NOTIFY modelPathChanged)
  Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
  Q_PROPERTY(bool autoPaste READ autoPaste WRITE setAutoPaste NOTIFY autoPasteChanged)
  Q_PROPERTY(qreal level READ level NOTIFY levelChanged)

public:
  explicit AppController(QObject *parent = nullptr);
  QString state() const { return m_state; }
  QString status() const { return m_status; }
  QString transcript() const { return m_transcript; }
  QString modelPath() const { return m_modelPath; }
  QString language() const { return m_language; }
  bool autoPaste() const { return m_autoPaste; }
  qreal level() const { return m_level; }

  void setModelPath(const QString &value);
  void setLanguage(const QString &value);
  void setAutoPaste(bool value);
  QAction *shortcutAction() { return &m_shortcut; }

  Q_INVOKABLE void toggle();

signals:
  void stateChanged();
  void statusChanged();
  void transcriptChanged();
  void modelPathChanged();
  void languageChanged();
  void autoPasteChanged();
  void levelChanged();

private:
  void setState(const QString &value);
  void setStatus(const QString &value);
  void saveSettings();
  void transcribe(QByteArray audio);
  void showStatusNotification(const QString &title, const QString &text, const QString &iconName,
                              bool persistent = false);

  AudioCapture m_audio;
  TextOutput m_output;
  KConfig m_config;
  QAction m_shortcut;
  QString m_state = QStringLiteral("idle");
  QString m_status = QStringLiteral("Ready — press Meta+Z to dictate.");
  QString m_transcript;
  QString m_modelPath;
  QString m_language = QStringLiteral("en");
  bool m_autoPaste = true;
  qreal m_level = 0.0;
  QPointer<KNotification> m_statusNotification;
};
