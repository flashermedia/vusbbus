/*++

Copyright (c) 2004 Chingachguk & Denger2k All Rights Reserved

Module Name:

    USBKeyEmu.c

Abstract:

    This module contains routines for emulation of USB bus and USB HASP key.

Environment:

    kernel mode only

Revision History:


--*/

#include <stdarg.h>
#include <ntddk.h>
#include <usb.h>
#include <usbdrivr.h>
#include <stdio.h>
#include ".\Include\driver.h"
#include "vusb.h"
#include "EncDecSim.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, Bus_HandleUSBIoCtl)
#pragma alloc_text (PAGE, EmulateKey)
#pragma alloc_text (PAGE, _Chiper)
#pragma alloc_text (PAGE, Chiper)
#pragma alloc_text (PAGE, GetMemorySize)
#pragma alloc_text (PAGE, Bus_LoadDumpsFromRegistry)
#pragma alloc_text (PAGE, Bus_LoadDumpFromRegistry)
#endif

void _Chiper(VOID *bufPtr, ULONG bufSize, PUSHORT key1Ptr, PUSHORT key2Ptr) {
/*++
Routine Description:

   Encode/decode response/request to key

Arguments:

   bufPtr - pointer to a encoded/decoded data

   bufSize - size of encoded information

   key1Ptr, key2Ptr - ptr to chiper keys

Return Value:

   none

--*/
    if (bufSize) {
        __asm {
            pusha
            mov esi, [key1Ptr]
            mov edi, [key2Ptr]

        loc_12C6A:
            xor dl, dl
            mov ebx, 4

        loc_12C71:
            mov cx, [esi]
            add dl, dl
            test    cl, 1
            jz  loc_12C96
            mov ax, [edi]
            or  dl, 1
            xor ax, cx
            mov [esi], ax
            shr ax, 1
            mov [esi], ax
            or  ah, 80h
            mov [esi], ax
            jmp loc_12C9D

        loc_12C96:
            shr cx, 1
            mov [esi], cx

        loc_12C9D:
            add dl, dl
            test    byte ptr [esi], 80h
            jz  loc_12CA7
            or  dl, 1

        loc_12CA7:
            dec ebx
            jnz loc_12C71
            mov eax, [bufPtr]
            inc [bufPtr]
            xor [eax], dl
            dec [bufSize]
            jnz loc_12C6A
            popa
        }
    }
}


void Chiper(VOID *buf, ULONG size, PKEYDATA pKeyData) {
/*++
Routine Description:

   Encode/decode response/request to key (stub only)

Arguments:

   bufPtr - pointer to a encoded/decoded data

   bufSize - size of encoded information

   pKeyData - ptr to key data

Return Value:

   none

--*/
    LogMessage ("Chiper inChiperKey1=%08X, inChiperKey2=%08X, length=%X\n",
        pKeyData->chiperKey1, pKeyData->chiperKey2, size);
    _Chiper(buf, size, &pKeyData->chiperKey1, &pKeyData->chiperKey2);
    LogMessage ("Chiper outChiperKey1=%08X, outChiperKey2=%08X\n",
        pKeyData->chiperKey1, pKeyData->chiperKey2);
}

// Disable warn 'function changes ebp register'
#pragma warning(disable:4731)

ULONG CheckEncodedStatus(UCHAR AdjustedReqCode, UCHAR SetupKeysResult, UCHAR *BufPtr) {
/*++
Routine Description:

   Check encoded status (byte after status) for validity 

Arguments:

   AdjustedReqCode - pointer to ((requested fn code) & 7E)

   SetupKeysResult - data[0] response of key to 0x80 function

   BufPtr - ptr to status byte

Return Value:

   bool, 1 - encoded status ok, 0 - wrong encoded status

--*/
    ULONG loopCnt;
    __asm {
        push ebx
        push edx
        push ecx
        push esi
        cmp [AdjustedReqCode], 0
        jz  loc_12D33
        cmp [SetupKeysResult], 2
        jb  loc_12D33
        mov esi, [BufPtr]
        mov cl, [esi]
        cmp cl, 1Fh
        jbe loc_12D06
        xor eax, eax
        jmp loc_CheckEncodedStatus_exit

loc_12D06:              
        mov byte ptr [loopCnt], 0Fh
        lea eax, [loopCnt]
        push    eax
        push    ecx
        call    sub_12D50
        lea eax, [loopCnt]
        mov cl, [esi+1]
        push    eax
        push    ecx
        call    sub_12D50
        mov al, byte ptr [loopCnt]
        and al, 0Fh
        cmp al, 1
        sbb eax, eax
        neg eax
        jmp loc_CheckEncodedStatus_exit

loc_12D33:
        mov al, 0Fh
        mov esi, [BufPtr]
        cmp al, [esi]
        sbb eax, eax
        inc eax
        jmp loc_CheckEncodedStatus_exit

sub_12D50:
        push    ebp
        mov ecx, 7
        mov ebp, esp
        push    ebx
        push    esi
        mov al, [ebp+8]
        mov esi, [ebp+0Ch]

loc_12D60:
        mov dl, al
        mov bl, [esi]
        shr dl, cl
        and dl, 1
        add bl, bl
        or  dl, bl
        test    dl, 10h
        mov [esi], dl
        jz  loc_12D79
        xor dl, 0Dh
        mov [esi], dl

loc_12D79:
        and byte ptr [esi], 0Fh
        dec ecx
        jns loc_12D60
        pop esi
        pop ebx
        pop ebp
        retn    8
loc_CheckEncodedStatus_exit:
        pop  esi
        pop  ecx
        pop  edx
        pop  ebx
    }
}

