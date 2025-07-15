#!/bin/bash
# Add AMDI0060 support to spi-amd driver

set -e

echo "=== Adding AMDI0060 support to spi-amd driver ==="
echo

# Create a modprobe configuration to alias AMDI0060 to spi-amd
echo "Creating modprobe alias..."
sudo tee /etc/modprobe.d/surface-spi.conf << EOF
# Surface Laptop 4 AMD uses AMDI0060 for SPI controller
# This aliases it to load spi-amd driver
alias acpi:AMDI0060:* spi-amd
EOF

# Try to bind the existing device
echo
echo "Attempting to bind AMDI0060 to spi-amd driver..."

# First, check if spi-amd has a bind interface
if [ -d /sys/bus/platform/drivers/spi-amd ]; then
    echo "AMDI0060:00" | sudo tee /sys/bus/platform/drivers/spi-amd/bind || true
else
    echo "spi-amd driver not found in platform drivers"
fi

# Alternative: Create udev rule
echo
echo "Creating udev rule for automatic binding..."
sudo tee /etc/udev/rules.d/99-surface-spi.rules << 'EOF'
# Surface Laptop 4 AMD SPI controller
ACTION=="add", SUBSYSTEM=="acpi", KERNEL=="AMDI0060:*", RUN+="/sbin/modprobe spi-amd"
EOF

echo
echo "Reloading udev rules..."
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=acpi

echo
echo "Done! The spi-amd driver should now recognize AMDI0060."
echo "You may need to reboot for the changes to take full effect."