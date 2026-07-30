# Kastword

Kastword is a native KDE Plasma dictation app. Audio is recorded and transcribed locally with
`whisper.cpp`; the application makes no network requests at runtime. Press **Meta+Shift+D** to start,
press it again to transcribe, and Kastword copies the result to the clipboard and optionally
pastes it into the previously focused application.

## MVP features

- KDE global shortcut and system tray integration
- Qt Multimedia microphone capture
- fully local Whisper transcription
- clipboard output
- automatic paste with `xdotool` on X11 or `ydotool` on Wayland
- local settings and no recording history

## Dependencies

- CMake 3.24+, Ninja, a C++20 compiler and Git
- Qt 6: Core, Gui, Quick, Quick Controls 2, Multimedia and Concurrent
- KDE Frameworks 6: Config, CoreAddons, GlobalAccel, I18n, Notifications and StatusNotifierItem
- Kirigami 6
- optional runtime paste helper: `xdotool` (X11) or `ydotool` plus a configured `ydotoold`

The first build downloads the pinned `whisper.cpp` v1.8.1 source and the checksum-verified
`base.en` model (about 142 MiB). Builds therefore require network access, but the resulting
program does not. Packagers may configure with `-DKASTWORD_FETCH_WHISPER=OFF` to provide a
system `whisper` CMake package and/or `-DKASTWORD_FETCH_DEFAULT_MODEL=OFF` to package models
separately.

## Build and run

```sh
make
make run
```

Use `make BUILD_TYPE=Debug` for a debug build and `make install PREFIX=$HOME/.local` for a
per-user installation. The default build directory is `build`.

### Kate

Open the repository directory in Kate and enable the Project plugin. The included `.kateproject`
provides Build, Run, Test, and Clean targets. CMake also generates `build/compile_commands.json`
for clangd code navigation.

## Model setup

The English `base.en` model is downloaded during the build, installed with Kastword, and selected
automatically. You may select another compatible ggml `.bin` model in the UI. Use a multilingual
model and select `auto` for language detection. The running application never downloads models.

## Automatic paste and Wayland

Kastword always puts successful transcription on the clipboard. On X11 it can invoke `xdotool`.
On Wayland it can invoke `ydotool`, which usually requires the separately configured `ydotoold`
service and access to `/dev/uinput`. If the matching helper is unavailable, Kastword reports that
the text was copied and you can press Ctrl+V yourself. It never requests elevated permissions.

## Privacy

- microphone data is retained in memory only until transcription completes
- raw audio and transcription history are not saved
- models remain at the path selected by the user
- no telemetry or network client is included

## Current limitations

- microphone input is downmixed and resampled to Whisper's 16 kHz mono format
- the global shortcut is fixed to Meta+Shift+D in this MVP
- model loading happens for every transcription, favoring simplicity over latency
- paste reliability depends on the active desktop session and target application

## License

GPL-3.0-or-later.