#pragma warning(default:4731)

LONG GetMemorySize(PKEYDATA pKeyData) {
/*++
Routine Description:

   Compute memory size of key in bytes

Arguments:

   pKeyData - ptr to key data

Return Value:

   memory size of key in bytes

--*/
    if (pKeyData->memoryType==4)
        return 512-16;
    return 128-16;
}

void EmulateKey(PKEYDATA pKeyData, PKEY_REQUEST request, PULONG outBufLen, PKEY_RESPONSE outBuf) {
/*++
Routine Description:

   Emulation of key main procedure (IOCTL_INTERNAL_USB_SUBMIT_URB handler)

Arguments:

   pKeyData - ptr to key data

   request - ptr to request buffer

   outBufLen - ptr to out buffer size variable

   outBuf - ptr to out buffer

Return Value:

   none

--*/
    UCHAR encodeOutData, status, encodedStatus;
    ULONG outDataLen;
    KEY_RESPONSE    keyResponse;
    LARGE_INTEGER   time;
    
    //
    // Get time data, it used as random number source
    //
    KeQueryTickCount(&time);

    //
    // Setup answer
    //
    LogMessage ("Setup answer\n");
    RtlZeroMemory(&keyResponse, sizeof(keyResponse));
    keyResponse.status=KEY_OPERATION_STATUS_ERROR;
    outDataLen=0; encodeOutData=0;

    //
    // Analyse fn number
    //
    LogMessage ("Analyse fn number\n");
    switch (request->majorFnCode) {
        case KEY_FN_SET_CHIPER_KEYS:
            LogMessage ("KEY_FN_SET_CHIPER_KEYS\n");
            pKeyData->chiperKey1=request->param1;
            pKeyData->chiperKey2=0xA0CB;
            // Setup random encoded status begin value
            pKeyData->encodedStatus=(UCHAR)pKeyData;
            pKeyData->isInitDone=1;

            // Make key response
            keyResponse.status=KEY_OPERATION_STATUS_OK;
            keyResponse.data[0]=0x02;
            keyResponse.data[1]=0x0A;
            keyResponse.data[2]=0x00;
            // Bytes 3, 4 - key sn, set it to low word of ptr to key data
            keyResponse.data[3]=(UCHAR)(((USHORT)pKeyData) & 0xFF);
            keyResponse.data[4]=(UCHAR)((((USHORT)pKeyData) >> 8) & 0xFF);
            outDataLen=5;
            encodeOutData=1;                    
            break;

        case KEY_FN_CHECK_PASS:
            // Decode pass
            Chiper(&request->param1, 4, pKeyData);
            // Show request
            LogMessage ("KEY_FN_CHECK_PASS pass=%08X, pKeyData->password=%08X, pKeyData->isInitDone=%X\n",
                *((PULONG)&request->param1), pKeyData->password, pKeyData->isInitDone);
            // Compare pass
            if (*((PULONG)&request->param1)==pKeyData->password && pKeyData->isInitDone==1) {
                keyResponse.status=KEY_OPERATION_STATUS_OK;
                // data[0], data[1] - memory size
                keyResponse.data[0]=(UCHAR)((GetMemorySize(pKeyData)) & 0xFF);
                keyResponse.data[1]=(UCHAR)((GetMemorySize(pKeyData) >> 8) & 0xFF);
                keyResponse.data[2]=0x10;
                outDataLen=3;
                encodeOutData=1;
                // FN_OPEN_KEY
                pKeyData->isKeyOpened=1;
            }
            break;

        case KEY_FN_READ_NETMEMORY_3WORDS:
            LogMessage ("KEY_FN_READ_NETMEMORY\n");
            // Decode offset into NetMemory
            Chiper(&request->param1, 2, pKeyData);
            // Typical data into NetMemory:
            // 12 1A 12 0F 03 00 70 00 02 FF 00 00 FF FF FF FF
            // 12 1A 12 0F - sn
            // 03 00 - key type
            // 70 00 - memory size in bytes
            // 02 FF - ?
            // 00 00 - net user count
            // FF FF - ?
            // FF - key type (FF - local, FE - net, FD - time)
            // FF - ?
            // Analyse memory offset
            if (pKeyData->isKeyOpened && request->param1>=0 && request->param1<=7) {
                keyResponse.status=KEY_OPERATION_STATUS_OK;
                RtlCopyMemory(keyResponse.data, &pKeyData->netMemory[request->param1*2], sizeof(USHORT)*3);
                outDataLen=sizeof(USHORT)*3;
                encodeOutData=1;
            }            
            break;

        case KEY_FN_READ_3WORDS:
            LogMessage ("KEY_FN_READ_3WORDS\n");
            // Decode memory offset
            Chiper(&request->param1, 2, pKeyData);
            // Do read
            if (pKeyData->isKeyOpened && request->param1>=0 && (request->param1*2)<GetMemorySize(pKeyData)) {
                keyResponse.status=KEY_OPERATION_STATUS_OK;
                RtlCopyMemory(keyResponse.data, &pKeyData->memory[request->param1*2], sizeof(USHORT)*3);
                outDataLen=sizeof(USHORT)*3;
                encodeOutData=1;
            }
            break;

        case KEY_FN_WRITE_WORD:
            LogMessage ("KEY_FN_WRITE_WORD\n");
            // Decode memory offset & value
            Chiper(&request->param1, 4, pKeyData);
            LogMessage ("offset=%X data=%X\n", request->param1, request->param2);
            // Do write
            if (pKeyData->isKeyOpened && request->param1>=0 && (request->param1*2)<GetMemorySize(pKeyData)) {
                keyResponse.status=KEY_OPERATION_STATUS_OK;
                RtlCopyMemory(&pKeyData->memory[request->param1*2], &request->param2, sizeof(USHORT));
                outDataLen=0;
                encodeOutData=0;
            }
            break;

        case KEY_FN_READ_ST:
            LogMessage ("KEY_FN_READ_ST\n");
            // Do read ST
            if (pKeyData->isKeyOpened) {
                LONG i;
                keyResponse.status=KEY_OPERATION_STATUS_OK;
                for (i=7; i>=0; i--) 
                    keyResponse.data[7-i]=pKeyData->secTable[i];
                outDataLen=8;
                encodeOutData=1;
            }
            break;

        case KEY_FN_HASH_DWORD:
            LogMessage ("KEY_FN_HASH_DWORD\n");
            // Decode dword
            Chiper(&request->param1, 4, pKeyData);
            // Do hash dword
            if (pKeyData->isKeyOpened) {
                keyResponse.status=KEY_OPERATION_STATUS_OK;
                RtlCopyMemory(keyResponse.data, &request->param1, 4);
                HashDWORD((DWORD *)keyResponse.data, pKeyData->edStruct);
                outDataLen=sizeof(ULONG);
                encodeOutData=1;
            }
            break;
    }

    //
    // Return results
    //

    // Create encodedStatus
    LogMessage ("Create encodedStatus\n");
    // Randomize encodedStatus
    pKeyData->encodedStatus^=(UCHAR)time.LowPart;
    // If status in range KEY_OPERATION_STATUS_OK...KEY_OPERATION_STATUS_LAST
    if (keyResponse.status>=KEY_OPERATION_STATUS_OK && keyResponse.status<=KEY_OPERATION_STATUS_LAST)
        // Then create encoded status
        do {
            keyResponse.encodedStatus=++pKeyData->encodedStatus;
        } while (CheckEncodedStatus(request->majorFnCode & 0x7F, 0x02, &keyResponse.status)==0);

    // Store encoded status
    status=keyResponse.status;
    encodedStatus=keyResponse.encodedStatus;
    LogMessage ("Encoded status: %02X\n", encodedStatus);

    // Crypt status & encoded status 
    Chiper(&keyResponse.status, 2, pKeyData);

    // Crypt data
    if (encodeOutData)
        Chiper(&keyResponse.data, outDataLen, pKeyData);

    // Shuffle encoding keys
    if (status==0) {
        pKeyData->chiperKey2=(pKeyData->chiperKey2 & 0xFF) | (encodedStatus << 8);
        LogMessage ("Shuffle keys: chiperKey1=%08X, chiperKey2=%08X,\n",
            pKeyData->chiperKey1, pKeyData->chiperKey2);
    }

    // Set out data size
    *outBufLen=min(sizeof(USHORT)+outDataLen, *outBufLen);
    LogMessage ("Out data size: %X\n", *outBufLen);

    // Copy data into out buffer
    RtlCopyMemory(outBuf, &keyResponse, *outBufLen);  
}

