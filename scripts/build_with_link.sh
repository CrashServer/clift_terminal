#!/bin/bash

# Build script for CLIFT with optional Ableton Link support

echo "Building CLIFT with optional Ableton Link support..."

# Check if we should use real Ableton Link
USE_REAL_LINK=0
LINK_PATH=""

# Check for Link SDK in common locations
for path in "/usr/local/include/ableton" "/usr/include/ableton" "../ableton" "../../ableton" "$HOME/ableton"; do
    if [ -f "$path/Link.hpp" ]; then
        echo "Ableton Link SDK found at: $path"
        USE_REAL_LINK=1
        LINK_PATH="$path"
        break
    fi
done

if [ $USE_REAL_LINK -eq 1 ]; then
    echo "Building with real Ableton Link support."
    LINK_CFLAGS="-I$(dirname "$LINK_PATH")"
    LINK_LIBS=""
    USE_FAKE_LINK_FLAG=""
else
    echo "Ableton Link SDK not found. Building with fake Link implementation."
    echo "To enable real Link support, install the Ableton Link SDK."
    LINK_CFLAGS=""
    LINK_LIBS=""
    USE_FAKE_LINK_FLAG="-DUSE_FAKE_LINK=1"
fi

# Check for dependencies
if ! command -v gcc &> /dev/null; then
    echo "Error: gcc not found. Please install build-essential."
    exit 1
fi

if ! command -v g++ &> /dev/null; then
    echo "Error: g++ not found. Please install build-essential."
    exit 1
fi

if ! ldconfig -p | grep -q libncurses; then
    echo "Error: ncurses not found. Please install libncurses-dev."
    exit 1
fi

# Clean previous build
make -f Makefile clean

# Build with or without real Link
if [ $USE_REAL_LINK -eq 1 ]; then
    make -f Makefile FAKE_LINK_FLAG="" EXTRA_CFLAGS="$LINK_CFLAGS" EXTRA_LDFLAGS="$LINK_LIBS"
else
    make -f Makefile FAKE_LINK_FLAG="-DUSE_FAKE_LINK=1"
fi

if [ $? -eq 0 ]; then
    echo ""
    echo "Build successful!"
    echo "Run with: ./clift"
    if [ $USE_REAL_LINK -eq 1 ]; then
        echo ""
        echo "Real Ableton Link support is enabled."
        echo "CLIFT can now sync with other Link-enabled applications."
    else
        echo ""
        echo "Using fake Link implementation."
        echo "Install the Ableton Link SDK and rebuild for real sync support."
    fi
else
    echo "Build failed!"
    exit 1
fi