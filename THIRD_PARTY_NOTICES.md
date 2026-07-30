<!--
SPDX-FileCopyrightText: 2026 Sri Rang
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Third-party components

Kastword uses the following components without changing their respective licenses.

| Component | Use | License | Source |
| --- | --- | --- | --- |
| Qt 6 | Application and multimedia framework | LGPL-3.0-only, GPL-2.0-only, GPL-3.0-only, or commercial terms depending on the distribution | <https://code.qt.io/cgit/qt/> |
| KDE Frameworks 6 and Kirigami | Plasma integration and interface | Library-specific LGPL/GPL terms | <https://invent.kde.org/frameworks> |
| whisper.cpp/ggml | Local speech recognition engine | MIT | <https://github.com/ggml-org/whisper.cpp> |
| Whisper `base.en` model | English speech-recognition weights converted to GGML | MIT | <https://huggingface.co/ggerganov/whisper.cpp> |
| xdotool | Optional X11 paste helper | BSD-3-Clause | <https://github.com/jordansissel/xdotool> |
| ydotool | Optional Wayland paste helper | AGPL-3.0-only | <https://github.com/ReimuNotMoe/ydotool> |

`whisper.cpp` is downloaded as source during the default build and linked into Kastword. The
model is downloaded separately during the build and is not stored in this Git repository. The
optional paste helpers are separate programs invoked at runtime and are not distributed as part
of Kastword.

Distribution maintainers remain responsible for retaining the notices and license texts required
by the exact dependency builds they ship.

