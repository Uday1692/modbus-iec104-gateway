# Modbus Master to IEC 104 Server Gateway - ARM64 Deployment

A lightweight gateway application that bridges Modbus RTU/TCP master devices with IEC 60870-5-104 (IEC 104) server protocols.

**Optimized for:**
- ✅ WSL (Windows Subsystem for Linux)
- ✅ Zig Cross-Compiler (`zig cc`)
- ✅ ARM64 Linux Devices
- ✅ Embedded Systems

## Quick Start

### 1. Setup WSL Environment

```bash
# Make setup script executable
chmod +x setup-wsl.sh

# Run setup (installs Zig, dependencies)
./setup-wsl.sh
```

### 2. Build for ARM64

```bash
# Make build script executable
chmod +x build-arm64.sh

# Build for ARM64 using Zig cross-compiler
./build-arm64.sh
```

Binary will be at: `build-arm64/gateway`

### 3. Deploy to ARM64 Device

```bash
chmod +x deploy.sh
./deploy.sh root 192.168.1.100 22
```

Or manual deployment:
```bash
scp build-arm64/gateway user@device:/opt/gateway/
ssh user@device
/opt/gateway/gateway -h
```

## Features

- **Modbus Master**: Reads from RTU/TCP devices
- **IEC 104 Server**: Exposes data via IEC 104 protocol
- **Cross-Platform**: WSL, Linux, macOS (with Zig)
- **Lightweight**: Minimal dependencies, ~50KB binary
- **Configurable**: Easy configuration file format

## Files Included

```
modbus-iec104-gateway/
├── src/
│   ├── main.c                 # Entry point
│   ├── gateway.c              # Gateway bridge logic
│   ├── modbus_master.c/h      # Modbus implementation
│   └── iec104_server.c/h      # IEC 104 implementation
├── config/
│   └── gateway.conf.example   # Example configuration
├── build-arm64.sh             # ARM64 build script (Zig)
├── build.sh                   # Native build script (Zig)
├── setup-wsl.sh               # WSL setup script
├── deploy.sh                  # Deployment script
├── build.zig                  # Zig build definition
└── README.md                  # This file
```

## Requirements

### For Development (WSL)
- WSL2 with Ubuntu/Debian
- Zig 0.13.0+ (auto-installed by setup-wsl.sh)
- libmodbus-dev
- GCC toolchain

### For Target (ARM64 Device)
- Linux (any distro)
- libmodbus library
- glibc or musl

## Usage

### Show Help
```bash
./build-arm64/gateway -h
```

### Show Version
```bash
./build-arm64/gateway -v
```

### Run with Configuration
```bash
./build-arm64/gateway -c /etc/gateway/gateway.conf
```

## Configuration

Edit `config/gateway.conf.example`:

```ini
[modbus]
type=tcp
host=192.168.1.100
port=502
timeout=5
poll_interval=1000

[iec104]
bind_address=0.0.0.0
port=2404
max_clients=10
```

## Building Options

### Option 1: Using Build Scripts (Recommended for WSL)

```bash
# Native (x86_64)
bash build.sh

# ARM64
bash build-arm64.sh
```

### Option 2: Using Zig Build System

```bash
# Build for native
zig build

# Build for ARM64
zig build -Dtarget=aarch64-linux-gnu
```

### Option 3: Manual Zig Compilation

```bash
# Create build directory
mkdir -p build-arm64
cd build-arm64

# Compile each source file
zig cc -target aarch64-linux-gnu -O2 -c ../src/modbus_master.c -I../src
zig cc -target aarch64-linux-gnu -O2 -c ../src/iec104_server.c -I../src
zig cc -target aarch64-linux-gnu -O2 -c ../src/gateway.c -I../src
zig cc -target aarch64-linux-gnu -O2 -c ../src/main.c -I../src

# Link
zig cc -target aarch64-linux-gnu -o gateway *.o -lmodbus -lpthread -lm
```

## Performance

- **Binary Size**: ~50-100KB (stripped)
- **Memory Usage**: ~2-5MB at runtime
- **Polling Rate**: Configurable (default 1000ms)
- **Max Clients**: Configurable (default 10)

## Troubleshooting

### "zig: command not found"
```bash
bash setup-wsl.sh
```

### "libmodbus.so not found"
```bash
sudo apt-get install libmodbus-dev
```

### Build fails on ARM64 target
```bash
# Verify Zig supports target
zig targets | grep aarch64

# Check dependencies on target device
ssh user@device 'ldd /opt/gateway/gateway'
```

### Connection refused on port 2404
```bash
# Check if firewall is blocking
sudo ufw allow 2404/tcp

# Or disable firewall (development only)
sudo ufw disable
```

## Advanced Topics

### Cross-Compile for Different Targets

```bash
# ARMv7 (32-bit ARM)
zig cc -target arm-linux-gnueabihf -O2 -c ../src/gateway.c

# ARM64 (MUSL libc)
zig cc -target aarch64-linux-musl -O2 -c ../src/gateway.c

# x86_64 Windows (MinGW)
zig cc -target x86_64-windows-gnu -O2 -c ../src/gateway.c
```

### Static Linking

```bash
zig cc -target aarch64-linux-gnu -O2 -static -o gateway *.o -lmodbus -lpthread -lm
```

### Strip Binary for Size

```bash
aarch64-linux-gnu-strip build-arm64/gateway
```

## Supported Targets (via Zig)

- `aarch64-linux-gnu` (ARM64 with glibc) ✓
- `aarch64-linux-musl` (ARM64 with musl)
- `arm-linux-gnueabihf` (ARMv7 32-bit)
- `x86_64-linux-gnu` (x86_64 Linux)
- `x86_64-windows-gnu` (Windows MinGW)
- `riscv64-linux-gnu` (RISC-V)

## License

MIT

## Support

For issues or questions, create an issue on GitHub.
