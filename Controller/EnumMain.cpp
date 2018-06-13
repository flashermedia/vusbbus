/*++

Copyright (c) 2004 Chingachguk & Denger2k All rights reserved.

Module Name:

    Enum.c

Environment:

    usermode console application

Revision History:


--*/

#include <basetyps.h>
#include <rpc.h>
#include <rpcndr.h>
#include <stdlib.h>
#include <wtypes.h>
#include <setupapi.h>
#include <initguid.h>
#include <stdio.h>
#include <string.h>
#include <winioctl.h>
#include "..\Bus\Include\public.h"

DEFINE_GUID (GUID_DEVINTERFACE_VUSB1,
	0x45e0f259, 0x9cbf, 0x456e, 0x9d, 0xa5, 0x82, 0x1a, 0xa4, 0x2c, 0x4a, 0x1);
// {45E0F259-9CBF-456e-9DA5-821AA42C4A01}

//
// Prototypes
//

BOOLEAN
OpenBusInterface (
    IN       HDEVINFO                    HardwareDeviceInfo,
    IN       PSP_DEVICE_INTERFACE_DATA   DeviceInterfaceData
    );



#define USAGE  \
"Usage: Enum [-p SerialNo] [Password] Plugs in a device. SerialNo must be greater than zero.\n\
             [-u SerialNo or 0] Unplugs device(s) - specify 0 to unplug all \
                                the devices enumerated so far.\n\
             [-e SerialNo or 0] Ejects device(s) - specify 0 to eject all \
                                the devices enumerated so far.\n"

BOOLEAN     bPlugIn, bUnplug, bEject;
ULONG       SerialNo, Password;

int _cdecl main (int argc, char *argv[])
{
    HDEVINFO                    hardwareDeviceInfo;
    SP_DEVICE_INTERFACE_DATA    deviceInterfaceData;

    bPlugIn = bUnplug = FALSE;

    if(argc <3) {
        goto usage;
    }

    if(argv[1][0] == '-') {
        if(tolower(argv[1][1]) == 'p') {
            if(argv[2])
                SerialNo = (ULONG)atol(argv[2]);
                if(argv[3])
                    Password = (ULONG)atol(argv[3]);
        bPlugIn = TRUE;
        }
        else if(tolower(argv[1][1]) == 'u') {
            if(argv[2])
                SerialNo = (ULONG)atol(argv[2]);
            bUnplug = TRUE;
        }
        else if(tolower(argv[1][1]) == 'e') {
            if(argv[2])
                SerialNo = (ULONG)atol(argv[2]); 
            bEject = TRUE;
        }
        else {
            goto usage;
        }
    }
    else
        goto usage;

    if(bPlugIn && 0 == SerialNo)
        goto usage;
    //
    // Open a handle to the device interface information set of all 
    // present USB bus enumerator interfaces.
    //

    hardwareDeviceInfo = SetupDiGetClassDevs (
                       (LPGUID)&GUID_DEVINTERFACE_VUSB1,
                       NULL, // Define no enumerator (global)
                       NULL, // Define no
                       (DIGCF_PRESENT | // Only Devices present
                       DIGCF_DEVICEINTERFACE)); // Function class devices.

    if(INVALID_HANDLE_VALUE == hardwareDeviceInfo)
    {
        printf("SetupDiGetClassDevs failed: %x\n", GetLastError());
        return 0;
    }

    deviceInterfaceData.cbSize = sizeof (SP_DEVICE_INTERFACE_DATA);

    if (SetupDiEnumDeviceInterfaces (hardwareDeviceInfo,
                                 0, // No care about specific PDOs
                                 (LPGUID)&GUID_DEVINTERFACE_VUSB,
                                 0, //
                                 &deviceInterfaceData)) {

        OpenBusInterface(hardwareDeviceInfo, &deviceInterfaceData);
    } else if (ERROR_NO_MORE_ITEMS == GetLastError()) {
    
        printf(
        "Error:Interface GUID_DEVINTERFACE_VUSB is not registered\n");
    }

    SetupDiDestroyDeviceInfoList (hardwareDeviceInfo);
    return 0;
usage: 
    printf(USAGE);
    exit(0);
}

