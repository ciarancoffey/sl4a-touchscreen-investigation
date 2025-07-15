#!/bin/bash
# Manual binding script for Surface Laptop 4 AMD touchscreen
# This works around the missing AMDI0060 support in spi-amd

set -e

echo "=== Surface Laptop 4 AMD Touchscreen Manual Fix ==="
echo

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo "Please run as root (use sudo)"
    exit 1
fi

echo "Current situation:"
echo "- AMDI0060:00 exists as SPI1 controller (ACPI)"
echo "- MSHW0231:00 exists under AMDI0060:00 (touchscreen)"
echo "- spi-amd driver doesn't recognize AMDI0060"
echo

# Load necessary modules
echo "1. Loading SPI modules..."
modprobe spi-amd || true
modprobe spi-hid || true

# Check if we can add the alias dynamically
echo
echo "2. Adding AMDI0060 to spi-amd supported devices..."

# Create a new_id entry if the driver supports it
if [ -f /sys/bus/platform/drivers/spi-amd/new_id ]; then
    echo "AMDI0060" > /sys/bus/platform/drivers/spi-amd/new_id 2>/dev/null || true
fi

# Try manual binding
echo
echo "3. Attempting manual binding..."
if [ -d /sys/bus/platform/drivers/spi-amd ]; then
    if [ -e /sys/bus/acpi/devices/AMDI0060:00 ]; then
        # Get the platform device name
        PLATFORM_DEV=$(ls -la /sys/bus/acpi/devices/AMDI0060:00/physical_node* 2>/dev/null | awk -F'/' '{print $NF}' | head -1)
        
        if [ -n "$PLATFORM_DEV" ]; then
            echo "   Found platform device: $PLATFORM_DEV"
            echo "$PLATFORM_DEV" > /sys/bus/platform/drivers/spi-amd/bind 2>/dev/null || true
        else
            echo "   No platform device found for AMDI0060:00"
        fi
    fi
fi

# Check results
echo
echo "4. Checking results..."
echo "   SPI masters:"
ls -la /sys/class/spi_master/ 2>/dev/null || echo "      None found"

echo
echo "   SPI devices:"
ls -la /sys/bus/spi/devices/ 2>/dev/null || echo "      None found"

echo
echo "   Loaded modules:"
lsmod | grep -E "spi_|spi-" | grep -v spi_nor || echo "      No SPI modules loaded"

echo
echo "5. Alternative approach - override ACPI ID:"
echo "   Since spi-amd doesn't support AMDI0060, we need to either:"
echo "   a) Patch the spi-amd driver to add AMDI0060 support"
echo "   b) Create an SSDT that changes AMDI0060 to AMDI0061"
echo "   c) Use a kernel parameter to override the ACPI ID"
echo
echo "The most reliable solution is to patch the kernel driver."