# Photo Transfer

Transfer photos and videos from your phone to your computer. Works with Android and iPhone/iPad.

![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-blue)
![License](https://img.shields.io/badge/License-MIT-green)

## Features

- ✅ **Android & iOS Support** - Works with Samsung, Pixel, iPhone, iPad, and more
- ✅ **Smart Deduplication** - Never transfer the same photo twice
- ✅ **HEIC to JPEG Conversion** - Automatically convert Apple photos for compatibility
- ✅ **Resume Transfers** - Pick up where you left off if interrupted
- ✅ **10 Color Themes** - Customize the look to your preference
- ✅ **Photo Preview** - See photos before transferring

---

## Download

### Linux
```bash
# Download the AppImage
wget https://github.com/frahay123/filetransfer/releases/latest/download/PhotoTransfer-Linux.AppImage

# Make it executable
chmod +x PhotoTransfer-Linux.AppImage

# Run it
./PhotoTransfer-Linux.AppImage
```

### macOS
1. Download `PhotoTransfer-macOS.dmg` from [Releases](https://github.com/frahay123/filetransfer/releases)
2. Open the DMG file
3. Drag **Photo Transfer** to your Applications folder
4. Open from Applications (right-click → Open on first launch)

### Windows
1. Download `PhotoTransfer-Windows.zip` from [Releases](https://github.com/frahay123/filetransfer/releases)
2. Extract the ZIP file
3. Run `photo_transfer_gui.exe`

---

## Build from Source

### Linux (Ubuntu/Debian)
```bash
# Install dependencies
sudo apt install build-essential cmake pkg-config \
    libmtp-dev libsqlite3-dev libssl-dev \
    libimobiledevice-dev libplist-dev usbmuxd \
    qt6-base-dev qt6-tools-dev

# Build
mkdir build && cd build
cmake .. -DBUILD_GUI=ON
make -j$(nproc)

# Run
./photo_transfer_gui
```

### macOS
```bash
# Install dependencies
brew install cmake qt@6 libmtp libimobiledevice openssl@3 sqlite3

# Build
mkdir build && cd build
cmake .. -DBUILD_GUI=ON \
    -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3) \
    -DQt6_DIR=$(brew --prefix qt@6)/lib/cmake/Qt6
make -j$(sysctl -n hw.ncpu)

# Run
./photo_transfer_gui
```

### Windows
```powershell
# Install Qt from https://www.qt.io/download
# Install vcpkg and dependencies:
vcpkg install sqlite3:x64-windows openssl:x64-windows

# Build
mkdir build && cd build
cmake .. -DBUILD_GUI=ON -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

---

## Usage

1. **Connect your phone** via USB cable
2. **Select your device** from the dropdown
3. **Click Connect**
4. **Select photos** to transfer (or click "Select All")
5. **Choose destination folder**
6. **Click Start Transfer**

### For iPhone Users
- When you connect, the app will detect HEIC photos
- Check "Convert HEIC to JPEG" if you want compatibility with all devices
- Choose quality level (90% recommended)

---

## Themes

Go to **File → Settings** to choose from 10 color themes:
- Light, Dark, Midnight, Forest, Sunset, Ocean, Purple, Slate, Rose, High Contrast

---

## Troubleshooting

### Phone not detected?
- **Android**: Enable "File Transfer" or "MTP" mode when prompted
- **iPhone**: Tap "Trust" when asked to trust this computer

### Permission denied (Linux)?
```bash
sudo usermod -aG plugdev $USER
# Log out and back in
```

---

## License

MIT License - Free to use and modify.
