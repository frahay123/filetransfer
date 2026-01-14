#!/bin/bash
# Build AppImage for Linux
# Requires: linuxdeploy, Qt

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build-appimage"
APPDIR="$BUILD_DIR/AppDir"

echo "=== Building PhotoTransfer AppImage ==="
echo "Project root: $PROJECT_ROOT"

# Clean and create build directory
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Build the project
echo "Building project..."
cmake "$PROJECT_ROOT" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DBUILD_GUI=ON

make -j$(nproc)

# Create AppDir structure
echo "Creating AppDir..."
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/lib"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"

# Copy executables
cp photo_transfer "$APPDIR/usr/bin/"
cp photo_transfer_gui "$APPDIR/usr/bin/" 2>/dev/null || true

# Create desktop file
cat > "$APPDIR/usr/share/applications/photo-transfer.desktop" << 'EOF'
[Desktop Entry]
Type=Application
Name=Photo Transfer
Comment=Transfer photos from mobile devices
Exec=photo_transfer_gui
Icon=photo-transfer
Categories=Utility;Graphics;
Terminal=false
EOF

# Create icon (placeholder - replace with actual icon)
convert -size 256x256 xc:#4a90d9 \
    -fill white -gravity center \
    -font DejaVu-Sans-Bold -pointsize 72 \
    -annotate 0 "PT" \
    "$APPDIR/usr/share/icons/hicolor/256x256/apps/photo-transfer.png" 2>/dev/null || \
    echo "Note: Install imagemagick for icon generation"

# Download linuxdeploy if not present
if [ ! -f "$BUILD_DIR/linuxdeploy-x86_64.AppImage" ]; then
    echo "Downloading linuxdeploy..."
    wget -q "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
    chmod +x linuxdeploy-x86_64.AppImage
fi

# Download linuxdeploy Qt plugin
if [ ! -f "$BUILD_DIR/linuxdeploy-plugin-qt-x86_64.AppImage" ]; then
    echo "Downloading linuxdeploy Qt plugin..."
    wget -q "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
    chmod +x linuxdeploy-plugin-qt-x86_64.AppImage
fi

# Build AppImage
echo "Building AppImage..."
export QMAKE=$(which qmake6 || which qmake)
./linuxdeploy-x86_64.AppImage \
    --appdir "$APPDIR" \
    --plugin qt \
    --output appimage \
    --desktop-file "$APPDIR/usr/share/applications/photo-transfer.desktop" || \
    echo "AppImage build may require running on the target distribution"

# Move result
mv Photo_Transfer*.AppImage "$PROJECT_ROOT/" 2>/dev/null || true

echo ""
echo "=== AppImage Build Complete ==="
echo "Output: $PROJECT_ROOT/Photo_Transfer-*.AppImage"
