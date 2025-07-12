# AMD SPI Controller Investigation Results

## Critical Discovery

Our investigation reveals why MSHW0231 touchscreen doesn't work and provides a clear path forward.

## The Problem: Wrong ACPI Device ID

### What We Found:
1. **AMDI0010** = AMD I2C Controller (what we have)
2. **AMDI0061/0062/0063** = AMD SPI Controllers (what we need)

### Current State:
```bash
# What's actually enumerated:
/sys/bus/platform/devices/AMDI0010:00 -> i2c_designware driver
/sys/bus/platform/devices/AMDI0010:01 -> i2c_designware driver  
/sys/bus/platform/devices/AMDI0010:02 -> i2c_designware driver

# What we need but don't have:
AMDI0061/0062/0063 -> spi-amd driver -> SPI controller -> MSHW0231
```

## Linux Kernel SPI Driver Status

The `spi-amd.c` driver exists and supports:
- **AMDI0061** (AMD_SPI_V1)
- **AMDI0062** (AMD_SPI_V2)  
- **AMDI0063** (AMD_HID2_SPI)

But Surface Laptop 4 AMD has **AMDI0010** devices, not these SPI controller IDs.

## Why This Matters

### Windows Configuration:
- Windows somehow configures MSHW0231 on "SPI bus 11"
- Uses `hidspi.sys` driver successfully
- Device works perfectly

### Linux Problem:
- No AMDI0061/0062/0063 devices exist in ACPI
- Only AMDI0010 (I2C) devices available
- MSHW0231 has no SPI controller to bind to

## Root Cause Analysis

This suggests one of these scenarios:

### Scenario 1: ACPI Firmware Bug
Surface Laptop 4 AMD firmware incorrectly declares:
- AMDI0010 (I2C) instead of AMDI0061/0062/0063 (SPI)
- MSHW0231 under wrong parent device
- Missing SPI controller declarations

### Scenario 2: Dual-Function Controller
AMDI0010 might be a dual-function controller that can operate as either:
- I2C controller (default Linux configuration)
- SPI controller (when properly configured)

### Scenario 3: Missing ACPI Configuration
The SPI controllers exist but aren't properly enumerated due to:
- Missing ACPI table entries
- Incorrect device dependencies
- Firmware initialization issues

## Investigation Evidence

### From Internet Research:
1. **AMDI0010 = I2C only** - All documentation shows this as I2C Designware controller
2. **AMD SPI uses different IDs** - spi-amd.c driver expects AMDI0061/0062/0063
3. **Surface devices are special** - Microsoft uses custom ACPI tables

### From Our System:
```bash
# No SPI controllers found
$ ls /sys/class/spi_master/
(empty)

# No AMD SPI device IDs present
$ grep -r AMDI006 /sys/firmware/acpi/tables/
(no results)

# Only I2C controllers available
$ dmesg | grep AMDI0010
i2c_designware AMDI0010:00: I2C bus managed by ACPI
```

## Potential Solutions

### Solution 1: ACPI Override
Create ACPI table override that:
1. Adds proper AMD SPI controller (AMDI0061/0062/0063)
2. Moves MSHW0231 under SPI controller
3. Configures correct resources (IRQ 4228, GPIO 2308)

### Solution 2: Force Controller Mode
Develop driver that:
1. Unbinds AMDI0010 from i2c_designware
2. Reconfigures hardware as SPI controller
3. Manually creates SPI devices

### Solution 3: Hybrid Approach
Use existing AMDI0010 but:
1. Create virtual SPI controller
2. Bridge I2C<->SPI communication
3. Translate SPI HID protocols

## Next Steps

### Immediate Testing:
1. **Check for hidden SPI controllers**:
   ```bash
   sudo cat /sys/firmware/acpi/tables/DSDT > dsdt.dat
   iasl -d dsdt.dat
   grep -i "AMDI006" dsdt.dsl
   ```

2. **Verify Windows configuration**:
   - Boot Windows
   - Check Device Manager for SPI controllers
   - Look for AMDI0061/0062/0063 devices

### Development Options:
1. **ACPI table patching** - Most promising for permanent fix
2. **Custom controller driver** - Requires deep hardware knowledge  
3. **Community collaboration** - Work with linux-surface on ACPI fixes

## Conclusion

The touchscreen issue isn't a driver problem - it's an **ACPI enumeration problem**. The Surface Laptop 4 AMD either:
- Has incorrect ACPI tables that enumerate I2C instead of SPI controllers
- Needs special configuration to enable SPI mode
- Requires custom ACPI overrides to work properly

This explains why "nobody was able to make [SPI-HID] work yet" in the linux-surface community - they have the right driver but the wrong hardware enumeration.