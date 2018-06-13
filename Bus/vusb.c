/*++

Copyright (c) 2004 Chingachguk & Denger2k All Rights Reserved

Module Name:

    VUSB.C

Abstract:

    This module contains the entry points for a virtual USB bus driver.

Environment:

    kernel mode only

Revision History:


--*/

#include <stdarg.h>
#include <ntddk.h>
#include <stdio.h>
#include ".\Include\driver.h"
#include "vusb.h"

//
// Global Debug Level
//

ULONG VUsbDebugLevel = BUS_DEFAULT_DEBUG_OUTPUT_LEVEL;

GLOBALS Globals;

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, Bus_DriverUnload)
#pragma alloc_text (PAGE, Bus_CreateClose)
#pragma alloc_text (PAGE, Bus_IoCtl)
#ifdef DEBUG_FULL
#pragma alloc_text (PAGE, PrintBufferContent)
#pragma alloc_text (PAGE, LogMessage)
#endif
#endif

NTSTATUS
DriverEntry (
    IN  PDRIVER_OBJECT  DriverObject,
    IN  PUNICODE_STRING RegistryPath
    )
/*++
Routine Description:

    Initialize the driver dispatch table. 

Arguments:

    DriverObject - pointer to the driver object

    RegistryPath - pointer to a unicode string representing the path,
                   to driver-specific key in the registry.

Return Value:

  NT Status Code

--*/
{

    Bus_KdPrint_Def (BUS_DBG_SS_TRACE, ("Driver Entry\n"));

    //
    // Save the RegistryPath
    //

    Globals.RegistryPath.MaximumLength = RegistryPath->Length +
                                          sizeof(UNICODE_NULL);
    Globals.RegistryPath.Length = RegistryPath->Length;
    Globals.RegistryPath.Buffer = ExAllocatePoolWithTag(
                                       PagedPool,
                                       Globals.RegistryPath.MaximumLength,
                                       VUSB_POOL_TAG
                                       );    

    if (!Globals.RegistryPath.Buffer) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    RtlCopyUnicodeString(&Globals.RegistryPath, RegistryPath);

    //
    // Set entry points into the driver
    //
    DriverObject->MajorFunction [IRP_MJ_CREATE] =
    DriverObject->MajorFunction [IRP_MJ_CLOSE]  = Bus_CreateClose;
    DriverObject->MajorFunction [IRP_MJ_PNP]    = Bus_PnP;
    DriverObject->MajorFunction [IRP_MJ_POWER]  = Bus_Power;
    DriverObject->MajorFunction [IRP_MJ_DEVICE_CONTROL] = Bus_IoCtl;
    DriverObject->MajorFunction [IRP_MJ_INTERNAL_DEVICE_CONTROL] = Bus_HandleUSBIoCtl;
    DriverObject->DriverUnload = Bus_DriverUnload;
    DriverObject->DriverExtension->AddDevice = Bus_AddDevice;

    return STATUS_SUCCESS;
}

