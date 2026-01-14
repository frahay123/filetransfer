#!/bin/bash

# Build script for Android Photo Transfer Project

set -e

echo "=== Building Android Photo Transfer Project ==="

# Check if build directory exists
if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir -p build
fi

cd build

echo "Running CMake..."
cmake ..

echo "Building project..."
make

echo ""
echo "=== Build Complete ==="
echo "Executable: build/android_photo_transfer"
echo ""
echo "To run:"
echo "  cd build && ./android_photo_transfer"