BOOLEAN
OpenBusInterface (
    IN       HDEVINFO                    HardwareDeviceInfo,
    IN       PSP_DEVICE_INTERFACE_DATA   DeviceInterfaceData
    )
{
    HANDLE                              file;
    PSP_DEVICE_INTERFACE_DETAIL_DATA    deviceInterfaceDetailData = NULL;
    ULONG                               predictedLength = 0;
    ULONG                               requiredLength = 0;
    ULONG                               bytes;
    VUSB_UNPLUG_HARDWARE             unplug;
    VUSB_EJECT_HARDWARE              eject;
    PVUSB_PLUGIN_HARDWARE            hardware;
    BOOLEAN                             bSuccess;

    //
    // Allocate a function class device data structure to receive the
    // information about this particular device.
    //
    
    SetupDiGetDeviceInterfaceDetail (
            HardwareDeviceInfo,
            DeviceInterfaceData,
            NULL, // probing so no output buffer yet
            0, // probing so output buffer length of zero
            &requiredLength,
            NULL); // not interested in the specific dev-node


    predictedLength = requiredLength;

    deviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA) malloc (predictedLength);

    if(deviceInterfaceDetailData) {
        deviceInterfaceDetailData->cbSize = 
                      sizeof (SP_DEVICE_INTERFACE_DETAIL_DATA);
    } else {
        printf("Couldn't allocate %d bytes for device interface details.\n", predictedLength);
        return FALSE;
    }

    
    if (! SetupDiGetDeviceInterfaceDetail (
               HardwareDeviceInfo,
               DeviceInterfaceData,
               deviceInterfaceDetailData,
               predictedLength,
               &requiredLength,
               NULL)) {
        printf("Error in SetupDiGetDeviceInterfaceDetail\n");
        free (deviceInterfaceDetailData);
        return FALSE;
    }

    printf("Opening %s\n", deviceInterfaceDetailData->DevicePath);

    file = CreateFile ( deviceInterfaceDetailData->DevicePath,
                        GENERIC_READ | GENERIC_WRITE,
                        0, // FILE_SHARE_READ | FILE_SHARE_WRITE
                        NULL, // no SECURITY_ATTRIBUTES structure
                        OPEN_EXISTING, // No special create flags
                        0, // No special attributes
                        NULL); // No template file

    if (INVALID_HANDLE_VALUE == file) {
        printf("Device not ready: %x", GetLastError());
        free (deviceInterfaceDetailData);
        return FALSE;
    }
    
    printf("Bus interface opened!!!\n");

    //
    // From this point on, we need to jump to the end of the routine for
    // common clean-up.  Keep track of whether we succeeded or failed, so
    // we'll know what to return to the caller.
    //
    bSuccess = FALSE;

    //
    // Enumerate Devices
    //

    if(bPlugIn) {

        printf("SerialNo./Password of the device to be enumerated: %d/%08X\n", SerialNo, Password);

        hardware =(PVUSB_PLUGIN_HARDWARE) malloc (bytes = (sizeof (VUSB_PLUGIN_HARDWARE) +
                                              BUS_HARDWARE_IDS_LENGTH));

        if(hardware) {
            hardware->Size = sizeof (VUSB_PLUGIN_HARDWARE);
            hardware->SerialNo = SerialNo;
            hardware->Password = Password;
        } else {
            printf("Couldn't allocate %d bytes for VUSB plugin hardware structure.\n", bytes);
            goto End;
        }

        //
        // Allocate storage for the Device ID
        //
        /*wchar_t *deviceHardwareID=L"USB\\VID_0529&PID_0001\0";
        wcscpy (hardware->HardwareIDs,
            deviceHardwareID);*/
        memcpy (hardware->HardwareIDs,
                BUS_HARDWARE_IDS,
                BUS_HARDWARE_IDS_LENGTH);

        if (!DeviceIoControl (file,
                              IOCTL_VUSB_PLUGIN_HARDWARE ,
                              hardware, bytes,
                              hardware, bytes,
                              &bytes, NULL)) {
              free (hardware);
              printf("PlugIn failed:0x%x\n", GetLastError());
              goto End;
        }

        free (hardware);
    }

    //
    // Removes a device if given the specific Id of the device. Otherwise this
    // ioctls removes all the devices that are enumerated so far.
    //
    
    if(bUnplug) {
        printf("Unplugging device(s)....\n");

        unplug.Size = bytes = sizeof (unplug);
        unplug.SerialNo = SerialNo;
        if (!DeviceIoControl (file,
                              IOCTL_VUSB_UNPLUG_HARDWARE,
                              &unplug, bytes,
                              &unplug, bytes,
                              &bytes, NULL)) {
            printf("Unplug failed: 0x%x\n", GetLastError());
            goto End;
        }
    }

    //
    // Ejects a device if given the specific Id of the device. Otherwise this
    // ioctls ejects all the devices that are enumerated so far.
    //

    if(bEject)
    {
        printf("Ejecting Device(s)\n");

        eject.Size = bytes = sizeof (eject);
        eject.SerialNo = SerialNo;
        if (!DeviceIoControl (file,
                              IOCTL_VUSB_EJECT_HARDWARE,
                              &eject, bytes,
                              &eject, bytes,
                              &bytes, NULL)) {
            printf("Eject failed: 0x%x\n", GetLastError());
            goto End;
        }
    }

    printf("Success!!!\n");
    bSuccess = TRUE;

End:
    CloseHandle(file);
    free (deviceInterfaceDetailData);
    return bSuccess;
}


