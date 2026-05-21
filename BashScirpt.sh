#!/bin/bash

set -e

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
BIN_PATH="$PROJECT_ROOT/bin/lds"
DEVICE="/dev/nbd0"
MOUNT_DIR="/mnt/lds_test"
APP_PID=""

cleanup()
{
    set +e

    if mountpoint -q "$MOUNT_DIR"; then
        sudo umount "$MOUNT_DIR"
    fi

    if [ -n "$APP_PID" ]; then
        sudo kill -INT "$APP_PID" 2>/dev/null || true
        wait "$APP_PID" 2>/dev/null || true
    fi

    sudo rm -rf "$MOUNT_DIR"
    rm -f /tmp/lds_expected.txt /tmp/lds_actual.txt
}

trap cleanup EXIT

echo "[1/8] Building project..."
make -C "$PROJECT_ROOT"

echo "[2/8] Loading NBD kernel module..."
sudo modprobe nbd

echo "[3/8] Starting LDS app..."
sudo "$BIN_PATH" "$DEVICE" 134217728 &
APP_PID=$!

sleep 2

echo "[4/8] Creating ext4 filesystem..."
sudo mkfs.ext4 -F -E nodiscard "$DEVICE"
echo "[5/8] Mounting device..."
sudo mkdir -p "$MOUNT_DIR"
sudo mount "$DEVICE" "$MOUNT_DIR"

echo "[6/8] Writing test file..."
echo "hello from LDS e2e test" > /tmp/lds_expected.txt
sudo cp /tmp/lds_expected.txt "$MOUNT_DIR/test.txt"

echo "[7/8] Reading back and comparing..."
sudo cat "$MOUNT_DIR/test.txt" > /tmp/lds_actual.txt
diff /tmp/lds_expected.txt /tmp/lds_actual.txt

echo "[8/8] Unmounting..."
sudo umount "$MOUNT_DIR"

echo "test passed successfully."