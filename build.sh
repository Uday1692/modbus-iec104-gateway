#!/bin/bash
# Quick build script for native architecture using Zig

set -e

echo "Building for native architecture..."

mkdir -p build
cd build
rm -rf *

echo "[1/4] Compiling modbus_master.c..."
zig cc -O2 -c ../src/modbus_master.c -I../src -o modbus_master.o

echo "[2/4] Compiling iec104_server.c..."
zig cc -O2 -c ../src/iec104_server.c -I../src -o iec104_server.o

echo "[3/4] Compiling gateway.c..."
zig cc -O2 -c ../src/gateway.c -I../src -o gateway.o

echo "[4/4] Compiling main.c..."
zig cc -O2 -c ../src/main.c -I../src -o main.o

echo "Linking executable..."
zig cc -o gateway modbus_master.o iec104_server.o gateway.o main.o -lmodbus -lpthread -lm

echo ""
echo "✓ Build complete!"
echo "Binary: ./build/gateway"
