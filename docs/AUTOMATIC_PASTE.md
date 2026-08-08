<!--
SPDX-FileCopyrightText: 2026 Sri Rang
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Automatic paste setup and troubleshooting

Kastword always places completed transcription on the clipboard. Automatic paste is optional,
disabled by default, and uses a separately installed helper to send your configured paste shortcut.
If no helper is available, copy and transcription still work for manual pasting.

## Install the session helper

On CachyOS or Arch Linux:

```sh
# Plasma Wayland
sudo pacman -S --needed ydotool

# Plasma X11
sudo pacman -S --needed xdotool
```

X11 paste uses `xdotool`. Kastword checks that the focused window is unchanged immediately before
sending keys, although focus can still change afterward.

Wayland does not expose an equivalent global focus check. Kastword uses `ydotool`, whose
`ydotoold` service creates a virtual keyboard through `/dev/uinput`. Focus can change during the
short delay before keys are sent.

## Enable automatic paste on Plasma Wayland

Enable and start the packaged user service:

```sh
systemctl --user enable --now ydotool.service
systemctl --user is-active ydotool.service
```

The second command must print `active`. Verify that the input device and daemon socket exist and
that the client can connect:

```sh
ls -l /dev/uinput "$XDG_RUNTIME_DIR/.ydotool_socket"
ydotool debug
```

Test actual input delivery by running this command, immediately focusing an editable field, and
waiting one second. `Kastword ydotool test` should appear:

```sh
sleep 1 && ydotool type 'Kastword ydotool test'
```

## Troubleshoot `ydotoold`

If the service is inactive or the socket is missing, inspect its status and current-boot log:

```sh
systemctl --user status ydotool.service
journalctl --user -u ydotool.service -b
```

Errors mentioning `/dev/uinput`, the daemon socket, or permission denied mean the helper is not
usable by the logged-in user. Check that the distribution package is current, restart the user
service, and log out and back in after changing device or group permissions.

Kastword never requests elevated permissions and does not start or configure `ydotoold`. Do not run
Kastword or `ydotool` with `sudo`.

## Security and privacy considerations

- Paste helpers are resolved from the inherited `PATH`; ensure every directory in `PATH` is trusted.
- The target application receives the transcription and may retain it.
- Clipboard managers may save clipboard history independently of Kastword.
- The clear-transcription action clears matching current clipboard selections, not entries already
  retained by a clipboard manager.
- Automatic paste sends keystrokes to whichever field has focus at delivery time. Confirm focus
  before dictating sensitive text, especially on Wayland.

Return to the [Kastword overview](../README.md) or see the [source installation and development
guide](DEVELOPMENT.md).
