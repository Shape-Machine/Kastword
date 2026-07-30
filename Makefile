.PHONY: all configure build run install test clean format

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
	cmake --install $(BUILD_DIR)

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

clean:
	cmake -E remove_directory $(BUILD_DIR)

format:
	clang-format -i src/*.cpp src/*.h

