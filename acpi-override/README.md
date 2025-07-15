# ACPI Override Solution for Surface Laptop 4 AMD Touchscreen

## Overview

This directory contains an ACPI SSDT override that fixes the touchscreen on Surface Laptop 4 AMD by adding the missing SPI controller that Windows uses but Linux doesn't enumerate.

## The Problem

- **Device**: MSHW0231 touchscreen is an SPI HID device
- **Windows**: Uses AMDI0061 SPI controller on bus 11
- **Linux**: Only enumerates AMDI0010 as I2C controllers
- **Result**: No SPI bus available for touchscreen to bind to

## The Solution

Our SSDT override:
1. Adds AMDI0061 SPI controller device
2. Creates TCH1 device for MSHW0231 touchscreen
3. Configures proper resources (IRQ 4228, GPIO 2308)
4. Sets up SPI bus connection parameters

## Files

- `ssdt-spi-fix.asl` - ACPI source code for the override
- `apply-acpi-override.sh` - Script to compile and install the override
- `verify-touchscreen.sh` - Script to check touchscreen status
- `README.md` - This documentation

## Installation

### Prerequisites

```bash
# Install ACPI tools
sudo apt-get install acpica-tools

# Ensure spi-hid driver is available
sudo modprobe spi-hid
```

### Apply the Override

```bash
# Run as root
sudo ./apply-acpi-override.sh

# Reboot
sudo reboot
```

### Verify

After reboot:

```bash
# Check current status
./verify-touchscreen.sh

# Expected output:
# - SPI controller at /sys/class/spi_master/spi1
# - MSHW0231 device under /sys/bus/spi/devices/
# - spi-hid driver loaded and bound
```

## How It Works

### 1. SSDT Structure

```asl
Device (SPI1)  // AMD SPI Controller
{
    _HID = "AMDI0061"
    _CRS = Memory + IRQ resources
}

Device (TCH1)  // Touchscreen
{
    _HID = "MSHW0231"
    _CRS = SPI bus + GPIO interrupt + GPIO reset
    _DEP = Dependencies on GPIO and SPI1
}
```

### 2. Resource Configuration

Based on Windows analysis:
- **SPI Bus**: 11 (0x0B)
- **SPI Speed**: 8MHz
- **Interrupt**: GPIO 132 (IRQ 4228)
- **Reset**: GPIO 2308
- **HID Descriptor**: Address 0x20

### 3. Boot Process

1. UEFI loads our SSDT via initrd
2. Linux ACPI parser creates platform devices
3. spi-amd driver binds to AMDI0061
4. SPI bus enumeration creates spi1-0 device
5. spi-hid driver binds to MSHW0231
6. Touchscreen becomes functional

## Troubleshooting

### No SPI Controller After Reboot

```bash
# Check if SSDT was loaded
sudo dmesg | grep "ACPI:.*SSDT"

# Verify override in initrd
sudo lsinitrd | grep acpi_override
```

### SPI Controller Present but No Touchscreen

```bash
# Check SPI devices
ls -la /sys/bus/spi/devices/

# Check for binding errors
dmesg | grep -E "spi|hid|MSHW"

# Manually bind driver
echo "spi1.0" > /sys/bus/spi/drivers/spi-hid/bind
```

### Driver Not Loading

```bash
# Load required modules
sudo modprobe spi-amd
sudo modprobe spi-hid

# Check module parameters
systool -vm spi-hid
```

## Reverting Changes

To remove the ACPI override:

```bash
# Restore original initrd
sudo cp /boot/initrd.img-$(uname -r).backup /boot/initrd.img-$(uname -r)

# Update bootloader
sudo update-grub

# Reboot
sudo reboot
```

## Technical Details

### Why This Works

1. **Windows Behavior**: Windows has proper ACPI entries for SPI controllers
2. **Linux Issue**: Linux ACPI tables missing SPI controller definitions
3. **Our Fix**: Add missing ACPI device definitions via SSDT overlay

### ACPI Hierarchy

```
_SB (System Bus)
├── GPIO (GPIO Controller) 
├── SPI1 (AMD SPI Controller) [Added by override]
│   └── spi1-0 (Created by SPI bus enumeration)
└── TCH1 (Touchscreen Device) [Added by override]
```

### Driver Stack

```
Hardware: MSHW0231 Touchscreen
    ↓
ACPI: TCH1 device with SPI resources  
    ↓
Platform: AMDI0061 SPI controller
    ↓
spi-amd: Creates /sys/class/spi_master/spi1
    ↓
SPI Core: Enumerates devices from ACPI
    ↓
spi-hid: Binds to MSHW0231 on spi1-0
    ↓
HID Core: Creates input device
    ↓
Input: /dev/input/eventX touchscreen
```

## Next Steps

1. **Test** on multiple Surface Laptop 4 AMD devices
2. **Refine** GPIO and interrupt configurations if needed
3. **Submit** to linux-surface project for inclusion
4. **Upstream** ACPI quirk to mainline kernel

## Contributing

Please test and report results:
- Device model and BIOS version
- Kernel version
- Success/failure details
- Any error messages

## Credits

- Investigation by: [Your name]
- Based on Windows analysis and SPI HID specifications
- Thanks to linux-surface community for driver work