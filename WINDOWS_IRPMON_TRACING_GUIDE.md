# Windows IRPMon Tracing Guide for MSHW0231 Surface Touchscreen

## Current Investigation Status

**Problem**: Linux device communicates successfully but remains stuck in initialization mode (report_type=0x0f) and never transitions to Collection 06 touch mode (report_type=0x06).

**Solution Needed**: Capture Windows driver behavior to identify the missing device state transition mechanism.

## IRPMon Requirements & Limitations

### ❌ **IRPMon Limitations**
- **Windows 11 Compatibility**: Limited/unstable support
- **Secure Boot**: Generally incompatible (requires driver signing bypass)
- **HVCI/VBS**: Core Isolation breaks IRPMon functionality  
- **Driver Signing**: Requires test signing mode or disabled driver signature enforcement

### ✅ **Alternative Tracing Methods for Windows 11**

## **Option 1: Windows Performance Toolkit (WPT) - RECOMMENDED**

**Advantages:**
- ✅ Native Microsoft tool
- ✅ Works with Secure Boot enabled
- ✅ Windows 11 compatible
- ✅ Captures kernel-level HID/SPI activity

**Setup:**
```powershell
# Download Windows SDK for WPT
# Create custom profile for MSHW0231 tracing
```

**Custom WPR Profile (save as MSHW0231_trace.wprp):**
```xml
<WindowsPerformanceRecorder Version="1.0">
  <Profiles>
    <Profile Id="MSHW0231.Verbose" Name="MSHW0231" DetailLevel="Verbose" LoggingMode="File">
      <Collectors>
        <EventCollector Id="EventCollector_MSHW0231" Name="MSHW0231">
          <EventProviders>
            <!-- HID Subsystem -->
            <EventProvider Id="Microsoft-Windows-HID" Name="Microsoft-Windows-HID" Level="5" />
            <!-- ACPI for MSHW0231 device -->
            <EventProvider Id="Microsoft-Windows-ACPI" Name="Microsoft-Windows-ACPI" Level="5" />
            <!-- PnP for device enumeration -->
            <EventProvider Id="Microsoft-Windows-Kernel-PnP" Name="Microsoft-Windows-Kernel-PnP" Level="5" />
            <!-- SPI controller events -->
            <EventProvider Id="Microsoft-Windows-SPI" Name="Microsoft-Windows-SPI" Level="5" />
            <!-- Power management -->
            <EventProvider Id="Microsoft-Windows-Kernel-Power" Name="Microsoft-Windows-Kernel-Power" Level="5" />
          </EventProviders>
        </EventCollector>
      </Collectors>
    </Profile>
  </Profiles>
</WindowsPerformanceRecorder>
```

**Capture Commands:**
```powershell
# Start tracing
wpr -start MSHW0231_trace.wprp

# Perform these actions while tracing:
# 1. Boot sequence (capture device initialization)
# 2. First finger touch on touchscreen
# 3. Multiple touch events
# 4. Device disable/enable in Device Manager

# Stop tracing
wpr -stop MSHW0231_complete_trace.etl

# Analyze with Windows Performance Analyzer (WPA)
wpa MSHW0231_complete_trace.etl
```

## **Option 2: Event Tracing for Windows (ETW)**

**PowerShell ETW Capture:**
```powershell
# Start ETW session targeting HID and ACPI
$session = New-EtwTraceSession -Name "MSHW0231_Trace" -LogFileMode Circular -LogFileName "C:\MSHW0231_ETW.etl"

# Add HID provider (Microsoft-Windows-HID)
Add-EtwTraceProvider -SessionName "MSHW0231_Trace" -Guid "{896f2806-9d0e-4d5f-aa25-7acfc93c8854}" -Level 5

# Add ACPI provider (Microsoft-Windows-ACPI)
Add-EtwTraceProvider -SessionName "MSHW0231_Trace" -Guid "{c514638f-7723-485b-bcfc-96565d735d4a}" -Level 5

# Add PnP provider (Microsoft-Windows-Kernel-PnP)
Add-EtwTraceProvider -SessionName "MSHW0231_Trace" -Guid "{9c205a39-1250-487d-abd7-e831c6290539}" -Level 5

# Perform touch events and device operations
Write-Host "Tracing started. Perform touch events now..."
Read-Host "Press Enter when done"

# Stop session
Remove-EtwTraceSession -Name "MSHW0231_Trace"
```

## **Option 3: Process Monitor (ProcMon) + API Monitor**

**ProcMon Setup:**
```powershell
# Download ProcMon from Microsoft Sysinternals
# Configure filters:
Process Name: contains "dwm" OR "explorer" OR "winlogon" OR "csrss"
Path: contains "MSHW0231" OR "hidspi" OR "touchscreen" OR "surface"
Operation: Process and Thread Activity, Registry, File System

# Focus on registry keys:
HKLM\SYSTEM\CurrentControlSet\Enum\ACPI\MSHW0231
HKLM\SYSTEM\CurrentControlSet\Services\hidspi
HKLM\SYSTEM\CurrentControlSet\Services\HidSpiCx
```

