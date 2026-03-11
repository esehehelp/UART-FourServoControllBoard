# Build targets
BINARY_NAME_LINUX=servo-controller
BINARY_NAME_WIN=servo-controller.exe
BIN_DIR=bin
OUT_DIR_LINUX=$(BIN_DIR)/linux
OUT_DIR_WIN=$(BIN_DIR)/windows

# Version info
VERSION?=$(shell git describe --tags --always --dirty 2>/dev/null || echo "dev")
BUILD_TIME?=$(shell date -u '+%Y-%m-%d %H:%M:%S UTC')
GIT_COMMIT?=$(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")

# Go build flags
LDFLAGS=-ldflags "-X main.Version=$(VERSION) -X 'main.BuildTime=$(BUILD_TIME)' -X main.GitCommit=$(GIT_COMMIT)"
GO=go

.PHONY: all build build-linux build-win clean help test

all: clean build

help:
	@echo "Servo Controller Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  make build        - Build for current OS (Linux or Windows)"
	@echo "  make build-linux  - Build for Linux (amd64)"
	@echo "  make build-win    - Build for Windows (amd64)"
	@echo "  make all          - Clean and build for both platforms"
	@echo "  make test         - Run tests"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make help         - Show this help message"
	@echo ""
	@echo "Build output:"
	@echo "  Linux:   $(OUT_DIR_LINUX)/$(BINARY_NAME_LINUX)"
	@echo "  Windows: $(OUT_DIR_WIN)/$(BINARY_NAME_WIN)"

# Create bin directories
$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

$(OUT_DIR_LINUX): $(BIN_DIR)
	@mkdir -p $(OUT_DIR_LINUX)

$(OUT_DIR_WIN): $(BIN_DIR)
	@mkdir -p $(OUT_DIR_WIN)

# Build for current platform
build: $(OUT_DIR_LINUX) $(OUT_DIR_WIN)
	@echo "Building for current platform..."
	@if [ "$$(uname -s)" = "Darwin" ] || [ "$$(uname -s)" = "Linux" ]; then \
		$(MAKE) build-linux; \
	elif [ "$$(uname -s)" = "MINGW64_NT" ] || [ "$$(uname -s)" = "MSYS_NT" ]; then \
		$(MAKE) build-win; \
	else \
		echo "Unsupported OS. Building for Linux and Windows..."; \
		$(MAKE) build-linux build-win; \
	fi

# Linux build
build-linux: $(OUT_DIR_LINUX)
	@echo "Building for Linux (amd64)..."
	@GOOS=linux GOARCH=amd64 CGO_ENABLED=1 $(GO) build $(LDFLAGS) -o $(OUT_DIR_LINUX)/$(BINARY_NAME_LINUX) main.go
	@echo "✓ Linux binary: $(OUT_DIR_LINUX)/$(BINARY_NAME_LINUX)"

# Windows build
build-win: $(OUT_DIR_WIN)
	@echo "Building for Windows (amd64)..."
	@GOOS=windows GOARCH=amd64 CGO_ENABLED=1 CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++ $(GO) build $(LDFLAGS) -o $(OUT_DIR_WIN)/$(BINARY_NAME_WIN) main.go
	@echo "✓ Windows binary: $(OUT_DIR_WIN)/$(BINARY_NAME_WIN)"

# Run tests
test:
	@echo "Running tests..."
	@$(GO) test -v ./test

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BIN_DIR)
	@$(GO) clean
	@echo "✓ Clean complete"

# Install (copy to ~/.local/bin on Linux)
install: build-linux
	@if [ -d "$$HOME/.local/bin" ]; then \
		cp $(OUT_DIR_LINUX)/$(BINARY_NAME_LINUX) $$HOME/.local/bin/; \
		echo "✓ Installed to $$HOME/.local/bin/$(BINARY_NAME_LINUX)"; \
	else \
		echo "Warning: $$HOME/.local/bin does not exist"; \
	fi

# Show version
version:
	@echo "Version: $(VERSION)"
	@echo "Build Time: $(BUILD_TIME)"
	@echo "Git Commit: $(GIT_COMMIT)"
