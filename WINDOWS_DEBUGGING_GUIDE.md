# Windows Debugging Guide for Surface Laptop 4 AMD Touchscreen

**Purpose**: Capture detailed Windows initialization sequences to solve the final Linux device initialization issue

**Status**: Device enumeration solved in Linux - need Windows protocol analysis for initialization sequence

**Target**: MSHW0231 touchscreen SPI HID device initialization

---

## 🎯 Current Linux Status

### ✅ What's Working in Linux
- **SPI controller**: AMDI0060 recognized and working
- **Device enumeration**: MSHW0231 appears on SPI bus
- **Driver binding**: spi-hid driver loaded and communicating
- **Data exchange**: Regular SPI communication every 611ms

### ❌ Remaining Issue
- **Device state**: Returns `0xFF` pattern (reset/standby state)
- **Initialization**: Missing proper wake-up/initialization sequence
- **Protocol**: Need to understand Windows initialization commands

---

## 🔍 Windows Investigation Tasks

### Phase 1: Device Initialization Capture (PRIORITY)

#### 1.1 SPI Communication Tracing
**Goal**: Capture exact SPI commands Windows sends during device initialization

**Tools Needed**:
- **ProcMon** (Process Monitor) - for high-level device operations
- **WPA/ETW** (Event Tracing for Windows) - for low-level SPI traces
- **PowerShell** - for device management commands

**Steps**:
1. **Boot into Windows**
2. **Start ProcMon BEFORE touchscreen initialization**:
   ```cmd
   # Run as Administrator
   procmon.exe
   ```
3. **Configure ProcMon filters**:
   - Process Name: `System`
   - Path contains: `MSHW0231`
   - Path contains: `hidspi`
   - Path contains: `AMDI0060`
   - Include: Process and Thread Activity, Registry, File System

4. **Capture touchscreen initialization**:
   ```powershell
   # Disable touchscreen
   Get-PnpDevice | Where-Object {$_.InstanceId -like "*MSHW0231*"} | Disable-PnpDevice -Confirm:$false
   
   # Wait 10 seconds
   Start-Sleep -Seconds 10
   
   # Re-enable touchscreen  
   Get-PnpDevice | Where-Object {$_.InstanceId -like "*MSHW0231*"} | Enable-PnpDevice -Confirm:$false
   ```

5. **Save ProcMon trace**: `surface-touchscreen-init-YYYYMMDD-HHMMSS.pml`

#### 1.2 Hardware-Level SPI Tracing
**Goal**: Capture low-level SPI bus communication

**Method 1: ETW Tracing**
```powershell
# Start SPI trace
wpr -start SPI.wprp

# Trigger touchscreen reset (from 1.1 above)

# Stop trace  
wpr -stop spi-trace.etl

# Convert for analysis
wpa spi-trace.etl
```

**Method 2: Driver Debug Logs**
```cmd
# Enable SPI driver debug logging
reg add "HKLM\SYSTEM\CurrentControlSet\Control\WMI\Autologger\EventLog-System\{D75DE688-582B-477E-BE2F-EA540F0F0F3C}" /v Enabled /t REG_DWORD /d 1 /f

# Reboot and check event logs
eventvwr.msc
```

#### 1.3 Registry Configuration Analysis
**Goal**: Document exact device configuration parameters

**Commands**:
```powershell
# Export MSHW0231 configuration
reg export "HKLM\SYSTEM\CurrentControlSet\Enum\ACPI\MSHW0231" mshw0231-config.reg

# Export SPI driver configuration  
reg export "HKLM\SYSTEM\CurrentControlSet\Services\hidspi" hidspi-config.reg

# Export device parameters
reg export "HKLM\SYSTEM\CurrentControlSet\Enum\ACPI\MSHW0231\a\Device Parameters" mshw0231-params.reg

# Check power management settings
Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Enum\ACPI\MSHW0231\a\Device Parameters" | Format-List
```

### Phase 2: Protocol Analysis (SECONDARY)

#### 2.1 Device Timing Analysis
**Goal**: Understand initialization timing requirements

**Method**:
1. **Use Performance Toolkit**:
   ```cmd
   wpr -start CPU.wprp
   # Trigger touchscreen reset
   wpr -stop timing-analysis.etl
   ```

2. **Analyze with Windows Performance Analyzer** (WPA)

#### 2.2 Power State Transitions  
**Goal**: Document power management sequence

**Commands**:
```powershell
# Monitor power state changes
Get-WinEvent -FilterHashtable @{LogName='System'; ID=109,187,506} | Where-Object {$_.Message -like "*MSHW0231*"}

# Check current power state
Get-PnpDevice | Where-Object {$_.InstanceId -like "*MSHW0231*"} | Get-PnpDeviceProperty -KeyName 'DEVPKEY_Device_PowerState'
```

#### 2.3 HID Report Analysis
**Goal**: Understand expected HID communication format

**Tools**: 
- **HID Analyzer tools**
- **USB/HID debugging utilities**

---

## 📊 Key Data to Collect

### Critical Information Needed 🎯

