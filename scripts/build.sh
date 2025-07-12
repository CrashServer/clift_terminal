#!/bin/bash

# Build script for CLIFT (CLI-Shift)

echo "Building CLIFT (CLI-Shift)..."

# Check for dependencies
if ! command -v gcc &> /dev/null; then
    echo "Error: gcc not found. Please install build-essential."
    exit 1
fi

if ! ldconfig -p | grep -q libncurses; then
    echo "Error: ncurses not found. Please install libncurses-dev."
    exit 1
fi

# Build
make -f Makefile clean
make -f Makefile

if [ $? -eq 0 ]; then
    echo "Build successful!"
    echo "Run with: ./clift"
else
    echo "Build failed!"
    exit 1
fi