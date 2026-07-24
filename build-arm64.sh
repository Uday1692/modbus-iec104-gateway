#!/bin/bash
# Build Script for WSL + Zig Cross-Compilation
# This script compiles for ARM64 using Zig's cross-compiler

set -e

echo "========================================"
echo "Modbus-IEC104 Gateway ARM64 Build"
echo "Using Zig Cross-Compiler (zig cc)"
echo "========================================"

# Check if zig is installed
if ! command -v zig &> /dev/null; then
    echo "ERROR: Zig not found. Please install Zig first."
    echo "Visit: https://ziglang.org/download/"
    exit 1
fi

echo "Zig version:"
zig version

# Create build directory
mkdir -p build-arm64
cd build-arm64

# Clean previous build
rm -rf *.o gateway

echo ""
echo "Compiling sources for ARM64 Linux..."
echo ""

# Compile Modbus Master
echo "[1/6] Compiling modbus_master.c..."
zig cc -target aarch64-linux-gnu -O2 -c ../src/modbus_master.c -I../src -o modbus_master.o

# Compile IEC104 Protocol
echo "[2/6] Compiling iec104_protocol.c..."
zig cc -target aarch64-linux-gnu -O2 -c ../src/iec104_protocol.c -I../src -o iec104_protocol.o

# Compile IEC104 Server
echo "[3/6] Compiling iec104_server.c..."
zig cc -target aarch64-linux-gnu -O2 -c ../src/iec104_server.c -I../src -o iec104_server.o

# Compile Gateway
echo "[4/6] Compiling gateway.c..."
zig cc -target aarch64-linux-gnu -O2 -c ../src/gateway.c -I../src -o gateway.o

# Compile Main
echo "[5/6] Compiling main.c..."
zig cc -target aarch64-linux-gnu -O2 -c ../src/main.c -I../src -o main.o

# Link all object files
echo "[6/6] Linking executable..."
zig cc -target aarch64-linux-gnu -o gateway \
    modbus_master.o \
    iec104_protocol.o \
    iec104_server.o \
    gateway.o \
    main.o \
    -lmodbus -lpthread -lm

echo ""
echo "========================================"
echo "✓ Build Complete!"
echo "========================================"
echo ""
echo "Binary location: ./build-arm64/gateway"
echo "Target: ARM64 Linux (aarch64-linux-gnu)"
echo ""
echo "Compiled modules:"
echo "  - Modbus Master (TCP/RTU)"
echo "  - IEC 104 Protocol (APDU encoding/decoding)"
echo "  - IEC 104 Server (Multi-client support)"
echo "  - Gateway Bridge (Modbus → IEC 104)"
echo ""
echo "To deploy to ARM64 device:"
echo "  scp build-arm64/gateway user@device:/opt/gateway/"
echo "  ssh user@device '/opt/gateway/gateway -h'"
echo ""
