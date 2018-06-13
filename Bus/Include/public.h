/*++
Copyright (c) 2004 Chingachguk & Denger2k All Rights Reserved

Module Name:

    public.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.

Environment:

    user and kernel
Notes:


Revision History:


--*/

//
// Define an Interface Guid for bus enumerator class.
// This GUID is used to register (IoRegisterDeviceInterface) 
// an instance of an interface so that enumerator application 
// can send an ioctl to the bus driver.
//

DEFINE_GUID(GUID_DEVINTERFACE_VUSB, 
    0x45e0f259, 0x9cbf, 0x456e, 0x9d, 0xa5, 0x82, 0x1a, 0xa4, 0x2c, 0x4a, 0x1);
// {45E0F259-9CBF-456e-9DA5-821AA42C4A01}

//
// GUID definition are required to be outside of header inclusion pragma to avoid
// error during precompiled headers.
//

#ifndef __PUBLIC_H
#define __PUBLIC_H

#define BUS_HARDWARE_IDS L"USB\\VID_0529&PID_0001&Rev_0100\0\0"
#define BUS_HARDWARE_IDS_LENGTH sizeof (BUS_HARDWARE_IDS)

#define FILE_DEVICE_VUSB         FILE_DEVICE_BUS_EXTENDER

#define VUSB_IOCTL(_index_) \
    CTL_CODE (FILE_DEVICE_VUSB, _index_, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_VUSB_PLUGIN_HARDWARE               VUSB_IOCTL (0x0)
#define IOCTL_VUSB_UNPLUG_HARDWARE               VUSB_IOCTL (0x1)
#define IOCTL_VUSB_EJECT_HARDWARE                VUSB_IOCTL (0x2)
#define IOCTL_VUSB_DONT_DISPLAY_IN_UI_DEVICE     VUSB_IOCTL (0x3)

//
//  Data structure used in PlugIn and UnPlug ioctls
//

typedef struct _VUSB_PLUGIN_HARDWARE
{
    //
    // sizeof (struct _VUSB_HARDWARE)
    //
    IN ULONG Size;                          
    
    //
    // Unique serial number of the device to be enumerated.
    // Enumeration will be failed if another device on the 
    // bus has the same serail number.
    //

    IN ULONG SerialNo;

    //
    // Password of key
    //
    IN ULONG Password;
    
    //
    // An array of (zero terminated wide character strings). The array itself
    //  also null terminated (ie, MULTI_SZ)
    //
    
    IN  WCHAR   HardwareIDs[]; 
                                                                        
} VUSB_PLUGIN_HARDWARE, *PVUSB_PLUGIN_HARDWARE;

typedef struct _VUSB_UNPLUG_HARDWARE
{
    //
    // sizeof (struct _REMOVE_HARDWARE)
    //

    IN ULONG Size;                                    

    //
    // Serial number of the device to be plugged out    
    //

    ULONG   SerialNo;
    
    ULONG Reserved[2];    

} VUSB_UNPLUG_HARDWARE, *PVUSB_UNPLUG_HARDWARE;

typedef struct _VUSB_EJECT_HARDWARE
{
    //
    // sizeof (struct _EJECT_HARDWARE)
    //

    IN ULONG Size;                                    

    //
    // Serial number of the device to be ejected
    //

    ULONG   SerialNo;
    
    ULONG Reserved[2];    

} VUSB_EJECT_HARDWARE, *PVUSB_EJECT_HARDWARE;


#endif


