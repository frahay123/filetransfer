#!/bin/bash
# Build DMG for macOS
# Requires: Xcode, Qt, macdeployqt

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build-macos"
APP_NAME="Photo Transfer"
DMG_NAME="PhotoTransfer-1.0.0-macOS"

echo "=== Building PhotoTransfer DMG ==="
echo "Project root: $PROJECT_ROOT"

# Clean and create build directory
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Build the project
echo "Building project..."
cmake "$PROJECT_ROOT" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_GUI=ON

make -j$(sysctl -n hw.ncpu)

# Create app bundle structure
echo "Creating app bundle..."
APP_BUNDLE="$BUILD_DIR/$APP_NAME.app"
mkdir -p "$APP_BUNDLE/Contents/MacOS"
mkdir -p "$APP_BUNDLE/Contents/Resources"

# Copy executable
cp photo_transfer_gui "$APP_BUNDLE/Contents/MacOS/"
cp photo_transfer "$APP_BUNDLE/Contents/MacOS/"

# Copy Info.plist
cp "$SCRIPT_DIR/Info.plist" "$APP_BUNDLE/Contents/"

# Create simple icon if iconutil available
if command -v iconutil &> /dev/null; then
    ICONSET="$BUILD_DIR/AppIcon.iconset"
    mkdir -p "$ICONSET"
    
    # Create placeholder icons (replace with actual icons)
    for size in 16 32 64 128 256 512; do
        sips -z $size $size "$SCRIPT_DIR/icon.png" --out "$ICONSET/icon_${size}x${size}.png" 2>/dev/null || true
        sips -z $((size*2)) $((size*2)) "$SCRIPT_DIR/icon.png" --out "$ICONSET/icon_${size}x${size}@2x.png" 2>/dev/null || true
    done
    
    iconutil -c icns "$ICONSET" -o "$APP_BUNDLE/Contents/Resources/AppIcon.icns" 2>/dev/null || true
fi

# Use macdeployqt to bundle Qt frameworks
echo "Bundling Qt frameworks..."
if command -v macdeployqt &> /dev/null; then
    macdeployqt "$APP_BUNDLE" -verbose=1
elif command -v macdeployqt6 &> /dev/null; then
    macdeployqt6 "$APP_BUNDLE" -verbose=1
else
    echo "Warning: macdeployqt not found. Qt frameworks not bundled."
fi

# Code sign (optional, requires Apple Developer certificate)
if [ -n "$APPLE_SIGNING_IDENTITY" ]; then
    echo "Code signing..."
    codesign --force --deep --sign "$APPLE_SIGNING_IDENTITY" "$APP_BUNDLE"
fi

# Create DMG
echo "Creating DMG..."
DMG_PATH="$PROJECT_ROOT/$DMG_NAME.dmg"
rm -f "$DMG_PATH"

# Create temporary DMG directory
DMG_DIR="$BUILD_DIR/dmg"
mkdir -p "$DMG_DIR"
cp -R "$APP_BUNDLE" "$DMG_DIR/"
ln -s /Applications "$DMG_DIR/Applications"

# Create DMG
hdiutil create -volname "$APP_NAME" \
    -srcfolder "$DMG_DIR" \
    -ov -format UDZO \
    "$DMG_PATH"

echo ""
echo "=== DMG Build Complete ==="
echo "Output: $DMG_PATH"
