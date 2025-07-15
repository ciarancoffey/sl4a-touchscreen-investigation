#!/bin/bash
# Install patched spi-amd driver with AMDI0060 support

set -e

echo "=== Installing Patched spi-amd Driver ==="
echo

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo "Please run as root (use sudo)"
    exit 1
fi

# Backup original module
echo "1. Backing up original spi-amd module..."
ORIG_MODULE="/lib/modules/$(uname -r)/kernel/drivers/spi/spi-amd.ko.zst"
if [ -f "$ORIG_MODULE" ]; then
    cp "$ORIG_MODULE" "$ORIG_MODULE.backup"
    echo "   ✓ Backed up to $ORIG_MODULE.backup"
fi

# Build the module
echo
echo "2. Building patched module..."
make clean
make

# Unload existing module if loaded
echo
echo "3. Unloading existing spi-amd module..."
rmmod spi-amd 2>/dev/null || true

# Install new module
echo
echo "4. Installing patched module..."
# Compress the module
zstd -f spi-amd.ko -o spi-amd.ko.zst

# Copy to kernel modules directory
cp spi-amd.ko.zst "$ORIG_MODULE"

# Update module dependencies
echo
echo "5. Updating module dependencies..."
depmod -a

# Load the new module
echo
echo "6. Loading patched module..."
modprobe spi-amd

# Check if it worked
echo
echo "7. Verifying installation..."
echo "   Module info:"
modinfo spi-amd | grep -E "filename|alias.*AMDI0060"

echo
echo "   SPI controllers:"
ls -la /sys/class/spi_master/ 2>/dev/null || echo "      No SPI controllers found yet"

echo
echo "   Checking if AMDI0060 is bound:"
if [ -e /sys/devices/platform/AMDI0060:00/driver ]; then
    echo "      ✓ AMDI0060:00 is bound to: $(readlink /sys/devices/platform/AMDI0060:00/driver | xargs basename)"
else
    echo "      ✗ AMDI0060:00 not bound to any driver"
fi

echo
echo "=== Installation Complete ==="
echo
echo "Next steps:"
echo "1. Check for SPI devices: ls /sys/bus/spi/devices/"
echo "2. Load spi-hid driver: sudo modprobe spi-hid"
echo "3. Monitor dmesg: dmesg -w"
echo
echo "To revert:"
echo "   sudo cp $ORIG_MODULE.backup $ORIG_MODULE"
echo "   sudo depmod -a"
echo "   sudo rmmod spi-amd && sudo modprobe spi-amd"