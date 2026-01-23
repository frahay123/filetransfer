# Photo Transfer

A fast, modern photo transfer app built with React, Tailwind CSS, and Tauri (Rust).

![Photo Transfer](https://img.shields.io/badge/version-2.0.0-blue)
![Platforms](https://img.shields.io/badge/platforms-Windows%20%7C%20macOS%20%7C%20Linux-green)

## Features

- 📱 **Universal Device Support** - Android (MTP) and iOS devices
- ⚡ **Fast Transfers** - Native Rust backend for maximum speed
- 🎨 **Modern UI** - Beautiful React + Tailwind CSS interface
- 📁 **Smart Organization** - Organize photos by date automatically
- 🔄 **Duplicate Detection** - Skip files already transferred
- 💾 **Lightweight** - ~10MB app size

## Installation

### Pre-built Binaries

Download from [Releases](../../releases):

| Platform | Download |
|----------|----------|
| Windows | `.exe` installer or `.msi` |
| macOS | `.dmg` disk image |
| Linux | `.AppImage` or `.deb` |

### Windows (iOS Support)

For iPhone/iPad support on Windows, install [iTunes](https://www.apple.com/itunes/) first.

### macOS (Gatekeeper)

Since the app isn't code-signed:
1. Download and open the `.dmg`
2. Drag to Applications
3. **Right-click** → **Open** (first time only)

## Development

### Prerequisites

- [Node.js](https://nodejs.org/) 18+
- [Rust](https://rustup.rs/) 1.70+
- [Tauri prerequisites](https://tauri.app/v1/guides/getting-started/prerequisites)

### Setup

```bash
# Install dependencies
npm install

# Run in development mode
npm run tauri dev

# Build for production
npm run tauri build
```

### Linux Dependencies

```bash
sudo apt install libwebkit2gtk-4.0-dev libssl-dev libgtk-3-dev \
  libayatana-appindicator3-dev librsvg2-dev libusb-1.0-0-dev \
  libmtp-dev libimobiledevice-dev ifuse
```

### macOS Dependencies

```bash
brew install libmtp libimobiledevice
```

## Project Structure

```
photo-transfer/
├── src/                    # React frontend
│   ├── components/         # UI components
│   ├── App.tsx            # Main app
│   └── index.css          # Tailwind styles
├── src-tauri/             # Rust backend
│   └── src/
│       ├── main.rs        # Tauri commands
│       ├── device.rs      # Device detection
│       └── transfer.rs    # File transfer
├── src-cpp/               # Legacy C++ code (reference)
├── package.json
└── tailwind.config.js
```

## Tech Stack

- **Frontend**: React 18, TypeScript, Tailwind CSS
- **Backend**: Rust, Tauri
- **Device Access**: 
  - Linux/macOS: libmtp, libimobiledevice
  - Windows: Windows Portable Devices (WPD) API
- **Build**: Vite, Cargo

## License

MIT
