#!/bin/bash
# Quick build script for native architecture using Zig

set -e

echo "Building for native architecture..."

mkdir -p build
cd build
rm -rf *

# Find libmodbus include directory
MODBUS_INCLUDE=""
if pkg-config --exists libmodbus 2>/dev/null; then
    MODBUS_INCLUDE=$(pkg-config --cflags libmodbus)
fi

zig cc -O2 -c ../src/modbus_master.c -I../src ${MODBUS_INCLUDE} -o modbus_master.o
zig cc -O2 -c ../src/iec104_protocol.c -I../src ${MODBUS_INCLUDE} -o iec104_protocol.o
zig cc -O2 -c ../src/iec104_server.c -I../src ${MODBUS_INCLUDE} -o iec104_server.o
zig cc -O2 -c ../src/config_parser.c -I../src ${MODBUS_INCLUDE} -o config_parser.o
zig cc -O2 -c ../src/gateway.c -I../src ${MODBUS_INCLUDE} -o gateway.o
zig cc -O2 -c ../src/main.c -I../src ${MODBUS_INCLUDE} -o main.o

echo "Linking executable..."
MODBUS_LIB=""
if pkg-config --exists libmodbus 2>/dev/null; then
    MODBUS_LIB=$(pkg-config --libs libmodbus)
else
    MODBUS_LIB="-lmodbus"
fi
zig cc -o gateway modbus_master.o iec104_protocol.o iec104_server.o config_parser.o gateway.o main.o ${MODBUS_LIB} -lpthread -lm

echo ""
echo "Build complete!"
echo "Binary: ./build/gateway"
