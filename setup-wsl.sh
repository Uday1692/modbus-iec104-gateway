#!/bin/bash
# Complete cross-compilation setup script for WSL

set -e

echo "========================================"
echo "Modbus-IEC104 Gateway Setup"
echo "WSL Environment"
echo "========================================"
echo ""

# Update package lists
echo "[1/4] Updating package lists..."
sudo apt-get update -qq

# Install dependencies
echo "[2/4] Installing dependencies..."
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libmodbus-dev \
    2>&1 | grep -E '(Setting up|Processing)'

# Install Zig (for cross-compilation)
echo "[3/4] Checking Zig installation..."
if ! command -v zig &> /dev/null; then
    echo "Installing Zig..."
    # Download latest Zig
    cd /tmp
    wget -q https://ziglang.org/download/0.13.0/zig-linux-x86_64-0.13.0.tar.xz
    tar -xf zig-linux-x86_64-0.13.0.tar.xz
    sudo mv zig-linux-x86_64-0.13.0 /opt/zig
    sudo ln -sf /opt/zig/zig /usr/local/bin/zig
    cd -
    echo "✓ Zig installed"
else
    echo "✓ Zig already installed"
fi

# Verify installation
echo "[4/4] Verifying installation..."
echo "Zig version: $(zig version)"
echo "Modbus library: $(pkg-config --modversion libmodbus)"

echo ""
echo "========================================"
echo "✓ Setup Complete!"
echo "========================================"
echo ""
echo "Next steps:"
echo "1. For native build:      bash build.sh"
echo "2. For ARM64 build:       bash build-arm64.sh"
echo ""
