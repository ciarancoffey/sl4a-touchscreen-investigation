# Reverse Engineering Windows HidSpi Driver

## Tools Needed
- IDA Pro / Ghidra (free alternative)
- WinDbg
- Windows SDK

## Steps to Analyze hidspi.sys

### 1. Extract hidspi.sys from Windows
```powershell
copy C:\Windows\System32\drivers\hidspi.sys C:\analysis\
copy C:\Windows\System32\drivers\HidSpiCx.sys C:\analysis\
```

### 2. Load in Ghidra/IDA
- Import hidspi.sys
- Let it analyze
- Look for key functions:
  - `HidSpi_Reset`
  - `HidSpi_Initialize`
  - `HidSpi_SendCommand`
  - `HidSpi_PowerOn`

### 3. Key Patterns to Find

#### Reset Sequence
Look for GPIO operations:
```asm
; Typically looks like:
mov ecx, 0x904  ; GPIO 2308
call IoWriteGpio
```

#### SPI Commands
Find command buffers:
```asm
; Look for patterns like:
mov byte ptr [rsp+0], 0x01  ; Command byte
mov byte ptr [rsp+1], 0x00  ; Sub-command
mov byte ptr [rsp+2], 0x00  ; Parameter
mov byte ptr [rsp+3], 0x00  ; Parameter
```

#### Collection 06 Handling
Search for:
- `0x06` constants
- Collection parsing code
- HID descriptor parsing

### 4. Dynamic Analysis with WinDbg

```
# Attach to System process
kd> !process 0 0 System

# Set breakpoints
kd> bp hidspi!DriverEntry
kd> bp hidspi!HidSpi_StartDevice
kd> bp hidspi!HidSpi_D0Entry

# Log SPI transactions
kd> bp nt!IofCallDriver "j (@rcx->MajorFunction==0xe) '.printf \"SPI: \"; dd @rdx L4; g'"
```

### 5. Common SPI HID Commands

Based on HID over SPI specification:
- `0x01 0x00` - Reset
- `0x02 0x00` - Get HID Descriptor  
- `0x03 0x00` - Set Power (D0)
- `0x04 0x00` - Set Power (D3)
- `0x05 0x00` - Get/Set Report
- `0x06 0x00` - Get Input Report

### 6. MSHW0231 Specific Sequences

Look for device-specific initialization:
- Vendor commands (0x80-0xFF range)
- Special timing requirements
- Collection-specific setup

## Expected Findings

1. **GPIO Sequence**
   - Which GPIO pins are used
   - Reset timing (high/low durations)
   - Multiple reset sequences?

2. **SPI Commands**
   - Initial command after reset
   - HID descriptor request format
   - Collection 06 specific commands

3. **Timing Requirements**
   - Delays between commands
   - Timeout values
   - Retry logic

## Quick Python Script for Analysis

```python
import re
import sys

def find_spi_commands(binary_file):
    with open(binary_file, 'rb') as f:
        data = f.read()
    
    # Look for potential SPI command patterns
    # Usually 4-byte aligned commands
    for i in range(0, len(data) - 4, 4):
        if data[i] in [0x01, 0x02, 0x03, 0x04, 0x05, 0x06]:
            if data[i+1] == 0x00 and data[i+2] == 0x00:
                print(f"Potential command at 0x{i:08x}: {data[i:i+4].hex()}")

if __name__ == "__main__":
    find_spi_commands("hidspi.sys")
```