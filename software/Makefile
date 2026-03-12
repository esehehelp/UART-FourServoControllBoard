WAILS=$(HOME)/go/bin/wails
BIN_DIR=bin

.PHONY: all build build-linux build-win dev test clean help

all: build-linux

help:
	@echo "Servo Controller (Wails v2)"
	@echo ""
	@echo "  make build-linux   Build Linux binary -> bin/servo-controller"
	@echo "  make build-win     Build Windows binary (requires wails cross-compile)"
	@echo "  make dev           Start dev server with hot reload (wails dev)"
	@echo "  make test          Run Go unit tests"
	@echo "  make clean         Remove build artifacts"

dev:
	@cd software && $(WAILS) dev

build-linux: $(BIN_DIR)
	@echo "Building Linux binary..."
	@$(WAILS) build -o $(BIN_DIR)/servo-controller
	@echo "Done: $(BIN_DIR)/servo-controller"

build-win: $(BIN_DIR)
	@echo "Building Windows binary..."
	@$(WAILS) build -platform windows/amd64 -o $(BIN_DIR)/servo-controller.exe
	@echo "Done: $(BIN_DIR)/servo-controller.exe"

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

test:
	@go test -v ./test/...

clean:
	@rm -rf $(BIN_DIR) frontend/dist frontend/node_modules
	@go clean
