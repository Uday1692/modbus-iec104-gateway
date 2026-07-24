#!/bin/bash
# Deploy script - copies binary to ARM64 device

set -e

if [ $# -lt 2 ]; then
    echo "Usage: $0 <device-user> <device-ip> [device-port]"
    echo "Example: $0 root 192.168.1.100 22"
    exit 1
fi

DEVICE_USER=$1
DEVICE_IP=$2
DEVICE_PORT=${3:-22}

echo "========================================"
echo "Deploying to ARM64 Device"
echo "========================================"
echo "User: $DEVICE_USER"
echo "IP: $DEVICE_IP"
echo "Port: $DEVICE_PORT"
echo ""

# Check if binary exists
if [ ! -f "build-arm64/gateway" ]; then
    echo "ERROR: Binary not found at build-arm64/gateway"
    echo "Please build first: bash build-arm64.sh"
    exit 1
fi

echo "[1/3] Creating remote directories..."
ssh -p $DEVICE_PORT $DEVICE_USER@$DEVICE_IP 'mkdir -p /opt/gateway /etc/gateway'

echo "[2/3] Uploading binary..."
scp -P $DEVICE_PORT build-arm64/gateway $DEVICE_USER@$DEVICE_IP:/opt/gateway/
scp -P $DEVICE_PORT config/gateway.conf.example $DEVICE_USER@$DEVICE_IP:/etc/gateway/gateway.conf

echo "[3/3] Setting permissions..."
ssh -p $DEVICE_PORT $DEVICE_USER@$DEVICE_IP 'chmod +x /opt/gateway/gateway'

echo ""
echo "========================================"
echo "✓ Deployment Complete!"
echo "========================================"
echo ""
echo "To run on device:"
echo "  ssh -p $DEVICE_PORT $DEVICE_USER@$DEVICE_IP"
echo "  /opt/gateway/gateway -c /etc/gateway/gateway.conf"
echo ""
