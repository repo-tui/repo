# Makefile for Repo project
# Wraps CMake commands for convenience

# Configuration
BUILD_DIR := build
BUILD_TYPE ?= Debug
VCPKG_ROOT ?= $(HOME)/vcpkg

# Detect if vcpkg is installed
VCPKG_EXISTS := $(shell test -f $(VCPKG_ROOT)/vcpkg && echo 1 || echo 0)
VCPKG_TOOLCHAIN := $(VCPKG_ROOT)/scripts/buildsystems/vcpkg.cmake

# CMake flags
CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
               -DREPO_BUILD_TESTS=ON

ifeq ($(VCPKG_EXISTS),1)
CMAKE_FLAGS += -DCMAKE_TOOLCHAIN_FILE=$(VCPKG_TOOLCHAIN)
endif

# Phony targets
.PHONY: help install-vcpkg install build test clean distclean run-tests format format-check

# Default target
.DEFAULT_GOAL := help

## help: Show this help message
help:
	@echo "Repo Project - Available targets:"
	@echo ""
	@echo "  make install-vcpkg  - Install vcpkg package manager"
	@echo "  make install        - Install dependencies via vcpkg"
	@echo "  make build          - Configure and build the project"
	@echo "  make test           - Run all tests"
	@echo "  make run-tests      - Run tests with verbose output"
	@echo "  make format         - Auto-format code with clang-format"
	@echo "  make format-check   - Check code formatting (CI use)"
	@echo "  make clean          - Clean build artifacts"
	@echo "  make distclean      - Remove build directory entirely"
	@echo ""
	@echo "Configuration:"
	@echo "  BUILD_TYPE=$(BUILD_TYPE)  - Build type (Debug/Release)"
	@echo "  VCPKG_ROOT=$(VCPKG_ROOT)  - vcpkg installation path"
	@echo ""
	@echo "Examples:"
	@echo "  make install build test           - Full setup and test"
	@echo "  make BUILD_TYPE=Release build     - Release build"
	@echo "  make VCPKG_ROOT=/opt/vcpkg build  - Custom vcpkg path"
	@echo "  make format && make build test    - Format, build and test"

## install-vcpkg: Clone and bootstrap vcpkg
install-vcpkg:
	@if [ -d "$(VCPKG_ROOT)" ]; then \
		echo "✓ vcpkg already exists at $(VCPKG_ROOT)"; \
	else \
		echo "Installing vcpkg to $(VCPKG_ROOT)..."; \
		git clone https://github.com/Microsoft/vcpkg.git $(VCPKG_ROOT); \
		cd $(VCPKG_ROOT) && ./bootstrap-vcpkg.sh; \
		echo "✓ vcpkg installed successfully"; \
	fi

## install: Install project dependencies via vcpkg
install: install-vcpkg
	@if [ ! -f "$(VCPKG_ROOT)/vcpkg" ]; then \
		echo "Error: vcpkg not found at $(VCPKG_ROOT)"; \
		echo "Run 'make install-vcpkg' first or set VCPKG_ROOT"; \
		exit 1; \
	fi
	@echo "Installing dependencies from vcpkg.json..."
	@echo "Note: vcpkg will auto-detect your platform and install from manifest"
	$(VCPKG_ROOT)/vcpkg install
	@echo "✓ Dependencies installed"

## build: Configure and build the project
build:
	@if [ ! -f "$(VCPKG_TOOLCHAIN)" ] && [ "$(VCPKG_EXISTS)" = "0" ]; then \
		echo "Warning: vcpkg not found at $(VCPKG_ROOT)"; \
		echo "Run 'make install' first or set VCPKG_ROOT"; \
		echo "Continuing without vcpkg (dependencies must be installed manually)..."; \
	fi
	@echo "Configuring with CMake..."
	cmake -S . -B $(BUILD_DIR) $(CMAKE_FLAGS)
	@echo "Building..."
	cmake --build $(BUILD_DIR)
	@echo "✓ Build complete: $(BUILD_DIR)/repo_tests"

## test: Run all tests with CTest
test: build
	@echo "Running tests..."
	cd $(BUILD_DIR) && ctest --output-on-failure

## run-tests: Run tests directly with Catch2 (verbose)
run-tests: build
	@echo "Running tests (verbose)..."
	$(BUILD_DIR)/repo_tests

## clean: Clean build artifacts (keep CMake cache)
clean:
	@if [ -d "$(BUILD_DIR)" ]; then \
		echo "Cleaning build artifacts..."; \
		cmake --build $(BUILD_DIR) --target clean; \
		echo "✓ Clean complete"; \
	else \
		echo "Nothing to clean (no build directory)"; \
	fi

## distclean: Remove build directory entirely
distclean:
	@if [ -d "$(BUILD_DIR)" ]; then \
		echo "Removing build directory..."; \
		rm -rf $(BUILD_DIR); \
		echo "✓ Build directory removed"; \
	else \
		echo "Nothing to clean (no build directory)"; \
	fi

# Convenience aliases
.PHONY: configure rebuild all

## configure: Run CMake configuration only
configure:
	@echo "Configuring with CMake..."
	cmake -S . -B $(BUILD_DIR) $(CMAKE_FLAGS)

## rebuild: Clean and build from scratch
rebuild: clean build

## all: Install dependencies and build
all: install build

# Development helpers
.PHONY: test-domain test-ops test-integration

## test-domain: Run only domain tests
test-domain: build
	$(BUILD_DIR)/repo_tests "[domain]"

## test-ops: Run only operation tests
test-ops: build
	$(BUILD_DIR)/repo_tests "[ops]"

## test-integration: Run only integration tests
test-integration: build
	$(BUILD_DIR)/repo_tests "[integration]"

# Code formatting
.PHONY: format format-check

# clang-format binary (can be overridden with CLANG_FORMAT env var)
CLANG_FORMAT ?= clang-format

## format: Auto-format code with clang-format
format:
	@echo "Formatting code with $(CLANG_FORMAT)..."
	@find src tests include \( -name '*.cpp' -o -name '*.hpp' \) -exec $(CLANG_FORMAT) -i {} +
	@echo "✓ Code formatted"

## format-check: Check code formatting (fails if unformatted)
format-check:
	@echo "Checking code formatting with $(CLANG_FORMAT)..."
	@$(CLANG_FORMAT) --version
	@find src tests include \( -name '*.cpp' -o -name '*.hpp' \) -exec $(CLANG_FORMAT) --dry-run --Werror {} +
	@echo "✓ Format check passed"

# Info target
.PHONY: info

## info: Show configuration information
info:
	@echo "Project Configuration:"
	@echo "  Build directory: $(BUILD_DIR)"
	@echo "  Build type:      $(BUILD_TYPE)"
	@echo "  vcpkg root:      $(VCPKG_ROOT)"
	@echo "  vcpkg exists:    $(VCPKG_EXISTS)"
	@if [ "$(VCPKG_EXISTS)" = "1" ]; then \
		echo "  vcpkg version:   $$($(VCPKG_ROOT)/vcpkg --version | head -1)"; \
	fi
	@echo ""
	@if [ -f "$(BUILD_DIR)/CMakeCache.txt" ]; then \
		echo "Build Status: Configured"; \
		if [ -f "$(BUILD_DIR)/repo_tests" ]; then \
			echo "  Executable: ✓ Built"; \
		else \
			echo "  Executable: ✗ Not built"; \
		fi \
	else \
		echo "Build Status: Not configured"; \
	fi