#ifdef DEBUG_FULL
//
// USB function codes to description string conversion list
//
static WCHAR *fnCodeList[] = {
        L"URB_FUNCTION_SELECT_CONFIGURATION",
        L"URB_FUNCTION_SELECT_INTERFACE",
        L"URB_FUNCTION_ABORT_PIPE",
        L"URB_FUNCTION_TAKE_FRAME_LENGTH_CONTROL",
        L"URB_FUNCTION_RELEASE_FRAME_LENGTH_CONTROL",
        L"URB_FUNCTION_GET_FRAME_LENGTH",
        L"URB_FUNCTION_SET_FRAME_LENGTH",
        L"URB_FUNCTION_GET_CURRENT_FRAME_NUMBER",
        L"URB_FUNCTION_CONTROL_TRANSFER",
        L"URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER",
        L"URB_FUNCTION_ISOCH_TRANSFER",
        L"URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE",
        L"URB_FUNCTION_SET_DESCRIPTOR_TO_DEVICE",
        L"URB_FUNCTION_SET_FEATURE_TO_DEVICE",
        L"URB_FUNCTION_SET_FEATURE_TO_INTERFACE",
        L"URB_FUNCTION_SET_FEATURE_TO_ENDPOINT",
        L"URB_FUNCTION_CLEAR_FEATURE_TO_DEVICE",
        L"URB_FUNCTION_CLEAR_FEATURE_TO_INTERFACE",
        L"URB_FUNCTION_CLEAR_FEATURE_TO_ENDPOINT",
        L"URB_FUNCTION_GET_STATUS_FROM_DEVICE",
        L"URB_FUNCTION_GET_STATUS_FROM_INTERFACE",
        L"URB_FUNCTION_GET_STATUS_FROM_ENDPOINT",
        L"URB_FUNCTION_RESERVED_0X0016",
        L"URB_FUNCTION_VENDOR_DEVICE",
        L"URB_FUNCTION_VENDOR_INTERFACE",
        L"URB_FUNCTION_VENDOR_ENDPOINT",
        L"URB_FUNCTION_CLASS_DEVICE",
        L"URB_FUNCTION_CLASS_INTERFACE",
        L"URB_FUNCTION_CLASS_ENDPOINT",
        L"URB_FUNCTION_RESERVE_0X001D",
        L"URB_FUNCTION_SYNC_RESET_PIPE_AND_CLEAR_STALL",
        L"URB_FUNCTION_CLASS_OTHER",
        L"URB_FUNCTION_VENDOR_OTHER",
        L"URB_FUNCTION_GET_STATUS_FROM_OTHER",
        L"URB_FUNCTION_CLEAR_FEATURE_TO_OTHER",
        L"URB_FUNCTION_SET_FEATURE_TO_OTHER",
        L"URB_FUNCTION_GET_DESCRIPTOR_FROM_ENDPOINT",
        L"URB_FUNCTION_SET_DESCRIPTOR_TO_ENDPOINT",
        L"URB_FUNCTION_GET_CONFIGURATION",
        L"URB_FUNCTION_GET_INTERFACE",
        L"URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE",
        L"URB_FUNCTION_SET_DESCRIPTOR_TO_INTERFACE",
        L"URB_FUNCTION_GET_MS_FEATURE_DESCRIPTOR",
        L"URB_FUNCTION_RESERVE_0X002B",
        L"URB_FUNCTION_RESERVE_0X002C",
        L"URB_FUNCTION_RESERVE_0X002D",
        L"URB_FUNCTION_RESERVE_0X002E",
        L"URB_FUNCTION_RESERVE_0X002F",
        L"URB_FUNCTION_SYNC_RESET_PIPE",
        L"URB_FUNCTION_SYNC_CLEAR_STALL",
};
#endif

