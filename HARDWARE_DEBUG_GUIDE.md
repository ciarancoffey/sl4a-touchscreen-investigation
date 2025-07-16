# Hardware Debugging Guide for MSHW0231

## Option 1: Logic Analyzer

### Required Tools
- Logic Analyzer (Saleae Logic, DSLogic, or similar)
- Fine-pitch probes or test clips
- Steady hands and magnification

### SPI Signals to Probe
1. **SCLK** - SPI Clock
2. **MOSI** - Master Out Slave In (Commands to device)
3. **MISO** - Master In Slave Out (Responses from device)
4. **CS** - Chip Select (Active low)
5. **GPIO 132** - Reset line (optional but helpful)

### Where to Probe
- Look for test points near the touchscreen connector
- Or probe the flex cable if accessible
- Check for exposed vias on the PCB

### Capture Procedure
1. Connect logic analyzer to SPI signals
2. Set up trigger on CS falling edge
3. Boot into Windows
4. Capture from cold boot through device initialization
5. Export as CSV/VCD for analysis

### What to Look For
- Initial SPI clock frequency
- Command sequences after reset
- First non-0xFF response
- Timing between reset and first command

## Option 2: Software SPI Sniffer

### Linux Kernel Module Approach
Create a SPI sniffer that sits between the controller and device:

```c
// Intercept SPI transfers
static int spi_sniffer_transfer_one(struct spi_controller *ctlr,
                                   struct spi_device *spi,
                                   struct spi_transfer *t)
{
    // Log the transfer
    print_hex_dump(KERN_INFO, "SPI TX: ", DUMP_PREFIX_OFFSET,
                   16, 1, t->tx_buf, t->len, true);
    
    // Call original transfer function
    ret = original_transfer_one(ctlr, spi, t);
    
    // Log the response
    print_hex_dump(KERN_INFO, "SPI RX: ", DUMP_PREFIX_OFFSET,
                   16, 1, t->rx_buf, t->len, true);
    
    return ret;
}
```

## Option 3: QEMU/VM Debugging

### Windows in QEMU with GDB
1. Run Windows in QEMU with gdbstub
2. Set breakpoints on hidspi.sys functions
3. Step through initialization
4. Dump SPI controller registers

### Commands
```bash
qemu-system-x86_64 -s -S -hda windows.img -device ...
gdb
(gdb) target remote :1234
(gdb) break *0xfffff8003e480000  # hidspi.sys base
```

## Option 4: Modified Windows Driver

### Create Logging Wrapper
1. Create a filter driver that logs all SPI transactions
2. Install between hidspi.sys and the SPI controller
3. Log to file or debug output
4. Analyze the initialization sequence

### Sample Code
```c
NTSTATUS
SpiFilter_EvtIoInternalDeviceControl(
    WDFQUEUE Queue,
    WDFREQUEST Request,
    size_t OutputBufferLength,
    size_t InputBufferLength,
    ULONG IoControlCode
    )
{
    // Log the SPI transaction
    LogSpiTransaction(Request);
    
    // Forward to next driver
    return WdfRequestSend(Request, Target, NULL);
}
```