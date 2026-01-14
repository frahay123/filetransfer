#!/bin/bash

# Photo Transfer - Dependency Installation Script

echo "=== Photo Transfer Dependency Installer ==="
echo ""

# Detect OS
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    if command -v apt &> /dev/null; then
        echo "Detected: Ubuntu/Debian Linux"
        echo ""
        echo "Installing dependencies..."
        sudo apt update
        sudo apt install -y \
            build-essential \
            cmake \
            pkg-config \
            libmtp-dev \
            libfuse3-dev \
            libsqlite3-dev \
            libssl-dev \
            libimobiledevice-dev \
            libplist-dev \
            usbmuxd
        
        echo ""
        echo "Adding user to plugdev group..."
        sudo usermod -a -G plugdev $USER
        
        echo ""
        echo "Creating udev rules for Samsung devices..."
        echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="04e8", MODE="0664", GROUP="plugdev"' | \
            sudo tee /etc/udev/rules.d/51-android.rules
        sudo udevadm control --reload-rules
        
    elif command -v dnf &> /dev/null; then
        echo "Detected: Fedora/RHEL Linux"
        sudo dnf install -y \
            gcc-c++ \
            cmake \
            pkgconfig \
            libmtp-devel \
            fuse3-devel \
            sqlite-devel \
            openssl-devel \
            libimobiledevice-devel \
            libplist-devel \
            usbmuxd
    elif command -v pacman &> /dev/null; then
        echo "Detected: Arch Linux"
        sudo pacman -Syu --noconfirm \
            base-devel \
            cmake \
            libmtp \
            fuse3 \
            sqlite \
            openssl \
            libimobiledevice \
            libplist \
            usbmuxd
    fi
elif [[ "$OSTYPE" == "darwin"* ]]; then
    echo "Detected: macOS"
    if command -v brew &> /dev/null; then
        echo "Installing via Homebrew..."
        brew install cmake libmtp libimobiledevice openssl sqlite3 macfuse
    else
        echo "Homebrew not found. Please install Homebrew first:"
        echo "/bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
        exit 1
    fi
else
    echo "Unsupported operating system: $OSTYPE"
    echo "Please install dependencies manually."
    exit 1
fi

echo ""
echo "=== Installation Complete ==="
echo ""
echo "Next steps:"
echo "1. Log out and back in (for group changes to take effect)"
echo "2. Build the project:"
echo "   cd build && cmake .. && make -j\$(nproc)"
echo "3. Run the application:"
echo "   ./photo_transfer"
