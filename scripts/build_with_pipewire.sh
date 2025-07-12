#!/bin/bash

# Build script for CLIFT with optional PipeWire support

echo "Building CLIFT with audio support..."

# Check if we should use PipeWire
USE_PIPEWIRE=0
if command -v pkg-config &> /dev/null && pkg-config --exists libpipewire-0.3; then
    echo "PipeWire development libraries found. Building with PipeWire support."
    USE_PIPEWIRE=1
    PIPEWIRE_CFLAGS=$(pkg-config --cflags libpipewire-0.3)
    PIPEWIRE_LIBS=$(pkg-config --libs libpipewire-0.3)
else
    echo "PipeWire development libraries not found. Building with test signal only."
    echo "To enable PipeWire support, install: libpipewire-0.3-dev"
    PIPEWIRE_CFLAGS=""
    PIPEWIRE_LIBS=""
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

# Build with or without PipeWire
if [ $USE_PIPEWIRE -eq 1 ]; then
    make -f Makefile EXTRA_CFLAGS="-DUSE_PIPEWIRE=1 $PIPEWIRE_CFLAGS" EXTRA_LDFLAGS="$PIPEWIRE_LIBS"
else
    make -f Makefile
fi

if [ $? -eq 0 ]; then
    echo ""
    echo "Build successful!"
    echo "Run with: ./clift"
    if [ $USE_PIPEWIRE -eq 1 ]; then
        echo ""
        echo "PipeWire audio input is enabled. You can:"
        echo "1. Use pw-link to connect audio sources to CLIFT"
        echo "2. Use Carla or qpwgraph to route audio"
        echo "3. CLIFT will auto-connect to the default monitor"
    else
        echo ""
        echo "Using test signal for audio visualization."
        echo "Install libpipewire-0.3-dev and rebuild for real audio input."
    fi
else
    echo "Build failed!"
    exit 1
fi