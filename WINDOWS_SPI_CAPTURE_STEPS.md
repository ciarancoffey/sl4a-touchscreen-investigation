# Step-by-Step Windows SPI Capture Guide

## Quick Method: Event Viewer + Device Manager

### 1. Enable Verbose Logging (Run as Administrator)
```powershell
# Enable verbose logging for HID SPI drivers
reg add "HKLM\SYSTEM\CurrentControlSet\Control\WMI\GlobalLogger\{0a6b3bb2-3504-49c1-81d0-6a4b88b96427}" /v Start /t REG_DWORD /d 1 /f
reg add "HKLM\SYSTEM\CurrentControlSet\Control\WMI\GlobalLogger\{0a6b3bb2-3504-49c1-81d0-6a4b88b96427}" /v LogFileMode /t REG_DWORD /d 0x10100 /f
reg add "HKLM\SYSTEM\CurrentControlSet\Control\WMI\GlobalLogger\{5ed8bb73-c76f-49d9-bf05-4982903c6ca5}" /v Start /t REG_DWORD /d 1 /f

# Create debug filter
wevtutil sl Microsoft-Windows-Kernel-PnP/Configuration /e:true
```

### 2. Capture Method A: WPA (Recommended)

1. **Download Windows Performance Toolkit**
   - https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/
   - Or direct: https://go.microsoft.com/fwlink/?linkid=2173743

2. **Create capture script** (save as `capture_spi.bat`):
```batch
@echo off
echo Starting SPI capture...

:: Clear previous logs
del spi_trace.etl 2>nul

:: Start trace
wpr -start GeneralProfile -start HidProfile -filemode

echo Please disable and re-enable the touchscreen in Device Manager
echo Press any key when done...
pause >nul

:: Stop trace
wpr -stop spi_trace.etl

echo Trace saved to spi_trace.etl
```

3. **Run the capture**:
   - Run `capture_spi.bat` as Administrator
   - When prompted, go to Device Manager
   - Find "HID-compliant touch screen" under "Human Interface Devices"
   - Right-click → Disable device
   - Wait 5 seconds
   - Right-click → Enable device
   - Press any key in the command window

### 3. Capture Method B: Direct ETW

Save as `spi_etw_trace.bat`:
```batch
@echo off
:: Create trace session
logman create trace spi_trace -o spi_trace.etl -nb 128 640 -bs 1024 -mode Circular -f bincirc -max 512 -ets

:: Add providers
logman update trace spi_trace -p {0a6b3bb2-3504-49c1-81d0-6a4b88b96427} 0xffffffffffffffff 0xff -ets
logman update trace spi_trace -p {5ed8bb73-c76f-49d9-bf05-4982903c6ca5} 0xffffffffffffffff 0xff -ets
logman update trace spi_trace -p Microsoft-Windows-Kernel-Pnp 0xffffffffffffffff 0xff -ets

echo Trace started. Disable/Enable touchscreen now.
echo Press any key to stop...
pause >nul

:: Stop trace
logman stop spi_trace -ets

echo Converting to text...
tracerpt spi_trace.etl -o spi_trace.txt -of TEXT
```

### 4. Quick PowerShell Analysis

After capturing, run this PowerShell script to extract SPI data:
```powershell
# Save as analyze_spi.ps1
$events = Get-WinEvent -Path ".\spi_trace.etl" -Oldest | Where-Object {
    $_.Message -match "SPI|HID|0231"
}

# Extract potential SPI commands
$events | ForEach-Object {
    if ($_.Message -match "([0-9A-F]{2}\s+){4,}") {
        Write-Host "Time: $($_.TimeCreated) - SPI Data: $($matches[0])"
    }
}

# Save to file
$events | Out-File -FilePath "spi_analysis.txt"
```

### 5. What to Look For

In the captured data, search for:
1. **Reset sequence**: Look for GPIO operations before SPI starts
2. **First SPI command**: Usually 4 bytes, NOT 0xFF
3. **Response pattern**: When device stops returning 0xFF
4. **Command sequence**: The exact order of commands

Common patterns:
```
01 00 00 00  - Reset command
02 00 00 00  - Get HID descriptor  
08 00 00 00  - Set power mode
05 01 00 00  - Get input report
```

### 6. Share the Results

Once captured, look for:
- Lines containing hex data (XX XX XX XX format)
- Timestamps showing command order
- The first non-FF response

Copy the relevant sections to share back here.

## Alternative: Bus Hound (If WPA doesn't work)

1. Download Bus Hound trial: https://www.bushound.com/
2. Install and run as Administrator
3. Select Devices → Find Devices
4. Look for "SPI" or "MSHW0231"
5. Start Capture
6. Disable/Enable touchscreen
7. Stop Capture
8. Export as text

## Quick Test Commands

If you want to test specific commands, save this as `test_spi.ps1`:
```powershell
# Get touchscreen device
$device = Get-PnpDevice | Where-Object {$_.Name -match "touch|HID.*0231"}
$device | Disable-PnpDevice -Confirm:$false
Start-Sleep -Seconds 2
$device | Enable-PnpDevice -Confirm:$false
```