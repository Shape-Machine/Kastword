// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QStringView>

inline bool isNativeDesktopPlatform(QStringView platformName) {
  return platformName == u"xcb" || platformName.startsWith(u"wayland");
}
