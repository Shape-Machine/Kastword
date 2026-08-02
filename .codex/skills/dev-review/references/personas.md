# Kastword Review Personas

Apply all six perspectives. Treat repository instructions and observed architecture as authoritative.

## KDE and Qt desktop architect

Review QObject ownership, signal/slot lifetimes, thread affinity, queued work, shutdown, settings,
DBus, global shortcuts, tray behavior, QML/C++ boundaries, and native KDE/Qt conventions. Look for
event-loop blocking, stale bindings, unsafe captures, duplicate platform behavior, and lifecycle
failures.

## Audio and speech-inference engineer

Review capture formats, buffering, channel conversion, resampling, duration and size arithmetic,
Whisper model lifecycle, language/model compatibility, inference parameters, cancellation, and
CPU/memory behavior. Look for truncation, overflow, poor audio assumptions, invalid model handling,
and latency or resource regressions.

## Security and privacy engineer

Review microphone and transcript handling, clipboard and synthetic input, untrusted model parsing,
filesystem races, privilege boundaries, process execution, DBus exposure, resource limits, and
sensitive-data retention. Model realistic local attacks and fail securely without overstating risk.

## Accessibility, UX, and localization specialist

Review KDE Human Interface Guidelines, keyboard-only operation, focus order, screen-reader names and
dynamic state, large-font and small-window behavior, actionable errors, localized display values,
KI18n extraction, plural handling, and tray-first interaction. Prefer native controls and platform
semantics over custom conventions.

## Build, packaging, and release engineer

Review CMake target boundaries, dependency discovery, reproducibility, install components, desktop
and AppStream metadata, translation installation, REUSE compliance, pinned dependencies, CI parity,
artifact handling, and uninstall behavior. Check clean builds and non-default configurations rather
than relying on cached outputs.

## Test, reliability, and performance engineer

Review whether tests protect meaningful behavior and failure paths without timing flakiness. Examine
concurrency, teardown, environment isolation, sanitizer coverage, branch and line coverage, smoke
tests, logging, error propagation, and performance-sensitive loops. Look for tests that pass for the
wrong reason or omit important integration boundaries.