#### 1. **SPI Initialization Sequence**
- [ ] **First command** sent to device after power-on
- [ ] **Command timing** intervals (delays between commands)  
- [ ] **Expected responses** from device
- [ ] **Retry logic** if device doesn't respond
- [ ] **Success indicators** (what indicates device is ready)

#### 2. **GPIO/Reset Timing**
- [ ] **Reset pin timing** (GPIO 2308 control sequence)
- [ ] **Power sequencing** (voltage rail timing)
- [ ] **Interrupt setup** (GPIO 132 configuration)
- [ ] **Clock frequency** actual vs specified (8MHz)

#### 3. **Protocol Details**
- [ ] **SPI mode settings** (polarity, phase, bit order)
- [ ] **Transfer sizes** (how many bytes per transaction)
- [ ] **Header format** (what valid headers look like vs our 0xFF)
- [ ] **Error handling** (how Windows handles communication errors)

### Specific Patterns to Look For 🔍

#### In ProcMon Traces:
```
Look for:
- Registry writes to GPIO/SPI configuration
- File I/O to device nodes  
- Process creation of HID services
- Timing between operations
- Error conditions and retries
```

#### In Event Logs:
```
Search for:
- Event ID 109 (Device power state change)
- Event ID 187 (Device enumeration)  
- Logs containing "MSHW0231", "hidspi", "AMDI0060"
- SPI-related error messages
```

---

## 📁 Data Collection Checklist

### Pre-Investigation Setup ✅
- [ ] **Clean Windows boot** (restart before testing)
- [ ] **Administrator access** confirmed
- [ ] **All tools installed**: ProcMon, WPA, PowerShell 5.1+
- [ ] **Baseline capture**: Device working normally

### Investigation Execution 📋
- [ ] **ProcMon trace**: Device disable→enable cycle captured
- [ ] **Registry exports**: All MSHW0231 configuration saved
- [ ] **Event logs**: Power and device events captured  
- [ ] **Timing analysis**: ETW traces for initialization timing
- [ ] **Error conditions**: Capture failed initialization if possible

### Post-Investigation Analysis 🔬
- [ ] **Compare with Linux**: Match Windows sequence to Linux behavior
- [ ] **Identify gaps**: What Linux is missing vs Windows
- [ ] **Protocol documentation**: Create initialization command reference
- [ ] **Test implementation**: Try Windows sequence in Linux

---

## 🛠️ Tools Installation

### Required Tools
```powershell
# Download ProcMon (if not installed)
# https://docs.microsoft.com/sysinternals/downloads/procmon

# Check WPA availability (included in Windows SDK)
wpa.exe

# Verify PowerShell version
$PSVersionTable.PSVersion
```

### Optional Advanced Tools
- **Windows Performance Toolkit** (WPT) - for ETW tracing
- **Device Manager** - for manual device control
- **Registry Editor** - for configuration analysis
- **Event Viewer** - for system logs

---

## 📝 Expected Output Files

After completing this investigation, you should have:

### Trace Files 📊
```
windows-debugging/
├── procmon-touchscreen-init-YYYYMMDD.pml      ← Main ProcMon trace
├── spi-etw-trace-YYYYMMDD.etl                 ← SPI bus traces  
├── timing-analysis-YYYYMMDD.etl               ← Timing data
└── event-logs-YYYYMMDD.evtx                   ← System event logs
```

### Configuration Files ⚙️
```
registry-exports/
├── mshw0231-config.reg                        ← Device configuration
├── hidspi-config.reg                          ← Driver configuration  
├── mshw0231-params.reg                        ← Device parameters
└── power-management.reg                       ← Power settings
```

### Analysis Documents 📋
```
analysis/
├── initialization-sequence.md                 ← Step-by-step init process
├── spi-protocol-specification.md              ← Technical protocol details
├── timing-requirements.md                     ← Timing analysis
└── linux-windows-comparison.md                ← Gap analysis
```

---

## 🎯 Success Criteria

### Investigation Complete When: ✅
1. **Initialization sequence documented** - exact commands Windows sends
2. **Timing requirements known** - delays and sequencing captured  
3. **Protocol specification** - valid vs invalid response patterns
4. **GPIO control sequence** - reset pin timing documented
5. **Linux implementation gap** - clear understanding of what's missing

### Ready for Linux Implementation When: 🚀
- **Reproducible Windows behavior** captured in traces
- **Missing Linux functionality** clearly identified  
- **Implementation strategy** defined based on Windows analysis
- **Test methodology** established for validating fixes

---

## 💡 Quick Start Commands

**When you boot into Windows, run these immediately**:

```powershell
# 1. Start monitoring
procmon.exe

# 2. Check current device status  
Get-PnpDevice | Where-Object {$_.InstanceId -like "*MSHW0231*"}

# 3. Test touchscreen reset
Get-PnpDevice | Where-Object {$_.InstanceId -like "*MSHW0231*"} | Disable-PnpDevice -Confirm:$false
Start-Sleep -Seconds 5
Get-PnpDevice | Where-Object {$_.InstanceId -like "*MSHW0231*"} | Enable-PnpDevice -Confirm:$false

# 4. Verify functionality
# Test touch input to confirm device working

# 5. Save ProcMon trace with descriptive filename
```

**This investigation will provide the final pieces needed to complete the Linux touchscreen solution!** 🎉