NTSTATUS
Bus_CreateClose (
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    )
/*++
Routine Description:

    Some outside source is trying to create a file against us.

    If this is for the FDO (the bus itself) then the caller 
    is trying to open the proprietary connection to tell us 
    to enumerate or remove a device.

    If this is for the PDO (an object on the bus) then this 
    is a client that wishes to use the VUSB device.
    
Arguments:

   DeviceObject - pointer to a device object.

   Irp - pointer to an I/O Request Packet.

Return Value:

   NT status code

--*/
{
    PIO_STACK_LOCATION  irpStack;
    NTSTATUS            status;
    PFDO_DEVICE_DATA    fdoData;

    PAGED_CODE ();

    fdoData = (PFDO_DEVICE_DATA) DeviceObject->DeviceExtension;

    status = STATUS_INVALID_DEVICE_REQUEST;
    Irp->IoStatus.Information = 0;
    
    //
    // If it's not for the FDO. We don't allow create/close on PDO
    // 
    if (fdoData->IsFDO) {
        
        //
        // Increment IO count on FDO
        //
        Bus_IncIoCount (fdoData);
        
        //
        // Check to see whether the bus is removed
        //
       
        if (fdoData->DevicePnPState == Deleted){         
            status = STATUS_DELETE_PENDING;
        } else {

            irpStack = IoGetCurrentIrpStackLocation (Irp);

            switch (irpStack->MajorFunction) {
            case IRP_MJ_CREATE:
                Bus_KdPrint_Def (BUS_DBG_SS_TRACE, ("Create \n"));
                status = STATUS_SUCCESS;
                break;

            case IRP_MJ_CLOSE:
                Bus_KdPrint_Def (BUS_DBG_SS_TRACE, ("Close \n"));
                status = STATUS_SUCCESS;
                break;
             default:
                break;
            } 
        }
        
    }

    Irp->IoStatus.Status = status;
    IoCompleteRequest (Irp, IO_NO_INCREMENT);

    //
    // Decrement IO count on FDO
    //
    if (fdoData->IsFDO)
        Bus_DecIoCount (fdoData);

    return status;
}

NTSTATUS
Bus_IoCtl (
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    )
/*++
Routine Description:

    Handle user mode PlugIn, UnPlug and device Eject requests.

Arguments:

   DeviceObject - pointer to a device object.

   Irp - pointer to an I/O Request Packet.

Return Value:

   NT status code

--*/
{
    PIO_STACK_LOCATION      irpStack;
    NTSTATUS                status;
    ULONG                   inlen, outlen;
    PFDO_DEVICE_DATA        fdoData;
    PVOID                   buffer;

    PAGED_CODE ();
    
    fdoData = (PFDO_DEVICE_DATA) DeviceObject->DeviceExtension;

    //
    // We only take Device Control requests for the FDO.
    // That is the bus itself.
    //

    if (!fdoData->IsFDO) {
    
        //
        // These commands are only allowed to go to the FDO.
        //   
        status = STATUS_INVALID_DEVICE_REQUEST;
        Irp->IoStatus.Status = status;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return status;

    }

    //
    // Check to see whether the bus is removed
    //
    
    if (fdoData->DevicePnPState == Deleted) {
        Irp->IoStatus.Status = status = STATUS_DELETE_PENDING;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return status;
    }
    
    Bus_IncIoCount (fdoData);
    
    irpStack = IoGetCurrentIrpStackLocation (Irp);

    buffer = Irp->AssociatedIrp.SystemBuffer;  
    inlen = irpStack->Parameters.DeviceIoControl.InputBufferLength;
    outlen = irpStack->Parameters.DeviceIoControl.OutputBufferLength;

    status = STATUS_INVALID_PARAMETER;
    
    switch (irpStack->Parameters.DeviceIoControl.IoControlCode) {
    case IOCTL_VUSB_PLUGIN_HARDWARE:
        if ((inlen == outlen) &&
            //
            // Make sure it has at least two nulls and the size 
            // field is set to the declared size of the struct
            //
            ((sizeof (VUSB_PLUGIN_HARDWARE) + sizeof(UNICODE_NULL) * 2) <=
             inlen) &&

            //
            // The size field should be set to the sizeof the struct as declared
            // and *not* the size of the struct plus the multi_sz
            //
            (sizeof (VUSB_PLUGIN_HARDWARE) ==
             ((PVUSB_PLUGIN_HARDWARE) buffer)->Size)) {

            Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("PlugIn called\n"));

            status= Bus_PlugInDevice((PVUSB_PLUGIN_HARDWARE)buffer,
                                inlen, fdoData);
            if (NT_SUCCESS (status))
                IoInvalidateDeviceRelations (fdoData->UnderlyingPDO, BusRelations);

            Irp->IoStatus.Information = outlen;

        }
        break;

    case IOCTL_VUSB_UNPLUG_HARDWARE:

        if ((sizeof (VUSB_UNPLUG_HARDWARE) == inlen) &&
            (inlen == outlen) &&
            (((PVUSB_UNPLUG_HARDWARE)buffer)->Size == inlen)) {

            Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("UnPlug called\n"));

            status= Bus_UnPlugDevice(
                    (PVUSB_UNPLUG_HARDWARE)buffer, fdoData);
            Irp->IoStatus.Information = outlen;

        }
        break;

    case IOCTL_VUSB_EJECT_HARDWARE:
    
        if ((sizeof (VUSB_EJECT_HARDWARE) == inlen) &&
            (inlen == outlen) &&
            (((PVUSB_EJECT_HARDWARE)buffer)->Size == inlen)) {

            Bus_KdPrint(fdoData, BUS_DBG_IOCTL_TRACE, ("Eject called\n"));

            status= Bus_EjectDevice((PVUSB_EJECT_HARDWARE)buffer, fdoData);
            Irp->IoStatus.Information = outlen;

        }
        break;   

    default:
        break; // default status is STATUS_INVALID_PARAMETER
    }

    Irp->IoStatus.Status = status;
    IoCompleteRequest (Irp, IO_NO_INCREMENT);
    Bus_DecIoCount (fdoData);
    return status;
}

