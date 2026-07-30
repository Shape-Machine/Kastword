<!--
SPDX-FileCopyrightText: 2026 Sri Rang
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Kastword

**Private, offline dictation built for KDE Plasma.**

> [!WARNING]
> Kastword is early alpha software. This repository currently provides source code only—there
> are no supported binary releases or stable compatibility guarantees yet.

Kastword records speech, transcribes it locally with `whisper.cpp`, and pastes the result into the
application you were using. No dictated audio or text is sent to a cloud service. Launch Kastword
manually and it stays out of the way in the system tray until you press **Meta+Shift+D**.

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

`ydotool` requires a separately configured `ydotoold` service with access to `/dev/uinput`.
Kastword never requests elevated permissions. Without a working helper, transcription is still
copied to the clipboard for manual pasting.

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
make install PREFIX="$HOME/.local"
```

Remove the installed files with:

```sh
rm "$HOME/.local/bin/kastword" \
   "$HOME/.local/share/applications/io.github.shape_machine.Kastword.desktop" \
   "$HOME/.local/share/metainfo/io.github.shape_machine.Kastword.metainfo.xml" \
   "$HOME/.local/share/kastword/models/ggml-base.en.bin" \
   "$HOME/.local/share/doc/kastword/README.md" \
   "$HOME/.local/share/doc/kastword/THIRD_PARTY_NOTICES.md" \
   "$HOME/.local/share/doc/kastword/GPL-3.0-or-later.txt"
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
3. Press **Meta+Shift+D** and speak.
4. Press **Meta+Shift+D** again.
5. Kastword transcribes locally, updates the clipboard, and pastes when a helper is available.

Click the tray icon to open or hide the settings window. The tray menu can start or stop
dictation and quit the application.

## Privacy and security model

- Microphone samples live in process memory only until transcription completes.
- Raw audio and transcription history are not written to disk.
- Transcribed text is placed on the desktop clipboard and primary selection.
- The selected model remains on local storage.
- No telemetry or runtime network client is included.
- The build downloads pinned dependencies over HTTPS and verifies the model checksum.
- Automatic Wayland paste relies on the separately installed `ydotool`/`ydotoold` service.

Clipboard managers, target applications, desktop services, crash dumps, and the operating system
may retain data independently of Kastword. Review their settings when dictating sensitive text.

## Known limitations

- The shortcut is currently fixed to Meta+Shift+D in Kastword's UI, though KDE can manage it.
- The model is loaded for every transcription, increasing latency.
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
make format
make validate
```

`make validate` additionally requires `reuse`, `appstreamcli`, `desktop-file-validate`, clang-format,
and Qt's `qmllint`. CI builds without downloading the model and runs the same core checks.

Kate users can open the repository directory and enable the Project, Build, and LSP Client
plugins. `.kateproject` provides Build, Run, Test, and Clean targets, while CMake generates
`build/compile_commands.json` for clangd.

See [CONTRIBUTING.md](CONTRIBUTING.md), [SECURITY.md](SECURITY.md), and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for project policies and dependency attribution.

## Roadmap

- broader Plasma Wayland/X11 and application compatibility testing
- lower-latency persistent model loading
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
- [ ] Enable GitHub private vulnerability reporting so `SECURITY.md` has a working channel.
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
