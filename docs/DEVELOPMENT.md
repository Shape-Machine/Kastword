<!--
SPDX-FileCopyrightText: 2026 Sri Rang
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Source installation and development

Kastword is early alpha software and currently has no supported binary releases. This guide covers
source installation, packaging, architecture, and contributor tooling. See the [project
overview](../README.md) for product capabilities, privacy, and usage.

## Dependencies

On CachyOS or Arch Linux:

```sh
sudo pacman -S --needed \
  base-devel cmake ninja git \
  qt6-base qt6-declarative qt6-multimedia \
  extra-cmake-modules kirigami kdeclarative \
  kconfig kcoreaddons kdbusaddons kglobalaccel ki18n \
  knotifications kstatusnotifieritem
```

Automatic paste requires a separate session helper. See [Automatic paste setup and
troubleshooting](AUTOMATIC_PASTE.md).

## Build and run

The first default build downloads one immutable, pinned dependency:

- `whisper.cpp` source at commit `a91dd3be72f70dd1b3cb6e252f35fa17b93f596c`

```sh
make
make run
```

`make run` shows the application window immediately for local development. A normal installed
launch starts Kastword in the system tray. No speech model is downloaded or packaged during a
normal build. The build needs network access only when the pinned Whisper.cpp source is not already
available.

## Install and uninstall

Install for the current user:

```sh
make install
```

This installs below `~/.local` and refreshes Plasma's application database. Set `PREFIX` explicitly
to install elsewhere. Launch Kastword from the application menu after installation.

Uninstall application-owned files:

```sh
make uninstall
```

Uninstalling preserves downloaded models and user settings.

The application ID is `io.github.shape_machine.Kastword`; the underscore follows D-Bus guidance
for the hyphen in the `Shape-Machine` organization name.

## Distribution builds

Packagers can provide Whisper.cpp without a build-time download:

```sh
cmake -S . -B build -G Ninja \
  -DKASTWORD_FETCH_WHISPER=OFF
```

This requires a compatible system `whisper` CMake package. Kastword packages must not include a
speech model; users choose models after installation. The legacy
`KASTWORD_FETCH_DEFAULT_MODEL=ON` option remains available only for development compatibility.

## Architecture

```text
Global shortcut / tray
         │
         ▼
 AppController state machine
    ├── AudioCapture ─── Qt Multimedia
    ├── WhisperEngine ── worker thread / whisper.cpp
    ├── ModelManager ─── explicit verified downloads + per-user storage
    └── TextOutput ───── clipboard + optional paste helper
```

Inference runs away from the UI thread. Recording and transcription status is reported through the
application window, system tray, and desktop notifications without opening the main window during
a normal dictation.

## Development and validation

```sh
make test
make coverage BUILD_DIR=build-coverage CMAKE_ARGS=-DKASTWORD_FETCH_DEFAULT_MODEL=OFF
make screenshots
make lint
make install-smoke
make format
make validate
```

`make test` builds and runs the deterministic test suites. `make coverage` enables instrumentation,
enforces the repository's line and branch thresholds, and writes HTML, text, and Cobertura XML
reports under `build-coverage/coverage/`. The default gates require at least 68% line and 55% branch
coverage and can be raised with `COVERAGE_MIN_LINE` and `COVERAGE_MIN_BRANCH`. Coverage uses an
installed `gcovr`, or runs it through `uvx` when available.

`make screenshots` renders all four top-level views in a 760×520 logical window with deterministic
fake data and safely replaces the permanent PNG assets in `screenshots/`. It requires an active
graphical session so output inherits the current Plasma theme, icons, fonts, and display scale. It
does not access the microphone or network and preserves the previous complete set on failure.

`make lint` checks C++ formatting without changing files; `make format` applies it.
`make install-smoke` installs into a temporary prefix, launches through the installed desktop entry,
validates metadata, and verifies complete uninstall behavior. `make validate` runs the build, tests,
formatting check, REUSE validation, desktop metadata validation, and QML linting. Coverage and
license checks use installed `gcovr` and `reuse`, or temporary tools through `uvx`. Remaining checks
require `appstreamcli`, `desktop-file-validate`, and `clang-format`.

CI uses the same Make targets without downloading a model, installs packages from a dated Arch
Linux Archive snapshot, and runs tests under AddressSanitizer and UndefinedBehaviorSanitizer. Each
run publishes its coverage summary and uploads the HTML, text, and XML reports as the
`coverage-report` artifact.

## Kate

Open the repository directory and enable the Project, Build, and LSP Client plugins.
`.kateproject` provides Build, Run, Test, and Clean targets, while CMake generates
`build/compile_commands.json` for clangd.

## Roadmap

- broader Plasma Wayland/X11 and application compatibility testing
- broader audio-device and hot-plug compatibility testing
- reproducible distribution packages and signed binary releases

Roadmap items are intentions, not promised dates.
