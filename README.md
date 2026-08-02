<!--
SPDX-FileCopyrightText: 2026 Sri Rang
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Kastword

[![Build](https://github.com/Shape-Machine/Kastword/actions/workflows/ci.yml/badge.svg)](https://github.com/Shape-Machine/Kastword/actions/workflows/ci.yml)

**Private, offline dictation built for KDE Plasma.**

> [!WARNING]
> Kastword is early alpha software. This repository currently provides source code only—there
> are no supported binary releases or stable compatibility guarantees yet.

Kastword records speech, transcribes it locally with `whisper.cpp`, and pastes the result into the
application you were using. No dictated audio or text is sent to a cloud service. Launch Kastword
manually and it stays out of the way in the system tray until you press **Meta+Z**.

Kastword is an independent project built with KDE technology; it is not currently an official KDE
project or endorsed by KDE e.V.

## What works today

- KDE global push-to-talk shortcut
- tray-first operation with recording, transcription, and success indicators
- responsive Qt Multimedia recording with microphone downmixing and resampling
- completely local English transcription using a bundled `base.en` model
- clipboard output with optional automatic paste
- X11 paste through `xdotool`
- Plasma Wayland paste through `ydotool`
- no saved recordings, transcription history, telemetry, or runtime network client

## Current status

| Environment | Status |
| --- | --- |
| KDE Plasma Wayland | Manually exercised during development |
| KWrite | Dictation and automatic paste exercised |
| Konsole | Dictation and automatic paste exercised |
| Plasma X11 | Implemented but needs broader testing |
| Other distributions and desktops | Community testing needed |

## Install dependencies on CachyOS or Arch Linux

```sh
sudo pacman -S --needed \
  base-devel cmake ninja git \
  qt6-base qt6-declarative qt6-multimedia \
  extra-cmake-modules kirigami \
  kconfig kcoreaddons kdbusaddons kglobalaccel ki18n \
  knotifications kstatusnotifieritem
```

Install the optional paste helper for your session:

```sh
# Plasma Wayland
sudo pacman -S --needed ydotool

# Plasma X11
sudo pacman -S --needed xdotool
```

### Enable automatic paste on Plasma Wayland

Installing `ydotool` provides both the command-line client and the `ydotoold` user service. Enable
and start the service for the current user:

```sh
systemctl --user enable --now ydotool.service
systemctl --user is-active ydotool.service
```

The second command must print `active`. The daemon creates a virtual keyboard through
`/dev/uinput` and listens on a socket in the user's runtime directory. Verify both are present and
that the client can connect:

```sh
ls -l /dev/uinput "$XDG_RUNTIME_DIR/.ydotool_socket"
ydotool debug
```

Finally, test actual input delivery. Run the following command, immediately focus an editable text
field, and wait one second; `Kastword ydotool test` should appear there:

```sh
sleep 1 && ydotool type 'Kastword ydotool test'
```

If the service is inactive or the socket is missing, inspect its log:

```sh
systemctl --user status ydotool.service
journalctl --user -u ydotool.service -b
```

Errors mentioning `/dev/uinput`, the daemon socket, or permission denied mean the helper is not
usable by the logged-in user. Check that the CachyOS/Arch package is current, restart the user
service, and log out and back in after changing device or group permissions. Avoid running Kastword
or `ydotool` with `sudo`.

Kastword never requests elevated permissions and does not start or configure `ydotoold` itself. If
the helper is unavailable, transcription is still copied to the clipboard for manual pasting.

## Build and run

The first default build downloads two immutable, pinned dependencies:

- `whisper.cpp` source at commit `a91dd3be72f70dd1b3cb6e252f35fa17b93f596c`
- the checksum-verified English `base.en` model, approximately 142 MiB

```sh
make
make run
```

The model is automatically selected from `build/models/ggml-base.en.bin`. Later builds reuse the
verified file. The build needs network access for missing dependencies; the running application
does not.

To install for the current user:

```sh
make install
```

This installs the application, desktop launcher, and default model below `~/.local`, then refreshes
Plasma's application database. Launch Kastword from the application menu; `make run` is not needed.
Set `PREFIX` explicitly to install somewhere else.

To uninstall:

```sh
make uninstall
```

The application ID is `io.github.shape_machine.Kastword`; the underscore follows D-Bus guidance
for the hyphen in the `Shape-Machine` organization name.

### Build without downloads

Distribution packagers and UI contributors can provide system dependencies or skip the model:

```sh
cmake -S . -B build -G Ninja \
  -DKASTWORD_FETCH_WHISPER=OFF \
  -DKASTWORD_FETCH_DEFAULT_MODEL=OFF
```

Disabling the Whisper fetch requires a compatible system `whisper` CMake package. Disabling only
the model download is useful for CI and UI development.

## Using Kastword

1. Start Kastword. It launches hidden in the system tray.
2. Focus the text field or terminal where the result should go.
3. Press **Meta+Z** and speak.
4. Press **Meta+Z** again.
5. Kastword transcribes locally, updates the clipboard, and pastes when a helper is available.

Click the tray icon to open or hide the settings window. The tray menu can start or stop
dictation and quit the application.

## Privacy and security model

- Microphone samples live in process memory only until transcription completes.
- Recordings stop at the configured duration limit (five minutes by default) or a 256 MiB raw
  audio ceiling, whichever comes first.
- Raw audio and transcription history are not written to disk.
- Transcribed text is placed on the desktop clipboard and primary selection.
- The selected model remains on local storage.
- No telemetry or runtime network client is included.
- The build downloads pinned dependencies over HTTPS and verifies the model checksum.
- Automatic Wayland paste relies on the separately installed `ydotool`/`ydotoold` service.
- Custom model files are trusted input parsed inside Kastword; use models from sources you trust.
- Paste helpers are resolved from the inherited `PATH`; ensure every directory in `PATH` is
  trusted. Kastword refuses to run with elevated privileges.

Clipboard managers, target applications, desktop services, crash dumps, and the operating system
may retain data independently of Kastword. Review their settings when dictating sensitive text.
The clear-transcription action clears Kastword's retained text and any matching current clipboard
or primary selection; it does not erase entries already retained by clipboard-manager history.
Automatic paste is disabled by default. X11 focus is checked again before keys are sent; Wayland
does not expose an equivalent global focus check, so focus can change during the short delay.

## Known limitations

- The shortcut is currently fixed to Meta+Z in Kastword's UI, though KDE can manage it.
- English `base.en` is the only model fetched automatically.
- Paste reliability depends on the session, helper, and target application.
- `ydotool` requires privileged input-device access configured outside Kastword.
- There are no supported binary packages or release builds yet.

## Architecture

```text
Global shortcut / tray
         │
         ▼
 AppController state machine
    ├── AudioCapture ─── Qt Multimedia
    ├── WhisperEngine ── worker thread / whisper.cpp
    └── TextOutput ───── clipboard + optional paste helper
```

Inference runs away from the UI thread. The on-screen indicator is non-focusable so showing it
does not change the application receiving pasted text.

## Development and validation

```sh
make test
make coverage BUILD_DIR=build-coverage CMAKE_ARGS=-DKASTWORD_FETCH_DEFAULT_MODEL=OFF
make lint
make install-smoke
make format
make validate
```

`make test` builds and runs the deterministic test suites. `make coverage` enables instrumentation,
enforces the repository's line and branch thresholds, and writes a browsable report to
`build-coverage/coverage/index.html`, a text summary, and Cobertura XML. It uses an installed
`gcovr`, or runs it through `uvx` when available. The default gates require at least 68% line and
55% branch coverage and can be raised explicitly with `COVERAGE_MIN_LINE` and
`COVERAGE_MIN_BRANCH`. `make lint` checks C++ formatting without changing files, while `make format`
applies it.
`make install-smoke` installs into a temporary prefix and checks the installed executable and
metadata. `make validate` runs the build, tests, formatting check, REUSE license validation, desktop
metadata validation, and QML linting. These targets require `gcovr`, `reuse`, `appstreamcli`,
`desktop-file-validate`, `clang-format`, and Qt's `qmllint`. CI invokes the same Make targets without
downloading the model and also runs the tests under AddressSanitizer and UndefinedBehaviorSanitizer.
Every CI run publishes the exact coverage summary on its job page and uploads the complete HTML,
text, and XML reports as the `coverage-report` artifact.

Kate users can open the repository directory and enable the Project, Build, and LSP Client
plugins. `.kateproject` provides Build, Run, Test, and Clean targets, while CMake generates
`build/compile_commands.json` for clangd.

## Roadmap

- broader Plasma Wayland/X11 and application compatibility testing
- configurable shortcut and microphone selection
- improved model management and multilingual workflows
- reproducible distribution packages and signed binary releases

Roadmap items are intentions, not promised dates.

## Maintainer TODO before making the repository public

The following tasks require decisions, accounts, credentials, or creative assets from the owner:

- [ ] Confirm that “Kastword” is acceptable from a naming and trademark perspective.
- [ ] Capture a screenshot and short demo showing dictation into KWrite and Konsole.
- [ ] Create a project icon and GitHub social-preview image with confirmed licensing.
- [ ] Confirm the tested Plasma, Qt, KDE Frameworks, CachyOS, and hardware versions.
- [ ] Decide whether to adopt a Code of Conduct and provide a private enforcement contact.
- [ ] Enable GitHub private vulnerability reporting.
- [ ] Configure repository description, topics, Issues, labels, and optional Discussions.
- [ ] Enable secret scanning, push protection, and appropriate GitHub Actions permissions.
- [ ] Protect `main` and require the CI workflow before merging once collaboration begins.
- [ ] Decide whether the initial public state stays untagged or receives an explicitly unsupported
      `v0.1.0-alpha` source tag.
- [ ] Plan binary packaging, signing, update delivery, and release support separately; none of that
      is implied by publishing this source repository.

## License

Kastword is licensed under `GPL-3.0-or-later`. See `LICENSES/GPL-3.0-or-later.txt`.
AppStream metadata is provided under `CC0-1.0`. Third-party components retain their own licenses.
