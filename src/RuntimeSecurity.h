// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtTypes>

bool shouldRefuseElevatedExecution(quint64 realUserId, quint64 effectiveUserId);
