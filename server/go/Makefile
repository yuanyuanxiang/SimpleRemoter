# Makefile for SimpleRemoter Go Server
# Compatible with GnuWin32 make on Windows cmd

# Binary name
BINARY_NAME=server

# Build directory
BUILD_DIR=build

# Main package path
MAIN_PACKAGE=./cmd

# Go build flags (strip debug info for smaller binary)
LDFLAGS=-ldflags "-s -w"

# Windows commands
MKDIR=if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
RMDIR=if exist $(BUILD_DIR) rmdir /s /q $(BUILD_DIR)

.PHONY: all clean windows linux build help windows-386 linux-arm64 all-platforms test fmt deps

# Default target
all: clean windows linux

# Build for current platform
build:
	@echo Building for current platform...
	@$(MKDIR)
	go build $(LDFLAGS) -o $(BUILD_DIR)\$(BINARY_NAME).exe $(MAIN_PACKAGE)
	@echo Done

# Build for Windows (amd64)
windows:
	@echo Building for Windows amd64...
	@$(MKDIR)
	set GOOS=windows&& set GOARCH=amd64&& go build $(LDFLAGS) -o $(BUILD_DIR)\$(BINARY_NAME)_windows_amd64.exe $(MAIN_PACKAGE)
	@echo Done: $(BUILD_DIR)\$(BINARY_NAME)_windows_amd64.exe

# Build for Windows (386)
windows-386:
	@echo Building for Windows 386...
	@$(MKDIR)
	set GOOS=windows&& set GOARCH=386&& go build $(LDFLAGS) -o $(BUILD_DIR)\$(BINARY_NAME)_windows_386.exe $(MAIN_PACKAGE)
	@echo Done: $(BUILD_DIR)\$(BINARY_NAME)_windows_386.exe

# Build for Linux (amd64)
linux:
	@echo Building for Linux amd64...
	@$(MKDIR)
	set GOOS=linux&& set GOARCH=amd64&& go build $(LDFLAGS) -o $(BUILD_DIR)\$(BINARY_NAME)_linux_amd64 $(MAIN_PACKAGE)
	@echo Done: $(BUILD_DIR)\$(BINARY_NAME)_linux_amd64

# Build for Linux (arm64)
linux-arm64:
	@echo Building for Linux arm64...
	@$(MKDIR)
	set GOOS=linux&& set GOARCH=arm64&& go build $(LDFLAGS) -o $(BUILD_DIR)\$(BINARY_NAME)_linux_arm64 $(MAIN_PACKAGE)
	@echo Done: $(BUILD_DIR)\$(BINARY_NAME)_linux_arm64

# Build all platforms
all-platforms: windows windows-386 linux linux-arm64

# Clean build artifacts
clean:
	@echo Cleaning...
	@$(RMDIR)
	@echo Done

# Run tests
test:
	go test -v ./...

# Format code
fmt:
	go fmt ./...

# Download dependencies
deps:
	go mod download
	go mod tidy

# Show help
help:
	@echo Available targets:
	@echo   all           - Build for Windows and Linux amd64
	@echo   build         - Build for current platform
	@echo   windows       - Build for Windows amd64
	@echo   windows-386   - Build for Windows 386
	@echo   linux         - Build for Linux amd64
	@echo   linux-arm64   - Build for Linux arm64
	@echo   all-platforms - Build for all platforms
	@echo   clean         - Clean build directory
	@echo   test          - Run tests
	@echo   fmt           - Format code
	@echo   deps          - Download dependencies
