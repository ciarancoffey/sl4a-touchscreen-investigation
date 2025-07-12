#!/bin/bash
# Surface Laptop 4 AMD Touchscreen Investigation Commands
# Created: July 12, 2025
# Purpose: Reproducible investigation and verification commands

echo "==============================================="
echo "Surface Laptop 4 AMD Touchscreen Investigation"
echo "==============================================="

# System Information
echo -e "\n🖥️  SYSTEM INFORMATION"
echo "Kernel: $(uname -r)"
echo "Hardware: $(dmidecode -s system-product-name 2>/dev/null || echo 'N/A')"
echo "Date: $(date)"

# 1. SPI Controller Check
echo -e "\n🔍 1. SPI CONTROLLER ENUMERATION"
echo "=== SPI Masters ==="
ls -la /sys/class/spi_master/ 2>/dev/null || echo "❌ No SPI masters found"

echo -e "\n=== SPI Devices ==="
ls -la /sys/bus/spi/devices/ 2>/dev/null || echo "❌ No SPI devices found"

# 2. AMD Controller Analysis  
echo -e "\n🔧 2. AMD CONTROLLER ANALYSIS"
echo "=== AMDI0010 Devices (Current I2C Controllers) ==="
for dev in /sys/bus/platform/devices/AMDI0010:*; do
    if [ -d "$dev" ]; then
        echo "Device: $(basename $dev)"
        echo "  Driver: $(readlink $dev/driver 2>/dev/null | xargs basename || echo 'No driver')"
        echo "  Modalias: $(cat $dev/modalias 2>/dev/null || echo 'N/A')"
        echo "  ---"
    fi
done

echo -e "\n=== Looking for AMD SPI Controllers (AMDI0061/0062/0063) ==="
for id in AMDI0061 AMDI0062 AMDI0063; do
    if find /sys -name "*${id}*" 2>/dev/null | grep -q .; then
        echo "✅ Found $id:"
        find /sys -name "*${id}*" 2>/dev/null
    else
        echo "❌ Missing $id"
    fi
done

# 3. MSHW0231 Device Status
echo -e "\n📱 3. MSHW0231 DEVICE STATUS"
if [ -d /sys/bus/acpi/devices/MSHW0231:00 ]; then
    echo "✅ MSHW0231 device found"
    echo "  Status: $(cat /sys/bus/acpi/devices/MSHW0231:00/status 2>/dev/null || echo 'N/A')"
    echo "  HID: $(cat /sys/bus/acpi/devices/MSHW0231:00/hid 2>/dev/null || echo 'N/A')"
    echo "  Modalias: $(cat /sys/bus/acpi/devices/MSHW0231:00/modalias 2>/dev/null || echo 'N/A')"
    echo "  Path: $(cat /sys/bus/acpi/devices/MSHW0231:00/path 2>/dev/null || echo 'N/A')"
    
    if [ -L /sys/bus/acpi/devices/MSHW0231:00/driver ]; then
        echo "  Driver: $(readlink /sys/bus/acpi/devices/MSHW0231:00/driver | xargs basename)"
    else
        echo "  Driver: ❌ No driver bound"
    fi
else
    echo "❌ MSHW0231 device not found"
fi

# 4. Driver Module Status
echo -e "\n🔌 4. DRIVER MODULE STATUS"
echo "=== SPI HID Module ==="
if lsmod | grep -q spi_hid; then
    echo "✅ spi_hid module loaded"
    lsmod | grep spi_hid
else
    echo "❌ spi_hid module not loaded"
fi

echo -e "\n=== AMD SPI Driver ==="
if lsmod | grep -q spi_amd; then
    echo "✅ spi_amd module loaded"
    lsmod | grep spi_amd
else
    echo "❌ spi_amd module not loaded"
fi

echo -e "\n=== I2C Designware Driver ==="
if lsmod | grep -q i2c_designware; then
    echo "✅ i2c_designware module loaded"
    lsmod | grep i2c_designware
else
    echo "❌ i2c_designware module not loaded"
fi