VOID
Bus_DriverUnload (
    IN PDRIVER_OBJECT DriverObject
    )
/*++
Routine Description:
    Clean up everything we did in driver entry.

Arguments:

   DriverObject - pointer to this driverObject.


Return Value:

--*/
{
    PAGED_CODE ();

    Bus_KdPrint_Def (BUS_DBG_SS_TRACE, ("Unload\n"));
    
    //
    // All the device objects should be gone.
    //

    ASSERT (NULL == DriverObject->DeviceObject);

    //
    // Here we free all the resources allocated in the DriverEntry
    //

    if(Globals.RegistryPath.Buffer)
        ExFreePool(Globals.RegistryPath.Buffer);   
        
    return;
}

VOID
Bus_IncIoCount (
    IN  PFDO_DEVICE_DATA   FdoData
    )   

/*++

Routine Description:

    This routine increments the number of requests the device receives
    

Arguments:

    FdoData - pointer to the FDO device extension.
    
Return Value:

    VOID

--*/

{

    LONG            result;


    result = InterlockedIncrement(&FdoData->OutstandingIO);

    ASSERT(result > 0);
    //
    // Need to clear StopEvent (when OutstandingIO bumps from 1 to 2) 
    //
    if (result == 2) {
        //
        // We need to clear the event
        //
        KeClearEvent(&FdoData->StopEvent);
    }

    return;
}

VOID
Bus_DecIoCount(
    IN  PFDO_DEVICE_DATA  FdoData
    )   

/*++

Routine Description:

    This routine decrements as it complete the request it receives

Arguments:

    FdoData - pointer to the FDO device extension.
    
Return Value:

    VOID

--*/
{

    LONG            result;
    
    result = InterlockedDecrement(&FdoData->OutstandingIO);

    ASSERT(result >= 0);

    if (result == 1) {
        //
        // Set the stop event. Note that when this happens
        // (i.e. a transition from 2 to 1), the type of requests we 
        // want to be processed are already held instead of being 
        // passed away, so that we can't "miss" a request that
        // will appear between the decrement and the moment when
        // the value is actually used.
        //
 
        KeSetEvent (&FdoData->StopEvent, IO_NO_INCREMENT, FALSE);
        
    }
    
    if (result == 0) {

        //
        // The count is 1-biased, so it can be zero only if an 
        // extra decrement is done when a remove Irp is received 
        //

        ASSERT(FdoData->DevicePnPState == Deleted);

        //
        // Set the remove event, so the device object can be deleted
        //

        KeSetEvent (&FdoData->RemoveEvent, IO_NO_INCREMENT, FALSE);
        
    }

    return;
}

