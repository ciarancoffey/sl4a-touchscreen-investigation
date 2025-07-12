# Modified SPI HID Driver

## Overview

This directory contains the modified linux-surface/spi-hid driver with MSHW0231 support added.

## Changes Made

### Added MSHW0231 Support

**File**: `spi-hid-core.c`  
**Line**: ~1520 (in `spi_hid_acpi_match` table)

```c
static const struct acpi_device_id spi_hid_acpi_match[] = {
    { "MSHW0134", 0 },  /* Surface Pro X (SQ1) */
    { "MSHW0162", 0 },  /* Surface Laptop 3 (AMD) */
    { "MSHW0231", 0 },  /* Surface Laptop 4 (AMD) */ ← ADDED
    { "MSHW0235", 0 },  /* Surface Pro X (SQ2) */
    { "PNP0C51",  0 },  /* Generic HID-over-SPI */
    {},
};
```

### Compatibility Fixes

Also fixed for kernel 6.15 compatibility:
- `strlcpy` → `strscpy` (deprecated function)
- `chip_select` → `chip_select[0]` (array access fix)

## Build Status

✅ **Compiles successfully** on kernel 6.15.3-arch1-2-surface  
✅ **Loads without errors** (`insmod spi-hid.ko`)  
❌ **Cannot bind to MSHW0231** - No SPI controllers available

## Test Results

```bash
# Module loads successfully
$ sudo insmod spi-hid.ko
$ lsmod | grep spi_hid
spi_hid                86016  0

# But cannot find SPI controllers to bind to
$ ls /sys/class/spi_master/
(empty - this is the problem)
```

## Original Source

- **Repository**: https://github.com/linux-surface/spi-hid
- **Commit**: d07569c (debug stuff)
- **Author**: Maximilian Luz <luzmaximilian@gmail.com>
- **License**: GPL

## Usage

This driver is ready to use once SPI controllers are properly enumerated. The blocking issue is that Surface Laptop 4 AMD has AMDI0010 (I2C) controllers instead of AMDI0061/0062/0063 (SPI) controllers.

Once ACPI enumeration is fixed, this driver should automatically bind to MSHW0231 and enable touchscreen functionality.