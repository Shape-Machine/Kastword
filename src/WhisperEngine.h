#pragma once

#include <QByteArray>
#include <QString>

class WhisperEngine
{
public:
    static QString transcribe(const QByteArray &audio, const QString &modelPath,
                              const QString &language, QString *error);
};

