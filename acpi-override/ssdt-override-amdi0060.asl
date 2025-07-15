/*
 * SSDT overlay to override AMDI0060 with AMDI0061 for Surface Laptop 4 AMD
 * This changes the existing SPI controller ID to one supported by spi-amd driver
 */

DefinitionBlock ("", "SSDT", 2, "LINUX", "SL4AFIX", 0x00000001)
{
    External (_SB.SPI1, DeviceObj)
    
    /*
     * Override the Hardware ID of the existing SPI1 controller
     * Change AMDI0060 to AMDI0061 so spi-amd driver will bind
     */
    Scope (_SB.SPI1)
    {
        // Override the _HID method to return AMDI0061 instead of AMDI0060
        Method (_HID, 0, NotSerialized)
        {
            Return ("AMDI0061")
        }
        
        // Ensure compatibility name stays the same
        Name (_CID, "AMDI0060")
    }
}