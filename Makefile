# SPDX-FileCopyrightText: 2026 Sri Rang
# SPDX-License-Identifier: GPL-3.0-or-later

.PHONY: all configure build run install test clean format validate

BUILD_DIR ?= build
BUILD_TYPE ?= Release
PREFIX ?= /usr/local

all: build

configure:
	cmake -S . -B $(BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_INSTALL_PREFIX=$(PREFIX)

build: configure
	cmake --build $(BUILD_DIR)

run: build
	./$(BUILD_DIR)/kastword

install: build
	cmake --install $(BUILD_DIR) --component Kastword

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

clean:
	cmake -E remove_directory $(BUILD_DIR)

format:
	clang-format -i src/*.cpp src/*.h

validate: build test
	reuse lint
	appstreamcli validate --no-net data/io.github.shape_machine.Kastword.metainfo.xml
	desktop-file-validate data/io.github.shape_machine.Kastword.desktop
	/usr/lib/qt6/bin/qmllint -I build -I /usr/lib/qt6/qml src/qml/Main.qml