NTSTATUS
Bus_HandleUSBIoCtl (
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
    ULONG                   inlen, outlen, i;
    PVOID                   buffer;
    PWCHAR                  str1, str2;
    PURB                    urb;
    PPDO_DEVICE_DATA        pdoData;

    NTSTATUS                status1;
    HANDLE                  FileHandle;
    IO_STATUS_BLOCK         ioStatusBlock;
    
    PAGED_CODE ();
    
    pdoData = (PPDO_DEVICE_DATA) DeviceObject->DeviceExtension;
      
    Bus_KdPrint(pdoData, BUS_DBG_IOCTL_TRACE, ("Recive IRP_MJ_INTERNAL_DEVICE_CONTROL\n"));

    //
    // We only take Device Control requests for the devices.
    //

    if (pdoData->IsFDO) {
    
        //
        // These commands are only allowed to go to the devices.
        //   
        status = STATUS_INVALID_DEVICE_REQUEST;
        Irp->IoStatus.Status = status;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return status;

    }

    //
    // Check to see whether the bus is removed
    //
    
    if (pdoData->DevicePnPState == Deleted) {
        Irp->IoStatus.Status = status = STATUS_DELETE_PENDING;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return status;
    }

    //
    // Get IRP packet info
    //
    irpStack = IoGetCurrentIrpStackLocation (Irp);

    buffer = Irp->AssociatedIrp.SystemBuffer;  
    inlen  = irpStack->Parameters.DeviceIoControl.InputBufferLength;
    outlen = irpStack->Parameters.DeviceIoControl.OutputBufferLength;

    //
    // Get URB
    // 
    urb = irpStack->Parameters.Others.Argument1;

    //
    // And set status to 'unhandled device request'
    //
    status = STATUS_INVALID_DEVICE_REQUEST;
    
    //
    // Analyse requested IoControlCode
    //
    switch (irpStack->Parameters.DeviceIoControl.IoControlCode) {
        //
        // Request for USB bus, handle it
        //
        case IOCTL_INTERNAL_USB_SUBMIT_URB:
            Bus_KdPrint(pdoData, BUS_DBG_IOCTL_TRACE, ("Recive IOCTL_INTERNAL_USB_SUBMIT_URB\n"));
            if (urb) {
                #ifdef DEBUG_FULL
                //
                // Print request info
                //
                str1 = ExAllocatePoolWithTag (PagedPool, 512, VUSB_POOL_TAG);
                if (!str1) {
                   status = STATUS_INSUFFICIENT_RESOURCES;
                   break;
                }
                str2 = ExAllocatePoolWithTag (PagedPool, 512, VUSB_POOL_TAG);
                if (!str2) {
                   ExFreePool(str1);
                   status = STATUS_INSUFFICIENT_RESOURCES;
                   break;
                }

                if (urb->UrbHeader.Function==URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE || urb->UrbHeader.Function==URB_FUNCTION_VENDOR_DEVICE) {
                    PrintBufferContent(str1,
                            urb->UrbControlVendorClassRequest.TransferBuffer,
                            urb->UrbControlVendorClassRequest.TransferBufferLength);
                    PrintBufferContent(str2,
                            &urb->UrbControlVendorClassRequest.Request,
                            1+2+2+2);
                    LogMessage ("Bus_HandleUSBIoCtl(): in\n"
                             "\tFunction:  %ws (%X)\n"
                             "\tLength:    %X\n"
                             "\tTransfer buffer length:     %X\n"
                             "\tTransfer buffer contents:   %ws\n"
                             "\tRequest buffer:             %ws\n"
                             "\tRequest:        %X\n"
                             "\tValue:          %X\n"
                             "\tIndex:          %X\n"
                             "\tTransferFlags:  %X\n"
                             "\tDescriptorType: %X\n"
                             "\tLanguageId:     %X\n",
                            (urb->UrbHeader.Function>=0 && urb->UrbHeader.Function<=0x31)?fnCodeList[urb->UrbHeader.Function]:L"UNKNOWN\0",
                            urb->UrbHeader.Function,
                            urb->UrbHeader.Length,
                            urb->UrbControlVendorClassRequest.TransferBufferLength,
                            str1,
                            str2,
                            urb->UrbControlVendorClassRequest.Request,
                            urb->UrbControlVendorClassRequest.Value,
                            urb->UrbControlVendorClassRequest.Index,
                            urb->UrbControlVendorClassRequest.TransferFlags,
                            urb->UrbControlDescriptorRequest.DescriptorType,
                            urb->UrbControlDescriptorRequest.LanguageId
                    );
                } else
                    LogMessage ("Bus_HandleUSBIoCtl(): in\n"
                            "\tFunction:  %ws (%X)\n"
                            "\tLength:    %X\n",
                            (urb->UrbHeader.Function>=0 && urb->UrbHeader.Function<=0x31)?fnCodeList[urb->UrbHeader.Function]:L"UNKNOWN\0",
                            urb->UrbHeader.Function,
                            urb->UrbHeader.Length
                    );
                #endif

                // Analyse requested URB function code
                switch (urb->UrbHeader.Function) {
                    //
                    // Get info about device fn
                    //
                    case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
                        switch (urb->UrbControlDescriptorRequest.DescriptorType) {
                            //
                            // Info about hardware of USB device
                            //
                            case USB_DEVICE_DESCRIPTOR_TYPE: {
                                    USB_DEVICE_DESCRIPTOR deviceDesc;
                                    deviceDesc.bLength=sizeof(deviceDesc);
                                    deviceDesc.bDescriptorType=USB_DEVICE_DESCRIPTOR_TYPE;
                                    deviceDesc.bcdUSB=0x100;
                                    deviceDesc.bDeviceClass=USB_DEVICE_CLASS_VENDOR_SPECIFIC;
                                    deviceDesc.bDeviceSubClass=0;
                                    deviceDesc.bDeviceProtocol=0;
                                    deviceDesc.bMaxPacketSize0=8;
                                    deviceDesc.idVendor=0x529;
                                    deviceDesc.idProduct=1;
                                    deviceDesc.bcdDevice=0x100;
                                    deviceDesc.iManufacturer=1;
                                    deviceDesc.iProduct=2;
                                    deviceDesc.iSerialNumber=0;
                                    deviceDesc.bNumConfigurations=1;

                                    urb->UrbControlVendorClassRequest.TransferBufferLength=
                                            min(urb->UrbControlVendorClassRequest.TransferBufferLength, sizeof(deviceDesc));

                                    RtlCopyMemory(urb->UrbControlVendorClassRequest.TransferBuffer,
                                        &deviceDesc,
                                        urb->UrbControlVendorClassRequest.TransferBufferLength
                                    );

                                    status = STATUS_SUCCESS;
                                    URB_STATUS(urb) = USBD_STATUS_SUCCESS;
                                }
                                break;

                            //
                            // Info about possible configurations of USB device
                            //
                            case USB_CONFIGURATION_DESCRIPTOR_TYPE: {
                                    struct  {
                                        USB_CONFIGURATION_DESCRIPTOR configDesc;
                                        USB_INTERFACE_DESCRIPTOR interfaceDesc;
                                    } configInfo;
                                    configInfo.configDesc.bLength=sizeof(configInfo.configDesc);
                                    configInfo.configDesc.bDescriptorType=USB_CONFIGURATION_DESCRIPTOR_TYPE;
                                    configInfo.configDesc.wTotalLength=sizeof(configInfo.configDesc)+sizeof(configInfo.interfaceDesc);
                                    configInfo.configDesc.bNumInterfaces=1;
                                    configInfo.configDesc.bConfigurationValue=1;
                                    configInfo.configDesc.iConfiguration=0;
                                    configInfo.configDesc.bmAttributes=USB_CONFIG_BUS_POWERED;
                                    configInfo.configDesc.MaxPower=54/2;

                                    configInfo.interfaceDesc.bLength=sizeof(configInfo.interfaceDesc);
                                    configInfo.interfaceDesc.bDescriptorType=USB_INTERFACE_DESCRIPTOR_TYPE;
                                    configInfo.interfaceDesc.bInterfaceNumber=0;
                                    configInfo.interfaceDesc.bAlternateSetting=0;
                                    configInfo.interfaceDesc.bNumEndpoints=0;
                                    configInfo.interfaceDesc.bInterfaceClass=USB_DEVICE_CLASS_VENDOR_SPECIFIC;
                                    configInfo.interfaceDesc.bInterfaceSubClass=0;
                                    configInfo.interfaceDesc.bInterfaceProtocol=0;
                                    configInfo.interfaceDesc.iInterface=0;

                                    urb->UrbControlVendorClassRequest.TransferBufferLength=
                                        min(urb->UrbControlVendorClassRequest.TransferBufferLength, sizeof(configInfo.configDesc)+sizeof(configInfo.interfaceDesc));

                                    RtlCopyMemory(urb->UrbControlVendorClassRequest.TransferBuffer,
                                        &configInfo,
                                        urb->UrbControlVendorClassRequest.TransferBufferLength
                                    );

                                    status = STATUS_SUCCESS;
                                    URB_STATUS(urb) = USBD_STATUS_SUCCESS;
                                }
                                break;
                        }
                        break;

                    //
                    // Select configurations of USB device
                    //
                    case URB_FUNCTION_SELECT_CONFIGURATION: {
                            typedef struct _URB_SELECT_CONFIGURATION *PURB_SELECT_CONFIGURATION;
                            PURB_SELECT_CONFIGURATION selectConfig;
                            selectConfig=(PURB_SELECT_CONFIGURATION)urb;
                            selectConfig->Interface.InterfaceNumber=0;
                            selectConfig->Interface.AlternateSetting=0;
                            selectConfig->Interface.Class=USB_DEVICE_CLASS_VENDOR_SPECIFIC;
                            selectConfig->Interface.SubClass=0;
                            selectConfig->Interface.Protocol=0;

                            status = STATUS_SUCCESS;
                            URB_STATUS(urb) = USBD_STATUS_SUCCESS;
                        }                                          
                        break;

                    //
                    // IO request to USB hardware
                    // 
                    case URB_FUNCTION_VENDOR_DEVICE: {
                            // Emulate request to key
                            EmulateKey(
                                &pdoData->keyData,
                                (PKEY_REQUEST)&urb->UrbControlVendorClassRequest.Request,
                                &urb->UrbControlVendorClassRequest.TransferBufferLength,
                                (PKEY_RESPONSE)urb->UrbControlVendorClassRequest.TransferBuffer
                            );
                            // I/o with key is succeful
                            status = STATUS_SUCCESS;
                            URB_STATUS(urb) = USBD_STATUS_SUCCESS;
                        } break;
                }

                #ifdef DEBUG_FULL
                //
                // Print request result
                //
                if (urb->UrbHeader.Function==URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE || urb->UrbHeader.Function==URB_FUNCTION_VENDOR_DEVICE) {
                    PrintBufferContent(str1,
                            urb->UrbControlVendorClassRequest.TransferBuffer,
                            urb->UrbControlVendorClassRequest.TransferBufferLength);
                    LogMessage ("Bus_HandleUSBIoCtl(): out\n"
                            "\tTransfer buffer length:     %X\n"
                            "\tTransfer buffer contents:   %ws\n"
                            "\tStatus:                     %X\n",
                            urb->UrbControlVendorClassRequest.TransferBufferLength,
                            str1,
                            URB_STATUS(urb) 
                    );
                } else
                    LogMessage ("Bus_HandleUSBIoCtl(): out\n"
                             "\tStatus: %X\n",
                             URB_STATUS(urb) 
                    );

                ExFreePool(str2);
                ExFreePool(str1);
                #endif
            }
            break;

        default:
            break; // default status is STATUS_INVALID_PARAMETER
    }

    Irp->IoStatus.Status = status;
    IoCompleteRequest (Irp, IO_NO_INCREMENT);
    return status;
}

