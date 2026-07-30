#pragma once

#include <QObject>
#include <QString>

class TextOutput final : public QObject
{
    Q_OBJECT
public:
    explicit TextOutput(QObject *parent = nullptr);
    QString deliver(const QString &text, bool autoPaste);
};

