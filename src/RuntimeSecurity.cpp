// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RuntimeSecurity.h"

bool shouldRefuseElevatedExecution(quint64 realUserId, quint64 effectiveUserId) {
  return effectiveUserId == 0 || effectiveUserId != realUserId;
}
