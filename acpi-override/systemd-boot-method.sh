#!/bin/bash
# Alternative ACPI override method for systemd-boot with Secure Boot

set -e

echo "=== Surface Laptop 4 AMD Touchscreen Fix for systemd-boot ==="
echo
echo "Current setup:"
echo "- Boot loader: systemd-boot"
echo "- Secure Boot: ENABLED"
echo

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo "Please run as root (use sudo)"
    exit 1
fi

# Method 1: Using acpi_override kernel parameter
echo "Method 1: Kernel Parameter Approach"
echo "===================================="
echo

# Compile SSDT
echo "1. Compiling SSDT..."
cd /home/ccoffey/repos/sl4a-touchscreen-investigation/acpi-override/
iasl -tc ssdt-spi-fix.asl

# Copy to EFI partition
echo "2. Copying SSDT to EFI partition..."
mkdir -p /efi/acpi
cp ssdt-spi-fix.aml /efi/acpi/

# Check current boot entries
echo "3. Current boot entries:"
ls -la /efi/loader/entries/

echo
echo "To apply the fix, you need to:"
echo "1. Edit your boot entry in /efi/loader/entries/"
echo "2. Add to the options line: acpi_override=/acpi/ssdt-spi-fix.aml"
echo
echo "Example:"
echo "  options root=PARTUUID=xxx rw quiet acpi_override=/acpi/ssdt-spi-fix.aml"
echo

# Method 2: Using configfs (runtime loading)
echo
echo "Method 2: Runtime Loading via configfs"
echo "======================================"
echo "This method works with Secure Boot but requires manual loading after each boot."
echo

# Create script for runtime loading
cat > /usr/local/bin/load-touchscreen-acpi.sh << 'EOF'
#!/bin/bash
# Runtime ACPI table loading for Surface touchscreen

if [ ! -d /sys/kernel/config/acpi/table ]; then
    modprobe acpi_configfs
fi

if [ -d /sys/kernel/config/acpi/table ]; then
    # Create new table entry
    mkdir -p /sys/kernel/config/acpi/table/touchscreen
    
    # Load our compiled SSDT
    cat /home/ccoffey/repos/sl4a-touchscreen-investigation/acpi-override/ssdt-spi-fix.aml > \
        /sys/kernel/config/acpi/table/touchscreen/aml
    
    echo "ACPI table loaded via configfs"
else
    echo "Error: ACPI configfs not available"
    exit 1
fi
EOF

chmod +x /usr/local/bin/load-touchscreen-acpi.sh

echo "Created: /usr/local/bin/load-touchscreen-acpi.sh"
echo

# Method 3: Disable Secure Boot
echo "Method 3: Disable Secure Boot"
echo "=============================="
echo "If the above methods don't work, you can temporarily disable Secure Boot:"
echo
echo "1. Reboot and enter UEFI/BIOS (usually F2 or Del during boot)"
echo "2. Navigate to Security settings"
echo "3. Disable Secure Boot"
echo "4. Save and exit"
echo "5. Run the original ./apply-acpi-override.sh script"
echo
echo "You can re-enable Secure Boot after confirming the fix works."
echo

echo "=== Summary ==="
echo
echo "With Secure Boot enabled, your options are:"
echo "1. Edit systemd-boot entry to add acpi_override= parameter"
echo "2. Run 'sudo /usr/local/bin/load-touchscreen-acpi.sh' after each boot"
echo "3. Temporarily disable Secure Boot for testing"
echo
echo "The SSDT has been compiled to: ./ssdt-spi-fix.aml"