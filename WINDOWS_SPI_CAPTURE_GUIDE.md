# Windows SPI Protocol Capture Guide

## Method 1: Windows Performance Analyzer (WPA)

1. **Install Windows Performance Toolkit**
   - Part of Windows SDK
   - Or standalone: https://docs.microsoft.com/en-us/windows-hardware/test/wpt/

2. **Create Custom Trace Profile**
   ```xml
   <?xml version="1.0" encoding="utf-8"?>
   <WindowsPerformanceRecorder Version="1.0">
     <Profiles>
       <SystemCollector Id="SystemCollector" Name="NT Kernel Logger">
         <BufferSize Value="1024"/>
         <Buffers Value="100"/>
       </SystemCollector>
       <EventCollector Id="EventCollector" Name="Event Collector">
         <BufferSize Value="1024"/>
         <Buffers Value="100"/>
       </EventCollector>
       <SystemProvider Id="SystemProvider">
         <Keywords>
           <Keyword Value="0x0000000000040000"/> <!-- DRIVER -->
           <Keyword Value="0x0000000000080000"/> <!-- IRP -->
           <Keyword Value="0x0000000000100000"/> <!-- PNP -->
         </Keywords>
       </SystemProvider>
       <EventProvider Id="SPI-Provider" Name="*HidSpi*" Level="5">
         <Keywords>
           <Keyword Value="0xFFFFFFFFFFFFFFFF"/>
         </Keywords>
       </EventProvider>
       <Profile Id="SPI-Trace.Verbose.File" Name="SPI Trace" DetailLevel="Verbose" LoggingMode="File" Description="SPI HID Trace">
         <Collectors>
           <SystemCollectorId Value="SystemCollector">
             <SystemProviderId Value="SystemProvider"/>
           </SystemCollectorId>
           <EventCollectorId Value="EventCollector">
             <EventProviderId Value="SPI-Provider"/>
           </EventCollectorId>
         </Collectors>
       </Profile>
     </Profiles>
   </WindowsPerformanceRecorder>
   ```

3. **Capture Trace**
   ```cmd
   wpr -start SPI-Trace.wprp
   # Disable/Enable touchscreen in Device Manager
   wpr -stop spi-capture.etl
   ```

## Method 2: Bus Hound or USBPcap for SPI

1. **Bus Hound** (Commercial, but has trial)
   - Supports SPI bus monitoring
   - Shows exact byte sequences
   - Download: https://www.bushound.com/

2. **Setup**
   - Install Bus Hound
   - Select SPI device (MSHW0231)
   - Start capture
   - Disable/Enable device
   - Stop and analyze

## Method 3: Windows Driver Logging

1. **Enable Driver Verifier**
   ```cmd
   verifier /standard /driver hidspi.sys HidSpiCx.sys
   ```

2. **Enable WPP Tracing**
   ```cmd
   reg add "HKLM\SYSTEM\CurrentControlSet\Services\hidspi\Parameters" /v VerboseOn /t REG_DWORD /d 1
   reg add "HKLM\SYSTEM\CurrentControlSet\Services\hidspi\Parameters" /v LogLevel /t REG_DWORD /d 7
   ```

3. **Capture with WPA**
   ```cmd
   wpa -start -wpa -provider {0a6b3bb2-3504-49c1-81d0-6a4b88b96427} -level 0xFF -f spi.etl
   # Restart device
   wpa -stop
   ```

## Method 4: Kernel Debugger (Most Detailed)

1. **Enable Kernel Debugging**
   ```cmd
   bcdedit /debug on
   bcdedit /dbgsettings serial debugport:1 baudrate:115200
   ```

2. **Set Breakpoints on SPI Functions**
   ```
   kd> bp hidspi!*Reset*
   kd> bp hidspi!*Init*
   kd> bp nt!IopCallDriver
   ```

3. **Log SPI Transactions**
   ```
   kd> wt -l 10 -m hidspi
   ```

## Expected Data to Capture

Look for:
1. **Reset Sequence Timing**
2. **Initial SPI Commands** (likely 4-byte sequences)
3. **HID Descriptor Request** (0x0100 or similar)
4. **Collection-specific Commands**
5. **Power State Transitions**

## Analysis Tips

1. Focus on the first 100ms after device enable
2. Look for non-0xFF responses
3. Compare working (after Windows init) vs non-working (cold boot) states
4. Identify the exact command that transitions from 0xFF to valid data