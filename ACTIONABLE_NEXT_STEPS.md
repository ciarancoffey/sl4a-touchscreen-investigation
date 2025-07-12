# Actionable Next Steps for Surface AMD Touchscreen Fix

## Immediate Actions (Next 1-2 Days)

### 1. Verify Windows Hardware Enumeration
**Goal**: Confirm if Windows has AMDI0061/0062/0063 devices or uses different mechanism

**Commands to run in Windows**:
```powershell
# Check Device Manager for SPI controllers
Get-PnpDevice | Where-Object {$_.DeviceID -like "*AMDI006*"}

# Look for SPI devices specifically  
Get-PnpDevice -Class "SPI" | Format-Table

# Check MSHW0231 current driver
Get-PnpDevice | Where-Object {$_.DeviceID -like "*MSHW0231*"} | Get-PnpDeviceProperty
```

### 2. Advanced ACPI Analysis
**Goal**: Deep dive into ACPI table differences between Windows and Linux

```bash
# Dump all ACPI tables
sudo acpidump -b

# Convert to readable format
for file in *.dat; do
    iasl -d "$file"
done

# Search for SPI-related entries
grep -r -i "spi\|AMDI006\|MSHW0231" *.dsl

# Look for device dependencies
grep -A10 -B5 "_DEP\|_CRS\|SerialBus" *.dsl
```

### 3. Community Engagement
**Goal**: Share findings with linux-surface community

**Actions**:
1. **Post GitHub issue** using `/home/ccoffey/repos/linux-surface/GITHUB_ISSUE_TEMPLATE.md`
2. **Share in Discord/Matrix** using `/home/ccoffey/repos/linux-surface/DISCORD_SUMMARY.md`  
3. **Upload supporting files** (Windows analysis, investigation results)

## Short-term Development (Next 1-2 Weeks)

### 4. ACPI Override Attempt
**Goal**: Test if we can add SPI controller via SSDT override

**Steps**:
1. **Create minimal SSDT** that adds AMDI0061 device:
```asl
DefinitionBlock ("", "SSDT", 2, "LINUX", "SPIFIX", 0x00000001)
{
    External (_SB_.PCI0, DeviceObj)
    
    Scope (_SB.PCI0)
    {
        Device (SPI1)
        {
            Name (_HID, "AMDI0061")
            Name (_UID, Zero)
            // Add resources, _CRS method, etc.
        }
    }
}
```

2. **Compile and test**:
```bash
iasl ssdt-spi-fix.asl
sudo cp ssdt-spi-fix.aml /sys/firmware/acpi/tables/
# Or use initrd method for loading
```

3. **Verify enumeration**:
```bash
ls /sys/class/spi_master/
dmesg | grep "spi-amd"
```

### 5. Hardware Investigation
**Goal**: Understand AMDI0010 dual-mode capabilities

**Research areas**:
1. **AMD documentation** - Look for AMDI0010 technical specs
2. **Designware controller** - Check if it supports SPI mode
3. **Register analysis** - Compare I2C vs SPI register maps
4. **Firmware traces** - How does Windows switch modes?

## Medium-term Development (Next 1-2 Months)

### 6. Custom Controller Driver Development
**Goal**: Create driver that enables SPI mode on AMDI0010

**Approach**:
```c
// Pseudo-code for dual-mode driver
static int amdi0010_spi_probe(struct platform_device *pdev)
{
    // 1. Unbind from i2c_designware
    // 2. Reconfigure hardware registers for SPI mode  
    // 3. Register as SPI controller
    // 4. Create SPI devices from ACPI
}
```

### 7. Testing Infrastructure
**Goal**: Systematic testing of different approaches

**Test matrix**:
- ACPI override vs driver modification
- Different AMDI006x device IDs
- Manual device creation vs automatic enumeration
- spi-hid driver binding verification

## Long-term Goals (3-6 Months)

### 8. Upstream Contribution
**Goal**: Get fixes accepted in mainline kernel

**Process**:
1. **Patch submission** to linux-surface maintainers
2. **Kernel mailing list** discussion for ACPI quirks
3. **Coordination with AMD** for proper hardware support
4. **Microsoft engagement** for firmware fixes

### 9. Comprehensive Solution
**Goal**: Fix touchscreen for all AMD Surface models

**Scope**:
- Surface Laptop 3 AMD (MSHW0162)
- Surface Laptop 4 AMD (MSHW0231)  
- Future AMD Surface devices
- Proper upstream ACPI handling

## Collaboration Opportunities

### Linux-Surface Community
- **Technical expertise** - ACPI override development
- **Testing resources** - Multiple AMD Surface devices
- **Upstream connections** - Kernel maintainer relationships

### AMD/Microsoft
- **Hardware documentation** - AMDI0010 SPI capabilities
- **Firmware updates** - Proper ACPI enumeration
- **Driver coordination** - Official support development

### Kernel Developers
- **ACPI subsystem** - Platform device enumeration
- **SPI subsystem** - Controller driver architecture
- **Surface support** - Device-specific quirks

## Success Metrics

### Phase 1 Success (Immediate)
- [ ] Windows enumeration confirmed/documented
- [ ] ACPI tables fully analyzed
- [ ] Community engagement initiated
- [ ] Technical findings shared

### Phase 2 Success (Short-term)  
- [ ] ACPI override working (SPI controller enumerated)
- [ ] spi-amd driver binding to override device
- [ ] MSHW0231 appearing under SPI controller
- [ ] spi-hid driver attempting to bind

### Phase 3 Success (Medium-term)
- [ ] Touchscreen input events detected
- [ ] Basic touch functionality working
- [ ] Multi-touch and pen support
- [ ] Suspend/resume working correctly

### Final Success (Long-term)
- [ ] Upstream kernel support
- [ ] All AMD Surface models working
- [ ] No custom patches required
- [ ] Community adoption complete

## Resource Requirements

### Technical Skills Needed
- ACPI/ASL programming
- Linux kernel driver development
- SPI protocol understanding
- Hardware reverse engineering

### Tools Required
- ACPI disassembler (iasl)
- Kernel build environment
- Hardware debugging tools
- Windows analysis capabilities

### Community Support
- linux-surface developers
- Kernel maintainer feedback
- AMD Surface device owners for testing
- Documentation and testing coordination

---

**Next Action**: Begin with Windows enumeration verification and community engagement while preparing ACPI analysis tools.