**API Monitor Setup:**
- Download API Monitor (free tool)
- Monitor processes: System, dwm.exe, explorer.exe
- Monitor APIs: `kernel32.dll`, `ntdll.dll`, `hid.dll`
- Focus on: `HidD_*` functions, `DeviceIoControl` calls, `CreateFile` for device handles

## **Option 4: Kernel Debug Output**

**Enable HID/SPI debugging:**
```powershell
# Run as Administrator
# Enable HID debugging
reg add "HKLM\SYSTEM\CurrentControlSet\Services\HidSpiCx\Parameters" /v DebugLevel /t REG_DWORD /d 0xFFFFFFFF /f
reg add "HKLM\SYSTEM\CurrentControlSet\Services\hidspi\Parameters" /v DebugLevel /t REG_DWORD /d 0xFFFFFFFF /f

# Enable ACPI debugging for MSHW0231
reg add "HKLM\SYSTEM\CurrentControlSet\Services\ACPI\Parameters" /v VerboseOn /t REG_DWORD /d 1 /f

# Restart required
shutdown /r /t 0

# Use DebugView (Sysinternals) to capture kernel debug output
# Filter: *MSHW0231* OR *hidspi* OR *collection*
```

## **Option 5: IRPMon with Secure Boot Disabled**

**If you need IRPMon specifically:**

**Disable Secure Boot Method:**
1. Boot into UEFI/BIOS settings
2. Disable Secure Boot
3. Enable Test Signing mode:
   ```cmd
   # Run as Administrator
   bcdedit /set testsigning on
   bcdedit /set nointegritychecks on
   bcdedit /set loadoptions DISABLE_INTEGRITY_CHECKS
   ```
4. Reboot
5. Install and run IRPMon

**IRPMon Target Settings:**
- **Drivers to Monitor:**
  - `hidspi.sys` 
  - `HidSpiCx.sys`
  - `ACPI.sys` (filter for MSHW0231)
  - `spb.sys` (SPI bus driver)

- **Device Filters:**
  - Process: `System` 
  - Device: `ACPI\MSHW0231\*`
  - Device: `HID\MSHW0231&COL*`

- **IRP Types:**
  - `IRP_MJ_PNP` (Plug and Play)
  - `IRP_MJ_INTERNAL_DEVICE_CONTROL` (HID internal)
  - `IRP_MJ_DEVICE_CONTROL` (Device I/O)
  - `IRP_MJ_POWER` (Power management)

**Re-enable Security After Capture:**
```cmd
bcdedit /set testsigning off
bcdedit /set nointegritychecks off
bcdedit /deletevalue loadoptions
# Re-enable Secure Boot in UEFI
```

## **Critical Data to Capture**

### 🎯 **Key Events to Monitor**

1. **System Boot Sequence**
   - MSHW0231 device enumeration
   - HID descriptor requests
   - Initial device configuration

2. **First Touch Event**
   - Clear capture buffer after boot
   - Perform single finger touch
   - Capture exact sequence that triggers mode transition

3. **Device State Changes**
   - Device Manager → Disable MSHW0231
   - Re-enable device
   - Capture re-initialization sequence

### 📋 **Expected Discoveries**

The traces should reveal:

1. **Missing HID Feature Reports**: Commands that Linux isn't sending
2. **ACPI _DSM Methods**: Device-specific methods for mode switching  
3. **Timing Dependencies**: Exact delays Windows uses between commands
4. **SPI Controller Setup**: AMD-specific configuration preventing deadlocks
5. **Multi-Collection Activation**: How Windows enables Collection 06 specifically
6. **Report Type Transition**: What triggers 0x0f → 0x06 mode switch

### 🔍 **Critical Questions to Answer**

1. **What specific command/IRP triggers the 0x0f → 0x06 transition?**
2. **Are there ACPI _DSM calls that Linux is missing?**
3. **What timing delays does Windows use between HID commands?**
4. **Does Windows send different SPI controller configuration?**
5. **How does Windows handle Collection 06 activation differently?**
6. **What HID feature reports enable touch mode?**

## **Analysis Tools**

- **WPA (Windows Performance Analyzer)**: For ETL files
- **EventViewer**: For Windows event logs  
- **WinAPIOverride**: Alternative to API Monitor
- **DebugView**: For kernel debug output
- **Hex editors**: For analyzing binary trace data

## **File Locations for Traces**

Save all traces to: `/home/ccoffey/Nextcloud/touchpad/windows_traces/`

**Naming Convention:**
- `MSHW0231_boot_sequence_YYYYMMDD.etl`
- `MSHW0231_first_touch_YYYYMMDD.etl`  
- `MSHW0231_device_restart_YYYYMMDD.etl`
- `MSHW0231_procmon_YYYYMMDD.pml`

## **Next Steps After Capture**

1. **Analyze traces** for missing Linux commands
2. **Implement discovered IRPs** in Linux driver
3. **Test device state transitions** 
4. **Validate Collection 06 activation**
5. **Confirm real touch event generation**

---

**Current Linux Status**: Stable communication established, phantom touches eliminated, device active but stuck in initialization mode. Windows traces will provide the final missing piece for complete touchscreen functionality.