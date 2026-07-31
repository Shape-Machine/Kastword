// SPDX-FileCopyrightText: 2026 Sri Rang
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAudioFormat>
#include <QByteArray>

QByteArray convertAudioForWhisper(const QByteArray &native, const QAudioFormat &format);
