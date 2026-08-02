// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>

struct ModelCatalogEntry {
  QString id;
  QString fileName;
  QUrl url;
  QByteArray sha256;
  qint64 size = 0;
  bool englishOnly = false;
  bool recommended = false;
  QString speed;
  QString accuracy;
};

QList<ModelCatalogEntry> whisperModelCatalog();
QString validateModelCatalog(const QList<ModelCatalogEntry> &catalog);
