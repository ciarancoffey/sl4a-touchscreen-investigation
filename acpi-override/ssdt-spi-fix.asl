/*
 * SSDT overlay to add missing AMD SPI controller for Surface Laptop 4 AMD
 * This fixes touchscreen enumeration by adding AMDI0061 SPI controller
 * that Windows uses but Linux is missing.
 */

DefinitionBlock ("", "SSDT", 2, "LINUX", "SL4ASPI", 0x00000001)
{
    External (_SB, DeviceObj)
    External (_SB.PCI0, DeviceObj)
    External (_SB.GPIO, DeviceObj)
    
    Scope (_SB)
    {
        // AMD SPI Controller - matches Windows configuration
        Device (SPI1)
        {
            Name (_HID, "AMDI0061")  // AMD SPI Controller
            Name (_UID, 0x01)
            Name (_DDN, "AMD SPI Controller")
            
            // Status - enabled
            Method (_STA, 0, NotSerialized)
            {
                Return (0x0F)
            }
            
            // Current Resource Settings
            Name (_CRS, ResourceTemplate ()
            {
                // Memory resource - typical AMD SPI controller region
                Memory32Fixed (ReadWrite,
                    0xFEC10000,         // Address
                    0x00001000,         // Length
                    )
                    
                // Interrupt - standard SPI interrupt
                Interrupt (ResourceConsumer, Level, ActiveLow, Exclusive, ,, )
                {
                    0x00000023,  // IRQ 35
                }
            })
            
            // SPI bus configuration
            Name (SPCF, ResourceTemplate ()
            {
                SpiSerialBusV2 (0x0000, PolarityLow, FourWireMode, 0x08,
                    ControllerInitiated, 0x00989680, ClockPolarityLow,
                    ClockPhaseFirst, "\\_SB.SPI1",
                    0x00, ResourceConsumer, , Exclusive,
                    )
            })
        }
        
        // Move MSHW0231 touchscreen to SPI bus
        Device (TCH1)
        {
            Name (_HID, "MSHW0231")
            Name (_UID, One)
            Name (_DDN, "Microsoft Surface Touchscreen")
            
            Method (_STA, 0, NotSerialized)
            {
                Return (0x0F)
            }
            
            // Dependencies - requires GPIO and SPI
            Name (_DEP, Package (0x02)
            {
                \_SB.GPIO,
                \_SB.SPI1
            })
            
            // Current Resource Settings - from Windows analysis
            Name (_CRS, ResourceTemplate ()
            {
                // SPI connection on bus 11 (0x0B)
                SpiSerialBusV2 (0x000B, PolarityLow, FourWireMode, 0x08,
                    ControllerInitiated, 0x007A1200, ClockPolarityLow,
                    ClockPhaseFirst, "\\_SB.SPI1",
                    0x00, ResourceConsumer, , Exclusive,
                    )
                    
                // GPIO Interrupt - IRQ 4228 from Windows
                GpioInt (Level, ActiveLow, Exclusive, PullUp, 0x0000,
                    "\\_SB.GPIO", 0x00, ResourceConsumer, ,
                    )
                    {
                        0x0084  // GPIO 132
                    }
                    
                // GPIO Reset - GPIO 2308 from Windows  
                GpioIo (Exclusive, PullDefault, 0x0000, 0x0000, IoRestrictionOutputOnly,
                    "\\_SB.GPIO", 0x00, ResourceConsumer, ,
                    )
                    {
                        0x0904  // GPIO 2308
                    }
            })
            
            
            // DSM - Device Specific Method for HID over SPI
            Method (_DSM, 4, NotSerialized)
            {
                If ((Arg0 == ToUUID ("6e2ac436-0fcf-41af-a265-b32a220dcfab")))
                {
                    If ((Arg1 == One))
                    {
                        If ((Arg2 == Zero))
                        {
                            Return (Buffer (One)
                            {
                                0x03
                            })
                        }
                        
                        If ((Arg2 == One))
                        {
                            // HID Descriptor Address for SPI
                            Return (0x20)
                        }
                    }
                }
                
                Return (Buffer (One)
                {
                    0x00
                })
            }
        }
    }
}