static WCHAR DUMP_PATH[] = DUMP_PATH_STR;

NTSTATUS
Bus_LoadDumpsFromRegistry(
    IN  PDEVICE_OBJECT          DeviceObject
    )
/*++

Routine Description:

    This routine enum dumps of HASP keys in registry and load it

Arguments:

    DeviceObject        A device object in the stack in which we attach HASP keys

Return Value:

    NTSTATUS

--*/
{
    //
    // Search for HASP key dumps in registry
    // and attach it as USB devices
    //
    NTSTATUS        status;
    HANDLE          hkey;
    ULONG           keysAttached;
    WCHAR           path[128];
    UNICODE_STRING  usPath;
    OBJECT_ATTRIBUTES   oa;
    PFDO_DEVICE_DATA    fdoData;

    PAGED_CODE();

    fdoData = (PFDO_DEVICE_DATA) DeviceObject->DeviceExtension;
    Bus_IncIoCount (fdoData);

    keysAttached=0;

    RtlInitUnicodeString(&usPath, DUMP_PATH);
    InitializeObjectAttributes(&oa, &usPath, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);

    //
    // Enum keys in registry and attach it
    //
    status = ZwOpenKey(&hkey, KEY_READ, &oa);
    if (NT_SUCCESS(status)) {
        ULONG size, i;
        PKEY_FULL_INFORMATION fip;

        ZwQueryKey(hkey, KeyFullInformation, NULL, 0, &size);
        size = min(size, PAGE_SIZE);
        fip = (PKEY_FULL_INFORMATION) ExAllocatePool(PagedPool, size);
        ZwQueryKey(hkey, KeyFullInformation, fip, size, &size);

        for (i = 0; i < fip->SubKeys; ++i) {
            ULONG curChar, password, isPasswordValid;
            PKEY_BASIC_INFORMATION bip;

            ZwEnumerateKey(hkey, i, KeyBasicInformation, NULL, 0, &size);
            size = min(size, PAGE_SIZE);
            bip = (PKEY_BASIC_INFORMATION)ExAllocatePool(PagedPool, size);
            ZwEnumerateKey(hkey, i, KeyBasicInformation, bip, size, &size);

            // Convert key name into password
            password=0; isPasswordValid=1;
            for (curChar=0; curChar<bip->NameLength/2; curChar++) {
                if (bip->Name[curChar]>=L'0' && bip->Name[curChar]<=L'9')
                    password=(password << 4) + (bip->Name[curChar] - L'0');
                else
                    if (bip->Name[curChar]>=L'A' && bip->Name[curChar]<=L'F')
                        password=(password << 4) + (bip->Name[curChar] - L'A') + 10;
                    else
                        if (bip->Name[curChar]>=L'a' && bip->Name[curChar]<=L'f')
                            password=(password << 4) + (bip->Name[curChar] - L'a') + 10;
                        else {
                            isPasswordValid=0;
                            break;
                        }
            }
            if (isPasswordValid && INSERT_ALL_KEYS_ON_STARTUP) {
                UCHAR                       buf[sizeof(VUSB_PLUGIN_HARDWARE)+BUS_HARDWARE_IDS_LENGTH];
                PVUSB_PLUGIN_HARDWARE    pOneKeyInfo;
                NTSTATUS                    status;
                
                LogMessage("Found key %08X\n", password); 

                pOneKeyInfo=(PVUSB_PLUGIN_HARDWARE)buf;
                pOneKeyInfo->Size=sizeof(VUSB_PLUGIN_HARDWARE);
                pOneKeyInfo->SerialNo=++keysAttached;
                pOneKeyInfo->Password=password;
                RtlCopyMemory(pOneKeyInfo->HardwareIDs, BUS_HARDWARE_IDS, BUS_HARDWARE_IDS_LENGTH);

                status=Bus_PlugInDevice(pOneKeyInfo, sizeof(VUSB_PLUGIN_HARDWARE)+BUS_HARDWARE_IDS_LENGTH, fdoData); 
                if (!NT_SUCCESS (status))
                    keysAttached--;
            } else
                LogMessage("Found bad key\n"); 
            
            ExFreePool(bip);
        }

        ExFreePool(fip);

        ZwClose(hkey);
        
        Bus_DecIoCount (fdoData);

        //if (keysAttached>0)
        //    IoInvalidateDeviceRelations (FdoData->UnderlyingPDO, BusRelations);

        return STATUS_SUCCESS;
    }
    return STATUS_SUCCESS;
}

