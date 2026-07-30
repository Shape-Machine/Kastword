#pragma once

#include "AudioCapture.h"
#include "TextOutput.h"

#include <KConfig>
#include <QObject>
#include <QAction>
#include <QString>

class AppController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString transcript READ transcript NOTIFY transcriptChanged)
    Q_PROPERTY(QString modelPath READ modelPath WRITE setModelPath NOTIFY modelPathChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(bool autoPaste READ autoPaste WRITE setAutoPaste NOTIFY autoPasteChanged)
    Q_PROPERTY(qreal level READ level NOTIFY levelChanged)
    Q_PROPERTY(int overlayX READ overlayX NOTIFY overlayPositionChanged)
    Q_PROPERTY(int overlayY READ overlayY NOTIFY overlayPositionChanged)

public:
    explicit AppController(QObject *parent = nullptr);
    QString state() const { return m_state; }
    QString status() const { return m_status; }
    QString transcript() const { return m_transcript; }
    QString modelPath() const { return m_modelPath; }
    QString language() const { return m_language; }
    bool autoPaste() const { return m_autoPaste; }
    qreal level() const { return m_level; }
    int overlayX() const { return m_overlayX; }
    int overlayY() const { return m_overlayY; }

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
    void overlayPositionChanged();

private:
    void setState(const QString &value);
    void setStatus(const QString &value);
    void saveSettings();
    void transcribe(QByteArray audio);
    void updateOverlayPosition();

    AudioCapture m_audio;
    TextOutput m_output;
    KConfig m_config;
    QAction m_shortcut;
    QString m_state = QStringLiteral("idle");
    QString m_status = QStringLiteral("Ready — press Meta+Shift+D to dictate.");
    QString m_transcript;
    QString m_modelPath;
    QString m_language = QStringLiteral("en");
    bool m_autoPaste = true;
    qreal m_level = 0.0;
    int m_overlayX = 0;
    int m_overlayY = 0;
};
