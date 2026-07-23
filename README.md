# Modbus Master to IEC 104 Server Gateway

A cross-platform gateway application that bridges Modbus RTU/TCP master devices with IEC 60870-5-104 (IEC 104) server protocols. Optimized for ARM64 deployment.

## Features

- **Modbus Master**: Reads data from Modbus RTU/TCP devices
- **IEC 104 Server**: Exposes data via IEC 104 protocol
- **Cross-compilation**: Easy ARM64 compilation with provided toolchain
- **Docker Support**: Containerized build environment
- **Configurable**: Support for multiple Modbus devices and IEC 104 mappings

## Requirements

### Host Machine
- CMake 3.10+
- GCC or Clang
- ARM64 cross-compilation toolchain (for ARM64 builds)

### For Docker Build
- Docker

## Building

### Native Build (Host Architecture)

```bash
mkdir build
cd build
cmake ..
make
```

### ARM64 Cross-Compilation

#### Option 1: Using CMake Toolchain

```bash
mkdir build-arm64
cd build-arm64
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-arm64.cmake ..
make
```

#### Option 2: Using Docker (Recommended)

```bash
docker build -f Dockerfile.build-arm64 -t modbus-iec104-builder:latest .
docker run --rm -v $(pwd):/workspace modbus-iec104-builder:latest
# Binary will be in build-arm64/ directory
```

## Installation on ARM64 Target

1. Cross-compile the project (see Building section)
2. Copy the binary to your ARM64 device:
   ```bash
   scp build-arm64/bin/gateway arm64-device:/opt/gateway/
   ```
3. Copy configuration files:
   ```bash
   scp config/gateway.conf arm64-device:/etc/gateway/
   ```
4. Run on the target device:
   ```bash
   ./gateway -c /etc/gateway/gateway.conf
   ```

## Configuration

See `config/gateway.conf.example` for configuration options including:
- Modbus device addresses and ports
- IEC 104 server settings
- Data mapping between protocols
- Logging configuration

## Architecture

```
┌─────────────────────────────────────────┐
│      Modbus RTU/TCP Devices             │
└────────────────┬────────────────────────┘
                 │ (Read Modbus Data)
                 ▼
        ┌────────────────────┐
        │  Modbus Master     │
        │   (libmodbus)      │
        └────────┬───────────┘
                 │ (Gateway Bridge)
                 ▼
        ┌────────────────────┐
        │   IEC 104 Server   │
        │   (libasdu)        │
        └────────┬───────────┘
                 │ (Send IEC 104 Data)
                 ▼
┌─────────────────────────────────────────┐
│      IEC 104 Clients                    │
└─────────────────────────────────────────┘
```

## Dependencies

- `libmodbus` - Modbus RTU/TCP library
- `libasdu` - IEC 104 protocol implementation (optional for basic build)

## Project Structure

```
modbus-iec104-gateway/
├── CMakeLists.txt
├── toolchain-arm64.cmake
├── Dockerfile.build-arm64
├── README.md
├── .gitignore
├── src/
│   ├── main.c
│   ├── modbus_master.c
│   ├── modbus_master.h
│   ├── iec104_server.c
│   ├── iec104_server.h
│   └── gateway.c
├── config/
│   └── gateway.conf.example
└── build/
```

## Usage

```bash
# Show help
./gateway -h

# Show version
./gateway -v

# Run with configuration file
./gateway -c /etc/gateway/gateway.conf
```

## License

MIT

## Support

For issues and questions, please open an issue on GitHub.
