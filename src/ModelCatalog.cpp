// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ModelCatalog.h"

#include <QFileInfo>
#include <QSet>

namespace {
constexpr auto revision = "5359861c739e955e79d9a303bcbc70fb988958b1";

ModelCatalogEntry model(const char *id, qint64 size, const char *sha256, bool englishOnly,
                        bool recommended, const char *speed, const char *accuracy) {
  const QString modelId = QString::fromLatin1(id);
  const QString fileName = QStringLiteral("ggml-%1.bin").arg(modelId);
  return {modelId,
          fileName,
          QUrl(QStringLiteral("https://huggingface.co/ggerganov/whisper.cpp/resolve/%1/%2")
                   .arg(QString::fromLatin1(revision), fileName)),
          QByteArray::fromHex(sha256),
          size,
          englishOnly,
          recommended,
          QString::fromLatin1(speed),
          QString::fromLatin1(accuracy)};
}
} // namespace

QList<ModelCatalogEntry> whisperModelCatalog() {
  return {
      model("tiny.en", 77704715, "921e4cf8686fdd993dcd081a5da5b6c365bfde1162e72b08d75ac75289920b1f",
            true, false, "Fastest", "Basic"),
      model("base.en", 147964211,
            "a03779c86df3323075f5e796cb2ce5029f00ec8869eee3fdfb897afe36c6d002", true, true, "Fast",
            "Good"),
      model("small.en", 487614201,
            "c6138d6d58ecc8322097e0f987c32f1be8bb0a18532a3f88f734d1bbf9c41e5d", true, false,
            "Moderate", "Better"),
      model("medium.en", 1533774781,
            "cc37e93478338ec7700281a7ac30a10128929eb8f427dda2e865faa8f6da4356", true, false, "Slow",
            "High"),
      model("tiny", 77691713, "be07e048e1e599ad46341c8d2a135645097a538221678b7acdd1b1919c6e1b21",
            false, false, "Fastest", "Basic"),
      model("base", 147951465, "60ed5bc3dd14eea856493d334349b405782ddcaf0028d4b5df4088345fba2efe",
            false, true, "Fast", "Good"),
      model("small", 487601967, "1be3a9b2063867b937e64e2ec7483364a79917e157fa98c5d94b5c1fffea987b",
            false, true, "Moderate", "Better"),
      model("medium", 1533763059,
            "6c14d5adee5f86394037b4e4e8b59f1673b6cee10e3cf0b11bbdbee79c156208", false, false,
            "Slow", "High"),
      model("large-v1", 3094623691,
            "7d99f41a10525d0206bddadd86760181fa920438b6b33237e3118ff6c83bb53d", false, false,
            "Slowest", "Highest"),
      model("large-v2", 3094623691,
            "9a423fe4d40c82774b6af34115b8b935f34152246eb19e80e376071d3f999487", false, false,
            "Slowest", "Highest"),
      model("large-v3", 3095033483,
            "64d182b440b98d5203c4f9bd541544d84c605196c4f7b845dfa11fb23594d1e2", false, false,
            "Slowest", "Highest"),
      model("large-v3-turbo", 1624555275,
            "1fc70f774d38eb169993ac391eea357ef47c88757ef72ee5943879b7e8e2bc69", false, true,
            "Moderate", "Highest"),
  };
}

QString validateModelCatalog(const QList<ModelCatalogEntry> &catalog) {
  const QSet<QString> speeds = {QStringLiteral("Fastest"), QStringLiteral("Fast"),
                                QStringLiteral("Moderate"), QStringLiteral("Slow"),
                                QStringLiteral("Slowest")};
  const QSet<QString> accuracies = {QStringLiteral("Basic"), QStringLiteral("Good"),
                                    QStringLiteral("Better"), QStringLiteral("High"),
                                    QStringLiteral("Highest")};
  QSet<QString> ids;
  QSet<QString> files;
  for (const ModelCatalogEntry &entry : catalog) {
    if (entry.id.isEmpty() || ids.contains(entry.id))
      return QStringLiteral("Model IDs must be non-empty and unique.");
    if (entry.fileName != QStringLiteral("ggml-%1.bin").arg(entry.id) ||
        QFileInfo(entry.fileName).fileName() != entry.fileName || files.contains(entry.fileName))
      return QStringLiteral("Model filenames must be safe and unique.");
    if (entry.url.scheme() != QStringLiteral("https") || entry.url.fileName() != entry.fileName)
      return QStringLiteral("Model URLs must be HTTPS and match their filenames.");
    if (entry.sha256.size() != 32 || entry.size <= 4 || entry.size > 4LL * 1024 * 1024 * 1024)
      return QStringLiteral("Model checksum or size metadata is invalid.");
    if (!speeds.contains(entry.speed) || !accuracies.contains(entry.accuracy))
      return QStringLiteral("Model display metadata is invalid.");
    ids.insert(entry.id);
    files.insert(entry.fileName);
  }
  return {};
}
