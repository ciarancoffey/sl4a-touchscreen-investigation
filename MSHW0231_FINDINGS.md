# Surface Laptop 4 AMD Touchscreen (MSHW0231) Technical Analysis

## Executive Summary

This document presents findings from a deep technical investigation of the MSHW0231 touchscreen device on Surface Laptop 4 AMD. The key discovery is that **MSHW0231 is an SPI HID device**, not I2C as previously attempted. The touchscreen remains non-functional in Linux due to fundamental SPI controller enumeration issues.

## Device Information

- **Device**: Microsoft Surface Laptop 4 AMD (Ryzen 7)
- **Touchscreen HID**: `ACPI\MSHW0231`
- **Compatible ID**: `ACPI\PNP0C51` (Generic SPI HID Device)
- **Linux Kernel**: 6.15.3-arch1-2-surface
- **Status**: Non-functional

## Key Findings

### 1. Device is SPI HID, Not I2C

Windows analysis revealed critical information:
- **Bus Type**: SPI (Serial Peripheral Interface)
- **SPI Controller**: Bus 11 (0x0B)
- **IRQ**: 4228 (0x1084)
- **GPIO Pin**: 2308 (0x0904) for reset/power control
- **Windows Driver**: `hidspi.sys` (Microsoft SPI HID Miniport Driver)

Previous Linux attempts failed because they assumed I2C communication.

### 2. Multi-Collection HID Device

MSHW0231 exposes 8 HID collections in a single device:
```
Collection 01: Surface Touch Communications
Collection 02: Surface Touch Pen Processor (HEAT)
Collection 03: Surface Digitizer Utility
Collection 04: Surface Virtual Function Enum Device
Collection 05: HID-compliant pen (Surface Touch Pen Device)
Collection 06: HID-compliant touch screen ← Primary touchscreen
Collection 07: Surface Pen BLE LC Adaptation Driver
Collection 08: Surface Pen Cfu Over Ble LC Connection
```

### 3. Linux SPI Controller Enumeration Problem

The fundamental issue preventing touchscreen functionality:

1. **AMDI0010 devices** should provide SPI controllers
2. In Linux, they're incorrectly bound to **I2C Designware driver**
3. Result: No SPI controllers available (`/sys/class/spi_master/` is empty)
4. MSHW0231 exists under wrong ACPI parent (AMDI0060 instead of SPI controller)

```bash
# Current incorrect state:
/sys/bus/platform/devices/AMDI0010:00 -> i2c_designware driver
/sys/bus/platform/devices/AMDI0010:01 -> i2c_designware driver
/sys/bus/platform/devices/AMDI0010:02 -> i2c_designware driver

# MSHW0231 location:
/sys/bus/acpi/devices/MSHW0231:00 (under AMDI0060:00, not SPI controller)
```

### 4. SPI HID Driver Status

- The linux-surface/spi-hid driver **compiles and loads successfully**
- Module cannot bind to MSHW0231 without SPI controller enumeration
- Driver includes MSHW0231 in ACPI match table (verified)

## Technical Details from Windows Analysis

### Power Management Configuration
```
DeviceResetNotificationEnabled = 1
EnhancedPowerManagementEnabled = 1
SelectiveSuspendEnabled = 1
SelectiveSuspendTimeout = 2000ms
D3ColdSupported = 1
```

### Device Resources (from Windows)
- SPI bus connection to controller 11
- GPIO interrupt on pin 2308
- IRQ 4228 for touch events

## Attempted Solutions

1. **Removed incorrect I2C HID patches** - Patch 0007 was for I2C, not SPI
2. **Built and loaded SPI HID module** - Loads but cannot bind
3. **Attempted kernel patches** - Complex SPI enumeration patches failed to apply
4. **Manual device creation** - Not possible without SPI controllers

## Root Cause Analysis

The touchscreen cannot function because:

1. AMDI0010 controllers are multi-function devices that can operate as either I2C or SPI
2. Linux incorrectly configures them as I2C controllers
3. Without SPI controllers, MSHW0231 cannot be enumerated as an SPI device
4. The device appears under the wrong ACPI parent in the device tree

## Proposed Solutions

### Short-term Workaround (Theoretical)
1. Unbind AMDI0010 devices from i2c_designware driver
2. Create minimal AMD SPI controller driver for AMDI0010
3. Force SPI mode for the controller serving bus 11
4. Enable ACPI-to-SPI device enumeration

### Long-term Fix
1. Modify AMDI0010 detection to identify when SPI mode is needed
2. Add proper ACPI parsing to detect child SPI devices
3. Implement controller mode switching (I2C vs SPI)
4. Submit patches upstream for proper AMD platform support

## Reproducible Test Environment

```bash
# Check current state
ls /sys/class/spi_master/  # Empty - no SPI controllers
ls /sys/bus/acpi/devices/MSHW0231:00/  # Device exists
cat /sys/bus/acpi/devices/MSHW0231:00/path  # Shows \_SB_.SPI1.HSPI
lsmod | grep spi_hid  # Module loaded but inactive

# The SPI HID driver is ready but has no SPI bus to bind to
```

## Community Collaboration Opportunities

1. **AMDI0010 SPI driver development** - Critical missing piece
2. **ACPI table analysis** - Understanding controller configuration
3. **Testing on other AMD Surface models** - Verify same issue pattern
4. **Windows driver reverse engineering** - How does Windows configure AMDI0010?

## Attachments Available

- Full Windows device analysis (`windows_summary_package.md`)
- ProcMon boot traces showing device initialization
- Modified spi-hid driver with MSHW0231 support
- Kernel patches attempted (for reference)

---

This investigation confirms the community observation that "nobody was able to make [SPI-HID] work yet" and provides specific technical reasons why. The path forward requires solving the AMDI0010 SPI controller enumeration issue before any touchscreen driver can function.