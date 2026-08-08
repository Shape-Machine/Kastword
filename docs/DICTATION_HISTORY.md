<!--
SPDX-FileCopyrightText: 2026 Sri Rang
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Encrypted dictation history

Kastword can keep completed transcriptions available across restarts. History is disabled by
default: no history file or encryption key is created until you explicitly enable it from the
History view.

## Storage and encryption

Enabled history is stored at `~/.local/share/kastword/history.enc` by default. Kastword encrypts and
authenticates the complete history before writing it. A separate random encryption key is stored as
a binary secret in KDE Wallet; it is never written beside the history file. The history directory
and file are restricted to the current user, and updates use atomic replacement.

If KDE Wallet is unavailable, remains locked, or rejects access, Kastword does not load or save
history and never falls back to plaintext. A damaged file or incorrect key also fails closed. You
can restore wallet access and retry, or disable history and request deletion of the unreadable file
and wallet key.

Only successful, non-empty transcription text and its timestamp are retained. Recorded audio,
model data, microphone information, target applications, and usage analytics are never included.

## Retention and deletion

The History view lets you set both a maximum age and maximum entry count. The defaults are 30 days
and 100 entries. Both limits are applied after each insertion and whenever history is loaded;
age-based expiry also continues automatically while Kastword remains running.

You can delete one entry or clear all entries. When disabling history, choose whether to keep the
encrypted data for later or delete both the file and its KDE Wallet key. Deletion prevents Kastword
from reading those records, but cannot guarantee forensic erasure from solid-state storage,
filesystem snapshots, swap, crash dumps, or backups.

## Security boundaries

Encryption at rest protects the history file when the wallet key is unavailable. It does not
protect text from software controlling your unlocked desktop session. Copying or automatically
pasting text can expose it to clipboard managers, target applications, desktop services, the
operating system, and their own history or backup mechanisms. Review those systems before using
dictation for sensitive material.

Return to the [project overview](../README.md) or read the [automatic paste security
guide](AUTOMATIC_PASTE.md).
