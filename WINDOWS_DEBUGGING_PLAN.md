# Surface Laptop 4 AMD Touchscreen - Windows Debugging Plan

## Current Status
- ✅ Linux SPI communication works for READ operations
- ❌ ANY HID output command causes AMD AMDI0060 SPI controller deadlock
- ✅ Device detection, ACPI _DSM, GPIO reset all working
- ⚠️ Device remains in standby (0xFF) - needs Windows analysis

## Critical Finding
**The AMD AMDI0060 SPI controller crashes on ANY write/output operation**, but Windows touchscreen works perfectly. This means Windows either:
1. Uses different SPI controller configuration
2. Has different initialization sequence that avoids output commands initially
3. Accesses device through alternative pathway

## Windows Debugging Strategy

### Phase 1: SPI/HID Trace Capture

#### Required Tools
1. **Windows Performance Toolkit (WPT)**
   - Download from Microsoft website
   - Provides ETW (Event Tracing for Windows) capabilities

2. **USB/HID Logger**
   - HID-SMART or similar tool
   - Captures low-level HID communication

3. **SPI Bus Analyzer** (if available)
   - Logic analyzer with SPI protocol support
   - Hardware approach for direct SPI monitoring

4. **Process Monitor (ProcMon)**
   - Monitor driver file/registry access
   - Already have some traces

#### Trace Capture Plan

**Step 1: Baseline Capture**
```bash
# Boot into Windows
# Start ETW session BEFORE touchscreen initialization
wpr -start generalprofile -start heapprofile -start drivers

# Or manual ETW:
tracelog -start MySpiSession -guid #GUID-HERE# -flag 0xFFFFFFFF -level 5
```

**Step 2: Touchscreen Event Capture**
```bash
# Capture during these events:
1. System boot (driver initialization)
2. Touchscreen first touch
3. Sleep/wake cycle
4. Driver restart (Device Manager disable/enable)
```

**Step 3: Specific SPI Tracing**
```bash
# Enable SPI controller tracing:
tracelog -start SpiTrace -guid {AMDI0060-GUID} -flag 0xFFFFFFFF
# Touch screen multiple times
# Stop trace:
tracelog -stop SpiTrace
```

#### Data Collection Points

**Critical Windows Information Needed:**
1. **SPI Controller Configuration**
   - Clock frequency used by Windows
   - SPI mode (CPOL/CPHA)
   - Chip select timing
   - DMA vs polling mode

2. **HID Initialization Sequence**
   - What HID commands does Windows send first?
   - Are there pre-initialization commands?
   - How does Windows wake device from standby?

3. **Driver Communication Pattern**
   - Does Windows use different ACPI methods?
   - Are there firmware calls we're missing?
   - Power management sequence differences

4. **Device State Transitions**
   - How does device transition from 0xFF to active?
   - What triggers the transition?
   - Are there intermediate states?

### Phase 2: Detailed Analysis

#### SPI Communication Analysis
1. **Compare Timing**
   - Windows SPI clock vs Linux 1MHz
   - Setup/hold times
   - Inter-transaction delays

2. **Command Sequence Analysis**
   - First commands sent by Windows
   - Order of operations
   - Error handling differences

3. **Power Management**
   - ACPI D-state transitions
   - GPIO pin usage differences
   - Reset sequence timing

#### Driver Architecture Analysis
1. **Windows Driver Stack**
   - HIDSpiCx.sys behavior
   - Surface-specific drivers
   - ACPI interaction patterns

2. **Registry Analysis**
   - Device-specific configuration
   - Timing parameters
   - Hidden settings

### Phase 3: Implementation Strategy

#### Based on Windows Analysis
1. **If SPI Timing Issue:**
   - Adjust Linux SPI configuration to match Windows
   - Implement Windows-style delays
   - Fix controller setup sequence

2. **If Initialization Sequence Issue:**
   - Implement Windows command sequence in Linux
   - Add missing ACPI calls
   - Replicate power management flow

3. **If Controller Driver Issue:**
   - Modify AMD SPI controller driver
   - Add Surface-specific quirks
   - Implement alternative communication path

## Execution Checklist

### Pre-Windows Boot Preparation
- [ ] Install Windows debugging tools
- [ ] Set up trace collection scripts
- [ ] Prepare storage for large trace files
- [ ] Document current Linux state for comparison

### Windows Trace Session
- [ ] Start ETW tracing before boot
- [ ] Capture SPI controller initialization
- [ ] Record first touchscreen interaction
- [ ] Test sleep/wake scenarios
- [ ] Capture driver restart sequence

### Post-Capture Analysis
- [ ] Extract SPI timing data
- [ ] Identify initialization command sequence
- [ ] Document power management differences
- [ ] Create Linux implementation plan

## Expected Outcomes

**Success Criteria:**
1. Identify exact SPI configuration Windows uses
2. Understand initialization sequence that avoids crashes
3. Find missing commands/ACPI methods
4. Create actionable Linux implementation plan

**Fallback Options:**
1. If Windows uses proprietary methods → Research alternative approaches
2. If hardware limitation → Document findings for community
3. If firmware issue → Consider UEFI/BIOS modifications

## File Locations
- Traces: `/home/ccoffey/Nextcloud/touchpad/windows_traces/`
- Analysis: `/home/ccoffey/repos/sl4a-touchscreen-investigation/windows_analysis/`
- Implementation: Continue in existing Linux driver files

---

**Next Step:** Boot into Windows with debugging tools ready and execute Phase 1 trace capture.