# 5. Input Device Detection
echo -e "\n🖱️  5. INPUT DEVICE DETECTION"
echo "=== Touchscreen Devices ==="
if grep -E -i "touch|Touch|screen" /proc/bus/input/devices 2>/dev/null; then
    echo "✅ Touchscreen-related input devices found"
else
    echo "❌ No touchscreen input devices found"
fi

echo -e "\n=== All HID Devices ==="
grep -E "Name=.*HID|Microsoft Surface" /proc/bus/input/devices 2>/dev/null | head -5 || echo "❌ No relevant HID devices"

# 6. ACPI Analysis
echo -e "\n📋 6. ACPI ANALYSIS"
echo "=== ACPI Device Table Check ==="
if command -v acpidump >/dev/null 2>&1; then
    echo "✅ acpidump available for ACPI analysis"
    echo "Run: sudo acpidump -b && iasl -d *.dat"
else
    echo "❌ acpidump not available (install acpica-tools)"
fi

# 7. Kernel Messages
echo -e "\n📝 7. RELEVANT KERNEL MESSAGES"
echo "=== Recent touchscreen/SPI messages ==="
if dmesg | grep -E -i "mshw0231|spi.*hid|touch.*screen|amdi00" | tail -5; then
    echo "(Showing last 5 relevant messages)"
else
    echo "❌ No relevant kernel messages found"
fi

# 8. Investigation Summary
echo -e "\n📊 8. INVESTIGATION SUMMARY"
echo "=== Key Status ==="

# Check critical components
SPI_MASTERS=$(ls /sys/class/spi_master/ 2>/dev/null | wc -l)
MSHW0231_EXISTS=$([ -d /sys/bus/acpi/devices/MSHW0231:00 ] && echo "YES" || echo "NO")
SPI_HID_LOADED=$(lsmod | grep -q spi_hid && echo "YES" || echo "NO")
AMDI0010_COUNT=$(ls -d /sys/bus/platform/devices/AMDI0010:* 2>/dev/null | wc -l)
TOUCHSCREEN_INPUT=$(grep -i touch /proc/bus/input/devices >/dev/null 2>&1 && echo "YES" || echo "NO")

echo "SPI Controllers: $SPI_MASTERS"
echo "MSHW0231 Device: $MSHW0231_EXISTS"  
echo "SPI HID Driver: $SPI_HID_LOADED"
echo "AMDI0010 (I2C): $AMDI0010_COUNT devices"
echo "Touchscreen Input: $TOUCHSCREEN_INPUT"

echo -e "\n=== Problem Analysis ==="
if [ "$SPI_MASTERS" -eq 0 ] && [ "$MSHW0231_EXISTS" = "YES" ] && [ "$AMDI0010_COUNT" -gt 0 ]; then
    echo "🔍 CONFIRMED: Root cause identified"
    echo "   - MSHW0231 exists but no SPI controllers"
    echo "   - AMDI0010 devices present (I2C controllers)"
    echo "   - Need AMDI0061/0062/0063 for SPI support"
elif [ "$SPI_MASTERS" -gt 0 ] && [ "$TOUCHSCREEN_INPUT" = "NO" ]; then
    echo "🔍 PROGRESS: SPI controllers found, driver issue"
else
    echo "🔍 Status: Further investigation needed"
fi

echo -e "\n=== Next Steps ==="
if [ "$SPI_MASTERS" -eq 0 ]; then
    echo "1. ❗ Critical: Fix SPI controller enumeration"
    echo "   - Check Windows for AMDI0061/0062/0063 devices"
    echo "   - Consider ACPI override to add SPI controller"
    echo "   - Investigate AMDI0010 dual-mode capabilities"
else
    echo "1. ✅ SPI controllers detected, check driver binding"
    echo "   - Verify spi-hid driver can bind to MSHW0231"
    echo "   - Test touchscreen functionality"
fi

echo -e "\n2. 📋 Documentation available:"
echo "   - COMPREHENSIVE_INVESTIGATION_SUMMARY.md"
echo "   - ACTIONABLE_NEXT_STEPS.md"
echo "   - Share with linux-surface community"

echo -e "\n==============================================="
echo "Investigation completed: $(date)"
echo "==============================================="