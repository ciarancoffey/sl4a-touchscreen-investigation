# Disable Secure Boot for Testing

## Option 3: Temporarily Disable Secure Boot

To test the ACPI override properly, we need to disable Secure Boot temporarily:

### Step 1: Access UEFI/BIOS
1. **Reboot** your system
2. **Press F2** or **Del** immediately during boot to enter UEFI/BIOS
   - On Surface devices, you may need to **hold Volume Up + Power** while turning on
3. Navigate to **Security** settings

### Step 2: Disable Secure Boot
1. Find **Secure Boot** option
2. Set it to **Disabled**
3. **Save and Exit** (usually F10)

### Step 3: Apply ACPI Override
Once back in Linux with Secure Boot disabled:

```bash
cd /home/ccoffey/repos/sl4a-touchscreen-investigation/acpi-override/
sudo ./apply-acpi-override.sh
sudo reboot
```

### Step 4: Test Touchscreen
After reboot:

```bash
./verify-touchscreen.sh

# Expected to see:
# - /sys/class/spi_master/spi1/
# - MSHW0231 under /sys/bus/spi/devices/
# - spi-hid driver loaded and bound
```

### Step 5: Test Touch Input
```bash
# Monitor touch events
sudo libinput debug-events

# Check input devices
xinput list | grep -i touch
```

### Step 6: Re-enable Secure Boot (Optional)
If the touchscreen works:
1. Reboot into UEFI/BIOS again
2. Re-enable Secure Boot
3. The ACPI override should persist

## Alternative: If You Can't Disable Secure Boot

Some enterprise devices have Secure Boot locked. In that case:

1. **Contact IT** if this is a corporate device
2. **Use Option 1** - compile a custom kernel with the AMDI0060 patch
3. **Wait for upstream fix** - submit the findings to linux-surface

## Surface-Specific UEFI Access

### Surface Laptop 4:
- **Method 1**: Hold **Volume Up + Power** while device is off, release when Surface logo appears
- **Method 2**: Boot to Windows → Settings → Update & Security → Recovery → Advanced startup → Restart now → Troubleshoot → Advanced options → UEFI Firmware Settings

### Alternative Boot Menu:
- **F12** during boot for boot menu
- Some UEFI options might be available from there