NTSTATUS
GetRegKeyData(
    HANDLE hkey,
    PWCHAR subKey,
    VOID *data,
    ULONG needSize)
/*++

Routine Description:

    This routine read data from registry key

Arguments:

    hkey        Ptr to registry key
    subKey      Name of subkey
    data        Ptr to buffer, in which we read information
    needSize    Size of buffer

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS    status;
    ULONG       size;
    UNICODE_STRING valname;
    PKEY_VALUE_PARTIAL_INFORMATION vpip;

    RtlInitUnicodeString(&valname, subKey);
    size=0;
    status = ZwQueryValueKey(hkey, &valname, KeyValuePartialInformation, NULL, 0, &size);
    if (status == STATUS_OBJECT_NAME_NOT_FOUND && size < needSize && size<=PAGE_SIZE)
        return STATUS_INVALID_PARAMETER;
    vpip = (PKEY_VALUE_PARTIAL_INFORMATION) ExAllocatePool(PagedPool, size);
    if (!vpip)
        return STATUS_INSUFFICIENT_RESOURCES;
    status = ZwQueryValueKey(hkey, &valname, KeyValuePartialInformation,
      vpip, size, &size);
    if (!NT_SUCCESS(status))
        return STATUS_INVALID_PARAMETER;
    RtlCopyMemory(data, vpip->Data, needSize);
    ExFreePool(vpip);
    return STATUS_SUCCESS;
}

//
// HASP secret table generator vectors, it used to create ST for old HASP keys 
//
static UCHAR HASP_rows[8][8]={
    {0,0,0,0,0,0,0,0},
    {0,1,0,1,0,1,0,1},
    {1,0,1,0,1,0,1,0},
    {0,0,1,1,0,0,1,1},
    {1,1,0,0,1,1,0,0},
    {0,0,0,0,1,1,1,1},
    {1,1,1,1,0,0,0,0},
    {1,1,1,1,1,1,1,1}
};

NTSTATUS
Bus_LoadDumpFromRegistry(
    ULONG password,
    PKEYDATA keyData)
/*++

Routine Description:

    This routine read dump of one HASP key from registry

Arguments:

    password    Password of key
    keyData     Ptr to structure with key data

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS        status;
    HANDLE          hkey;
    WCHAR           path[128];
    UNICODE_STRING  usPath;
    OBJECT_ATTRIBUTES oa;

    RtlZeroMemory(keyData, sizeof(keyData));

    //
    // Try to read data about key from registry
    //
    swprintf(path, L"%ws\\%08X", DUMP_PATH, password);
    RtlInitUnicodeString(&usPath, path);

    InitializeObjectAttributes(&oa, &usPath, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);
    
    status = ZwOpenKey(&hkey, KEY_READ, &oa);
    if (NT_SUCCESS(status)) {
        NTSTATUS readStatus, hasOption, hasSecTable; readStatus=STATUS_SUCCESS;
        keyData->password=(password >> 16) | (password << 16); // Password stored in shuffled form
        readStatus+=GetRegKeyData(hkey, L"Type", &keyData->keyType, sizeof(keyData->keyType));
        readStatus+=GetRegKeyData(hkey, L"Memory", &keyData->memoryType, sizeof(keyData->memoryType));
        readStatus+=GetRegKeyData(hkey, L"SN", &keyData->netMemory[0], sizeof(ULONG));
        readStatus+=GetRegKeyData(hkey, L"Data", keyData->memory, min(GetMemorySize(keyData), sizeof(keyData->memory)));

        if (NT_SUCCESS(readStatus)) {
            hasOption=GetRegKeyData(hkey, L"Option", keyData->options, sizeof(keyData->options));
            hasSecTable=GetRegKeyData(hkey, L"SecTable", keyData->secTable, sizeof(keyData->secTable));
            if (!(NT_SUCCESS(hasOption) && NT_SUCCESS(hasSecTable) && keyData->options[0]==1) ||
                    !(!NT_SUCCESS(hasOption) && NT_SUCCESS(hasSecTable))) {
                // Universal ST case
                ULONG   password=keyData->password, i, j;
                UCHAR   alBuf[8];

                LogMessage("Revert to universal ST\n");

                password^=0x09071966;

                for (i=0; i<8; i++) {
                    alBuf[i]= (UCHAR)(password & 7);
                    password = password >> 3;
                }

                RtlZeroMemory(keyData->secTable, sizeof(keyData->secTable));

                for(i=0; i<8; i++)
                    for(j=0; j<8; j++)
                        keyData->secTable[j]|=HASP_rows[alBuf[i]][j] << (7 - i);
            }
            if (GetRegKeyData(hkey, L"NetMemory", &keyData->netMemory[4], sizeof(keyData->netMemory)-sizeof(ULONG))!=STATUS_SUCCESS) {
                RtlFillMemory(&keyData->netMemory[4], sizeof(keyData->netMemory)-sizeof(ULONG), 0xFF);
                if (keyData->memoryType==4) {
                    // Unlimited Net key
                    keyData->netMemory[6+4]=0xFF;
                    keyData->netMemory[7+4]=0xFF;
                    keyData->netMemory[10+4]=0xFE;
                } else {
                    // Local key
                    keyData->netMemory[6+4]=0;
                    keyData->netMemory[7+4]=0;
                    keyData->netMemory[10+4]=0;
                }
            }
            GetRegKeyData(hkey, L"EDStruct", keyData->edStruct, sizeof(keyData->edStruct));
        }
        ZwClose(hkey);
        if (!NT_SUCCESS(readStatus))
            return STATUS_INVALID_PARAMETER;
    } else
        return STATUS_INVALID_PARAMETER;
    return STATUS_SUCCESS;
}