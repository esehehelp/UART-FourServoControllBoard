#!/bin/bash
# Build script for Servo Controller
# Supports Linux and Windows cross-compilation

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BIN_DIR="bin"
LINUX_DIR="$BIN_DIR/linux"
WIN_DIR="$BIN_DIR/windows"
VERSION=$(git describe --tags --always --dirty 2>/dev/null || echo "dev")
BUILD_TIME=$(date -u '+%Y-%m-%d %H:%M:%S UTC')
GIT_COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")

LDFLAGS="-X main.Version=$VERSION -X 'main.BuildTime=$BUILD_TIME' -X main.GitCommit=$GIT_COMMIT"

# Functions
print_header() {
    echo -e "${BLUE}===========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}===========================================${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_info() {
    echo -e "${YELLOW}ℹ $1${NC}"
}

build_linux() {
    print_header "Building for Linux (amd64)"
    
    mkdir -p "$LINUX_DIR"
    
    export GOOS=linux
    export GOARCH=amd64
    export CGO_ENABLED=1
    
    go build -ldflags "$LDFLAGS" -o "$LINUX_DIR/servo-controller" main.go
    
    if [ -f "$LINUX_DIR/servo-controller" ]; then
        SIZE=$(du -h "$LINUX_DIR/servo-controller" | cut -f1)
        print_success "Linux binary built: $LINUX_DIR/servo-controller ($SIZE)"
    else
        print_error "Failed to build Linux binary"
        return 1
    fi
}

build_windows() {
    print_header "Building for Windows (amd64)"
    
    mkdir -p "$WIN_DIR"
    
    export GOOS=windows
    export GOARCH=amd64
    export CGO_ENABLED=1
    export CC=x86_64-w64-mingw32-gcc
    export CXX=x86_64-w64-mingw32-g++
    
    go build -ldflags "$LDFLAGS" -o "$WIN_DIR/servo-controller.exe" main.go
    
    if [ -f "$WIN_DIR/servo-controller.exe" ]; then
        SIZE=$(du -h "$WIN_DIR/servo-controller.exe" | cut -f1)
        print_success "Windows binary built: $WIN_DIR/servo-controller.exe ($SIZE)"
    else
        print_error "Failed to build Windows binary"
        print_info "Note: Windows cross-compilation requires mingw32-gcc"
        print_info "Install with: sudo apt-get install mingw-w64"
        return 1
    fi
}

run_tests() {
    print_header "Running Tests"
    
    if go test -v ./test; then
        print_success "All tests passed"
    else
        print_error "Tests failed"
        return 1
    fi
}

show_version() {
    echo ""
    echo "Build Information:"
    echo "  Version:     $VERSION"
    echo "  Build Time:  $BUILD_TIME"
    echo "  Git Commit:  $GIT_COMMIT"
    echo ""
}

show_summary() {
    print_header "Build Summary"
    
    echo ""
    echo "Output Binaries:"
    
    if [ -f "$LINUX_DIR/servo-controller" ]; then
        SIZE=$(du -h "$LINUX_DIR/servo-controller" | cut -f1)
        echo "  • Linux:   $LINUX_DIR/servo-controller ($SIZE)"
    fi
    
    if [ -f "$WIN_DIR/servo-controller.exe" ]; then
        SIZE=$(du -h "$WIN_DIR/servo-controller.exe" | cut -f1)
        echo "  • Windows: $WIN_DIR/servo-controller.exe ($SIZE)"
    fi
    
    echo ""
}

# Main script
main() {
    case "${1:-all}" in
        linux)
            build_linux || exit 1
            ;;
        windows)
            build_windows || exit 1
            ;;
        test)
            run_tests || exit 1
            ;;
        all)
            print_info "Building for all platforms..."
            build_linux || exit 1
            build_windows || exit 1
            run_tests || exit 1
            ;;
        clean)
            print_header "Cleaning Build Artifacts"
            rm -rf "$BIN_DIR"
            go clean
            print_success "Clean complete"
            ;;
        help|--help|-h)
            echo "Servo Controller Build Script"
            echo ""
            echo "Usage: ./build.sh [COMMAND]"
            echo ""
            echo "Commands:"
            echo "  linux       Build for Linux (amd64)"
            echo "  windows     Build for Windows (amd64)"
            echo "  all         Build for all platforms (default)"
            echo "  test        Run unit tests"
            echo "  clean       Remove build artifacts"
            echo "  help        Show this help message"
            echo ""
            ;;
        *)
            print_error "Unknown command: $1"
            echo "Run './build.sh help' for usage"
            exit 1
            ;;
    esac
    
    show_summary
    show_version
}

main "$@"
