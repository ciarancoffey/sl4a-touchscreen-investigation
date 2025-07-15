#!/bin/bash
# Script to compile and apply ACPI SSDT override for Surface Laptop 4 AMD touchscreen

set -e

echo "=== Surface Laptop 4 AMD Touchscreen ACPI Fix ==="
echo

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo "Please run as root (use sudo)"
    exit 1
fi

# Check for iasl compiler
if ! command -v iasl &> /dev/null; then
    echo "Error: iasl not found. Install with: sudo apt-get install acpica-tools"
    exit 1
fi

# Compile the SSDT
echo "1. Compiling SSDT override..."
iasl -tc ssdt-spi-fix.asl

if [ ! -f "ssdt-spi-fix.aml" ]; then
    echo "Error: Failed to compile SSDT"
    exit 1
fi

echo "   ✓ Compiled successfully"
echo

# Create initrd directory structure
echo "2. Setting up initrd override..."
mkdir -p kernel/firmware/acpi
cp ssdt-spi-fix.aml kernel/firmware/acpi/

# Create cpio archive
echo "3. Creating initrd image..."
find kernel | cpio -H newc --create > acpi_override.img

echo "   ✓ Created acpi_override.img"
echo

# Backup current initrd
echo "4. Backing up current initrd..."
cp /boot/initrd.img-$(uname -r) /boot/initrd.img-$(uname -r).backup

# Combine with existing initrd
echo "5. Combining with existing initrd..."
cat acpi_override.img /boot/initrd.img-$(uname -r).backup > /boot/initrd.img-$(uname -r)

echo "   ✓ Updated initrd"
echo

# Update GRUB
echo "6. Updating bootloader..."
update-grub

echo
echo "=== ACPI Override Applied ==="
echo
echo "Next steps:"
echo "1. Reboot your system"
echo "2. Check if SPI controller appears:"
echo "   ls /sys/class/spi_master/"
echo "   dmesg | grep -i 'spi\|amdi\|mshw'"
echo "3. Check touchscreen enumeration:"
echo "   ls /sys/bus/spi/devices/"
echo
echo "To revert changes:"
echo "   sudo cp /boot/initrd.img-$(uname -r).backup /boot/initrd.img-$(uname -r)"
echo "   sudo update-grub"
echo

# Cleanup
rm -rf kernel/
rm -f acpi_override.img