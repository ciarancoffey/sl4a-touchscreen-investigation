#!/bin/bash
# Verification script for Surface Laptop 4 AMD touchscreen

echo "=== Surface Laptop 4 AMD Touchscreen Status ==="
echo
echo "Date: $(date)"
echo "Kernel: $(uname -r)"
echo

# Check for SPI controllers
echo "1. SPI Controllers:"
echo "-------------------"
if [ -d /sys/class/spi_master ]; then
    ls -la /sys/class/spi_master/ 2>/dev/null || echo "   No SPI controllers found"
else
    echo "   /sys/class/spi_master directory does not exist"
fi
echo

# Check for AMDI devices
echo "2. AMD Controller Devices:"
echo "--------------------------"
echo "   I2C Controllers (AMDI0010 - incorrect for touchscreen):"
ls -la /sys/bus/platform/devices/ | grep -i amdi0010 || echo "      None found"
echo
echo "   SPI Controllers (AMDI0061/2/3 - needed for touchscreen):"
ls -la /sys/bus/platform/devices/ | grep -E "amdi006[1-3]" || echo "      None found"
echo

# Check for MSHW0231 device
echo "3. Touchscreen Device (MSHW0231):"
echo "---------------------------------"
find /sys -name "*MSHW0231*" 2>/dev/null | head -10 || echo "   Not found in sysfs"
echo

# Check ACPI devices
echo "4. ACPI Devices:"
echo "----------------"
ls -la /sys/bus/acpi/devices/ | grep -E "MSHW0231|AMDI006" || echo "   No relevant devices found"
echo

# Check SPI devices
echo "5. SPI Bus Devices:"
echo "-------------------"
if [ -d /sys/bus/spi/devices ]; then
    ls -la /sys/bus/spi/devices/ || echo "   No SPI devices found"
else
    echo "   SPI bus not available"
fi
echo

# Check loaded modules
echo "6. Relevant Kernel Modules:"
echo "---------------------------"
echo "   SPI modules:"
lsmod | grep -E "spi_|spi-" | grep -v spi_nor || echo "      No SPI modules loaded"
echo
echo "   HID modules:"
lsmod | grep -E "hid_|i2c_hid|spi_hid" || echo "      No relevant HID modules loaded"
echo

# Check dmesg for errors
echo "7. Recent Driver Messages:"
echo "--------------------------"
dmesg | tail -100 | grep -E -i "spi|hid|mshw|amdi|touchscreen|touch" | tail -20 || echo "   No relevant messages"
echo

# Summary
echo "8. Summary:"
echo "-----------"
if ls /sys/class/spi_master/spi* &>/dev/null; then
    echo "   ✓ SPI controllers present"
else
    echo "   ✗ No SPI controllers found (this is the problem)"
fi

if find /sys -name "*MSHW0231*" &>/dev/null; then
    echo "   ✓ MSHW0231 device detected"
else
    echo "   ✗ MSHW0231 device not found"
fi

if lsmod | grep -q spi_hid; then
    echo "   ✓ spi-hid driver loaded"
else
    echo "   ✗ spi-hid driver not loaded"
fi

echo
echo "Expected after ACPI fix:"
echo "- AMDI0061 SPI controller should appear"
echo "- /sys/class/spi_master/spi1 should exist"
echo "- MSHW0231 should appear under /sys/bus/spi/devices/"
echo "- spi-hid driver should bind to the device"