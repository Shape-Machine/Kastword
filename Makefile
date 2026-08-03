# SPDX-FileCopyrightText: 2026 Sri Rang
# SPDX-License-Identifier: GPL-3.0-or-later

.PHONY: all configure build run install install-smoke uninstall test coverage clean format lint license validate

BUILD_DIR ?= build
BUILD_TYPE ?= Release
PREFIX ?= $(HOME)/.local
CMAKE_ARGS ?=
COVERAGE_DIR ?= $(BUILD_DIR)/coverage
COVERAGE_MIN_LINE ?= 68
COVERAGE_MIN_BRANCH ?= 55

all: build

configure:
	cmake -S . -B $(BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_INSTALL_PREFIX=$(PREFIX) $(CMAKE_ARGS)

build: configure
	cmake --build $(BUILD_DIR)

run: build
	./$(BUILD_DIR)/kastword --show-window

install:
	cmake -S . -B $(BUILD_DIR) -G Ninja \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_INSTALL_PREFIX="$(PREFIX)" \
		-DKASTWORD_FETCH_WHISPER=ON \
		-DKASTWORD_FETCH_DEFAULT_MODEL=OFF \
		$(CMAKE_ARGS)
	cmake --build $(BUILD_DIR)
	cmake --install $(BUILD_DIR) --component Kastword
	@if command -v update-desktop-database >/dev/null 2>&1; then \
		update-desktop-database "$(PREFIX)/share/applications"; \
	fi
	@if [ "$(abspath $(PREFIX))" = "$$HOME/.local" ] && command -v kbuildsycoca6 >/dev/null 2>&1; then \
		kbuildsycoca6; \
	fi
	@echo "Kastword is installed. Launch it from the application menu."

install-smoke: build
	@smoke_prefix="$$(mktemp -d)"; \
	installed_files="$$(mktemp)"; \
	trap 'cmake -E remove_directory "$$smoke_prefix"; cmake -E rm -f "$$installed_files"' EXIT; \
	cmake --install "$(BUILD_DIR)" --prefix "$$smoke_prefix" --component Kastword; \
	test -x "$$smoke_prefix/bin/kastword"; \
	desktop_file="$$smoke_prefix/share/applications/io.github.shape_machine.Kastword.desktop"; \
	desktop-file-validate "$$desktop_file"; \
	test "$$(sed -n 's/^Exec=//p' "$$desktop_file")" = "kastword"; \
	resolved_binary="$$(PATH="$$smoke_prefix/bin:$$PATH" command -v kastword)"; \
	test "$$resolved_binary" = "$$smoke_prefix/bin/kastword"; \
	QT_QPA_PLATFORM=offscreen "$$resolved_binary" --smoke-test; \
	appstreamcli validate --no-net \
		"$$smoke_prefix/share/metainfo/io.github.shape_machine.Kastword.metainfo.xml"; \
	test -f "$$smoke_prefix/share/locale/x-test/LC_MESSAGES/kastword.mo"; \
	test ! -e "$$smoke_prefix/share/kastword/models/ggml-base.en.bin"; \
	find "$$smoke_prefix" -type f -print > "$$installed_files"; \
	$(MAKE) uninstall PREFIX="$$smoke_prefix" BUILD_DIR="$(BUILD_DIR)"; \
	while IFS= read -r installed_file; do test ! -e "$$installed_file"; done < "$$installed_files"

uninstall:
	cmake -E rm -f \
		"$(PREFIX)/bin/kastword" \
		"$(PREFIX)/share/applications/io.github.shape_machine.Kastword.desktop" \
		"$(PREFIX)/share/metainfo/io.github.shape_machine.Kastword.metainfo.xml" \
		"$(PREFIX)/share/locale/x-test/LC_MESSAGES/kastword.mo" \
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

coverage: override CMAKE_ARGS += -DKASTWORD_ENABLE_COVERAGE=ON
coverage: test
	COVERAGE_MIN_LINE=$(COVERAGE_MIN_LINE) COVERAGE_MIN_BRANCH=$(COVERAGE_MIN_BRANCH) \
		./tools/coverage.sh "$(BUILD_DIR)" "$(COVERAGE_DIR)"

clean:
	cmake -E remove_directory $(BUILD_DIR)

format:
	clang-format -i src/*.cpp src/*.h tests/*.cpp

lint:
	clang-format --dry-run --Werror src/*.cpp src/*.h tests/*.cpp

license:
	./tools/reuse-lint.sh

validate: license
	$(MAKE) test lint
	appstreamcli validate --no-net data/io.github.shape_machine.Kastword.metainfo.xml
	desktop-file-validate $(BUILD_DIR)/io.github.shape_machine.Kastword.desktop
	cmake --build $(BUILD_DIR) --target kastword_qmllint
