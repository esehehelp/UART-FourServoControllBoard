#!/bin/bash
# Build script for Servo Controller (Wails v2)
set -e

WAILS="$HOME/go/bin/wails"
BIN_DIR="bin"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

ok()   { echo -e "${GREEN}✓ $1${NC}"; }
err()  { echo -e "${RED}✗ $1${NC}"; }
info() { echo -e "${YELLOW}  $1${NC}"; }
hdr()  { echo -e "${BLUE}=== $1 ===${NC}"; }

build_linux() {
    hdr "Building Linux (amd64)"
    mkdir -p "$BIN_DIR"
    "$WAILS" build -o "$BIN_DIR/servo-controller"
    ok "Linux: $BIN_DIR/servo-controller"
}

build_windows() {
    hdr "Building Windows (amd64)"
    mkdir -p "$BIN_DIR"
    "$WAILS" build -platform windows/amd64 -o "$BIN_DIR/servo-controller.exe"
    ok "Windows: $BIN_DIR/servo-controller.exe"
}

run_tests() {
    hdr "Running Tests"
    go test -v ./test/... && ok "All tests passed" || { err "Tests failed"; return 1; }
}

case "${1:-linux}" in
    linux)   build_linux ;;
    windows) build_windows ;;
    all)     build_linux; build_windows ;;
    test)    run_tests ;;
    dev)     "$WAILS" dev ;;
    clean)
        hdr "Cleaning"
        rm -rf "$BIN_DIR" frontend/dist frontend/node_modules
        go clean
        ok "Clean complete"
        ;;
    help|--help|-h)
        echo "Usage: ./build.sh [linux|windows|all|test|dev|clean]"
        echo "  linux    Build Linux binary (default)"
        echo "  windows  Build Windows binary"
        echo "  all      Build both"
        echo "  test     Run Go unit tests"
        echo "  dev      Start Wails dev server with hot reload"
        echo "  clean    Remove artifacts"
        ;;
    *) err "Unknown: $1"; exit 1 ;;
esac
