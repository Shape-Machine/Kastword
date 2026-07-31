// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ModelLocator.h"

#include <QFileInfo>

QString firstReadableModel(const QStringList &candidates) {
  for (const QString &candidate : candidates) {
    const QFileInfo info(candidate);
    if (info.isFile() && info.isReadable())
      return info.canonicalFilePath();
  }
  return {};
}
