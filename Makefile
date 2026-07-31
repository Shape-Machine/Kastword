# SPDX-FileCopyrightText: 2026 Sri Rang
# SPDX-License-Identifier: GPL-3.0-or-later

.PHONY: all configure build run install uninstall test clean format validate

BUILD_DIR ?= build
BUILD_TYPE ?= Release
PREFIX ?= $(HOME)/.local

all: build

configure:
	cmake -S . -B $(BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_INSTALL_PREFIX=$(PREFIX)

build: configure
	cmake --build $(BUILD_DIR)

run: build
	./$(BUILD_DIR)/kastword

install:
	cmake -S . -B $(BUILD_DIR) -G Ninja \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_INSTALL_PREFIX="$(PREFIX)" \
		-DKASTWORD_FETCH_WHISPER=ON \
		-DKASTWORD_FETCH_DEFAULT_MODEL=ON
	cmake --build $(BUILD_DIR)
	cmake --install $(BUILD_DIR) --component Kastword
	@if command -v update-desktop-database >/dev/null 2>&1; then \
		update-desktop-database "$(PREFIX)/share/applications"; \
	fi
	@if [ "$(abspath $(PREFIX))" = "$$HOME/.local" ] && command -v kbuildsycoca6 >/dev/null 2>&1; then \
		kbuildsycoca6; \
	fi
	@echo "Kastword is installed. Launch it from the application menu."

uninstall:
	cmake -E rm -f \
		"$(PREFIX)/bin/kastword" \
		"$(PREFIX)/share/applications/io.github.shape_machine.Kastword.desktop" \
		"$(PREFIX)/share/metainfo/io.github.shape_machine.Kastword.metainfo.xml" \
		"$(PREFIX)/share/kastword/models/ggml-base.en.bin" \
		"$(PREFIX)/share/doc/kastword/README.md" \
		"$(PREFIX)/share/doc/kastword/GPL-3.0-or-later.txt"
	@if command -v update-desktop-database >/dev/null 2>&1; then \
		update-desktop-database "$(PREFIX)/share/applications"; \
	fi
	@if [ "$(abspath $(PREFIX))" = "$$HOME/.local" ] && command -v kbuildsycoca6 >/dev/null 2>&1; then \
		kbuildsycoca6; \
	fi
	@echo "Kastword has been uninstalled."

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

clean:
	cmake -E remove_directory $(BUILD_DIR)

format:
	clang-format -i src/*.cpp src/*.h

validate: build test
	reuse lint
	appstreamcli validate --no-net data/io.github.shape_machine.Kastword.metainfo.xml
	desktop-file-validate $(BUILD_DIR)/io.github.shape_machine.Kastword.desktop
	/usr/lib/qt6/bin/qmllint -I build -I /usr/lib/qt6/qml src/qml/Main.qml
