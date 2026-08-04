# SPDX-FileCopyrightText: 2026 Sri Rang
# SPDX-License-Identifier: GPL-3.0-or-later

.PHONY: all configure ensure-configured build run install install-smoke uninstall test coverage clean format lint license validate

BUILD_DIR ?= build
BUILD_TYPE ?= Release
PREFIX ?= $(HOME)/.local
CMAKE_ARGS ?=
COVERAGE_DIR ?= $(BUILD_DIR)/coverage
COVERAGE_MIN_LINE ?= 68
COVERAGE_MIN_BRANCH ?= 55
CONFIG_KEY = $(shell { sha256sum Makefile; printf '%s\n' '$(BUILD_TYPE)' '$(PREFIX)' '$(CMAKE_ARGS)'; } \
	| sha256sum | cut -d' ' -f1)
CONFIG_STAMP := $(BUILD_DIR)/.make-config
RUN_LOCKED := mkdir -p "$(BUILD_DIR)" && flock "$(BUILD_DIR)/.build.lock"
CONFIGURE_COMMAND = cmake -S . -B $(BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
	-DCMAKE_INSTALL_PREFIX=$(PREFIX) \
	-DKASTWORD_FETCH_WHISPER=ON \
	-DKASTWORD_FETCH_DEFAULT_MODEL=OFF \
	-DKASTWORD_ENABLE_COVERAGE=OFF \
	-DKASTWORD_ENABLE_SANITIZERS=OFF \
	$(CMAKE_ARGS)

all: build

configure:
	$(RUN_LOCKED) $(CONFIGURE_COMMAND)
	@printf '%s\n' '$(CONFIG_KEY)' > "$(CONFIG_STAMP)"

ensure-configured:
	@if [ ! -f "$(BUILD_DIR)/build.ninja" ] \
		|| [ "$$(cat "$(CONFIG_STAMP)" 2>/dev/null || true)" != "$(CONFIG_KEY)" ]; then \
		$(RUN_LOCKED) $(CONFIGURE_COMMAND); \
		printf '%s\n' '$(CONFIG_KEY)' > "$(CONFIG_STAMP)"; \
	fi

build: ensure-configured
	$(RUN_LOCKED) cmake --build $(BUILD_DIR) --target kastword

run: build
	./$(BUILD_DIR)/kastword --show-window

install: ensure-configured
	$(RUN_LOCKED) cmake --build $(BUILD_DIR) --target kastword pofiles tsfiles
	cmake --install $(BUILD_DIR) --component Kastword
	@if command -v update-desktop-database >/dev/null 2>&1; then \
		update-desktop-database "$(PREFIX)/share/applications"; \
	fi
	@if [ "$(abspath $(PREFIX))" = "$$HOME/.local" ] && command -v kbuildsycoca6 >/dev/null 2>&1; then \
		kbuildsycoca6; \
	fi
	@echo "Kastword is installed. Launch it from the application menu."

install-smoke: ensure-configured
	$(RUN_LOCKED) cmake --build $(BUILD_DIR) --target kastword pofiles tsfiles
	@set -eu; \
	smoke_prefix="$$(mktemp -d)"; \
	installed_files=""; \
	broken_fixture_dir=""; \
	trap 'cmake -E remove_directory "$$smoke_prefix"; \
		test -z "$$installed_files" || cmake -E rm -f "$$installed_files"; \
		test -z "$$broken_fixture_dir" || cmake -E remove_directory "$$broken_fixture_dir"' EXIT; \
	installed_files="$$(mktemp)"; \
	broken_fixture_dir="$$(mktemp -d)"; \
	broken_desktop="$$broken_fixture_dir/broken.desktop"; \
	cmake --install "$(BUILD_DIR)" --prefix "$$smoke_prefix" --component Kastword; \
	test -x "$$smoke_prefix/bin/kastword"; \
	desktop_file="$$smoke_prefix/share/applications/io.github.shape_machine.Kastword.desktop"; \
	./tools/check-desktop-launcher.sh "$$smoke_prefix" "$$desktop_file"; \
	sed 's/^Exec=.*/Exec=missing-kastword/' "$$desktop_file" > "$$broken_desktop"; \
	if ./tools/check-desktop-launcher.sh "$$smoke_prefix" "$$broken_desktop"; then \
		echo "Broken desktop launcher unexpectedly passed smoke testing" >&2; \
		exit 1; \
	fi; \
	appstreamcli validate --no-net \
		"$$smoke_prefix/share/metainfo/io.github.shape_machine.Kastword.metainfo.xml"; \
	test -f "$$smoke_prefix/share/locale/x-test/LC_MESSAGES/kastword.mo"; \
	test ! -e "$$smoke_prefix/share/kastword/models/ggml-base.en.bin"; \
	find "$$smoke_prefix" -type f -print > "$$installed_files"; \
	user_model="$$smoke_prefix/share/kastword/models/ggml-base.en.bin"; \
	cmake -E make_directory "$$(dirname "$$user_model")"; \
	cmake -E touch "$$user_model"; \
	case "$$smoke_prefix" in ""|/) echo "Refusing unsafe uninstall prefix" >&2; exit 1;; esac; \
	$(MAKE) uninstall PREFIX="$$smoke_prefix" BUILD_DIR="$(BUILD_DIR)"; \
	while IFS= read -r installed_file; do test ! -e "$$installed_file"; done < "$$installed_files"; \
	test -f "$$user_model"

uninstall:
	cmake -E rm -f \
		"$(PREFIX)/bin/kastword" \
		"$(PREFIX)/share/applications/io.github.shape_machine.Kastword.desktop" \
		"$(PREFIX)/share/metainfo/io.github.shape_machine.Kastword.metainfo.xml" \
		"$(PREFIX)/share/locale/x-test/LC_MESSAGES/kastword.mo" \
		"$(PREFIX)/share/doc/kastword/README.md" \
		"$(PREFIX)/share/doc/kastword/GPL-3.0-or-later.txt"
	@if command -v update-desktop-database >/dev/null 2>&1; then \
		update-desktop-database "$(PREFIX)/share/applications"; \
	fi
	@if [ "$(abspath $(PREFIX))" = "$$HOME/.local" ] && command -v kbuildsycoca6 >/dev/null 2>&1; then \
		kbuildsycoca6; \
	fi
	@echo "Kastword has been uninstalled."

test: ensure-configured
	$(RUN_LOCKED) cmake --build $(BUILD_DIR)
	$(RUN_LOCKED) ctest --test-dir $(BUILD_DIR) --output-on-failure

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
	$(RUN_LOCKED) cmake --build $(BUILD_DIR) --target kastword_qmllint