#ifdef DEBUG_FULL
void PrintBufferContent(OUT PWCHAR outStr, UCHAR *buf, LONG length) {
/*++
Routine Description:

   Print buffer content in hex format into string

Arguments:

   outStr - ptr to output string buffer

   buf - ptr to buffer

   length - length of buffer

Return Value:

   none

--*/
    PWCHAR tmpBuf;
    LONG i;
    // Check parameters
    if (!buf) {
       swprintf(outStr, L"NULL (ptr to data)");
       return;
    }
    if (length<=0) {
       swprintf(outStr, L"NULL (length)");
       return;
    }

    // Create string
    outStr[0]=0;
    outStr[1]=0;
    outStr[2]=0;
    outStr[3]=0;
    tmpBuf = ExAllocatePoolWithTag (PagedPool, 512, VUSB_POOL_TAG);
    if (!tmpBuf)
       return;
    for (i=0; i<length ; i++) {
        swprintf(tmpBuf, L"%02X ", (ULONG) ((UCHAR *)buf)[i]);
        wcscat(outStr, tmpBuf);                              
    }

    ExFreePool(tmpBuf);
}

NTSTATUS LogMessage(PCHAR szFormat, ...) {
/*++
Routine Description:

   Log msg into debug console and file

Arguments:

   szFormat - message format string

   ... - data

Return Value:

   NT status code

--*/
    ULONG Length;
    char messagebuf[1024];
    va_list va;
    IO_STATUS_BLOCK  IoStatus;
    OBJECT_ATTRIBUTES objectAttributes;
    NTSTATUS status;
    HANDLE FileHandle;
    UNICODE_STRING fileName;
    
    //format the string
    va_start(va,szFormat);
    _vsnprintf(messagebuf,sizeof(messagebuf),szFormat,va);
    va_end(va);
    
    //get a handle to the log file object
    fileName.Buffer = NULL;
    fileName.Length = 0;
    fileName.MaximumLength = sizeof(DEFAULT_LOG_FILE_NAME) + sizeof(UNICODE_NULL);
    fileName.Buffer = ExAllocatePool(PagedPool,
                                     fileName.MaximumLength);
    if (!fileName.Buffer) {
        DbgPrint ("LogMessageInFile: FAIL. ExAllocatePool Failed.\n");
        return FALSE;
    }
    RtlZeroMemory(fileName.Buffer, fileName.MaximumLength);
    status = RtlAppendUnicodeToString(&fileName, (PWSTR)DEFAULT_LOG_FILE_NAME);
     
    InitializeObjectAttributes (&objectAttributes,
                                (PUNICODE_STRING)&fileName,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                NULL );
    
    status = ZwCreateFile( &FileHandle,
                           FILE_APPEND_DATA,
                           &objectAttributes,
                           &IoStatus,
                           0, 
                           FILE_ATTRIBUTE_NORMAL,
                           FILE_SHARE_WRITE,
                           FILE_OPEN_IF,
                           FILE_SYNCHRONOUS_IO_NONALERT,
                           NULL,     
                           0 );
    
    if( NT_SUCCESS(status) ) {
        CHAR buf[1024];
        
        sprintf(buf,"%s",messagebuf);
        
        //format the string to make sure it appends a newline carrage-return to the 
        //end of the string.
        Length=strlen(buf);
        if( buf[Length-1]=='\n' ) {
            buf[Length-1]='\r';
            strcat(buf,"\n");
            Length++;
        } else {
            strcat(buf,"\r\n");
            Length+=2;
        }
        
        DbgPrint("%s", buf); 

        ZwWriteFile( FileHandle, NULL, NULL, NULL, &IoStatus, buf, Length, NULL, NULL );
        
        ZwClose( FileHandle );
    }

    if( fileName.Buffer ) {
        ExFreePool (fileName.Buffer);
    }

    return STATUS_SUCCESS;
}
#endif