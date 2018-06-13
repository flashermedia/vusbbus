/*++

Copyright (c) 2004 Chingachguk & Denger2k All Rights Reserved

Module Name:

    USBKeyEmu.h

Abstract:

    This module contains the common private declarations 
    for the emulation of USB bus and HASP key

Environment:

    kernel mode only

Notes:


Revision History:


--*/

#ifndef USBKeyEmu_H
#define USBKeyEmu_H

//
// The generic ids for installation of key pdo
//
#define VUSB_COMPATIBLE_IDS L"USB\\VID_0529&PID_0001\0"
#define VUSB_COMPATIBLE_IDS_LENGTH sizeof(VUSB_COMPATIBLE_IDS)

//
// Automatic insert keys on startup of driver
//
#define INSERT_ALL_KEYS_ON_STARTUP 1

//
// Path to dumps in registry
//
#define DUMP_PATH_STR L"\\Registry\\MACHINE\\System\\CurrentControlSet\\Services\\Emulator\\HASP\\Dump"

//
// Path to log file
//
#define DEFAULT_LOG_FILE_NAME   L"\\??\\C:\\vusb.log"

//
// Description of key data
//
#pragma pack(1)
typedef struct _KEY_DATA {
    //
    // Current key state
    //
    UCHAR   isInitDone;     // Is chiperkeys given to key
    UCHAR   isKeyOpened;    // Is valid password is given to key
    UCHAR   encodedStatus;  // Last encoded status

    USHORT  chiperKey1,     // Keys for chiper
            chiperKey2;

    //
    // Static information about HASP key 
    //
    UCHAR   keyType;        // Type of key
    UCHAR   memoryType;     // Memory size of key
    ULONG   password;       // Password for key
    UCHAR   options[14];    // Options for key
    UCHAR   secTable[8];    // ST for key
    UCHAR   netMemory[16];  // NetMemory for key

    UCHAR   memory[512];    // Memory content
    UCHAR   edStruct[256];  // EDStruct for key
} KEY_DATA, *PKEYDATA;
#pragma pack()

//
// List of supported functions for HASP key
//
enum KEY_FN_LIST {
    KEY_FN_SET_CHIPER_KEYS          = 0x80,
    KEY_FN_CHECK_PASS               = 0x81,
    KEY_FN_READ_3WORDS              = 0x82,
    KEY_FN_WRITE_WORD               = 0x83,
    KEY_FN_READ_ST                  = 0x84,
    KEY_FN_READ_NETMEMORY_3WORDS    = 0x8B,
    KEY_FN_HASH_DWORD               = 0x98
};

//
// Status of operation with HASP key
//
enum KEY_OPERATION_STATUS {
    KEY_OPERATION_STATUS_OK                     = 0,
    KEY_OPERATION_STATUS_ERROR                  = 1,
    KEY_OPERATION_STATUS_INVALID_MEMORY_ADDRESS = 4,
    KEY_OPERATION_STATUS_LAST                   = 0x1F
};

//
// Structure for request to HASP key
//
#pragma pack(1)
typedef struct _KEY_REQUEST {
    UCHAR   majorFnCode;    // Requested fn number (type of KEY_FN_LIST)
    USHORT  param1,         // Key parameters
    param2, param3;
} KEY_REQUEST, *PKEY_REQUEST;

//
// Structure for respond of HASP key
//
typedef struct _KEY_RESPONSE {
    UCHAR           status,         // Status of operation (type of KEY_OPERATION_STATUS)
                    encodedStatus;  // CRC of status and majorFnCode
    UCHAR           data[16];       // Output data
} KEY_RESPONSE, *PKEY_RESPONSE;
#pragma pack()

//
// Defined in USBKeyEmu.c
//

NTSTATUS Bus_HandleUSBIoCtl(IN  PDEVICE_OBJECT  DeviceObject,
                            IN  PIRP            Irp);
NTSTATUS Bus_LoadDumpFromRegistry (ULONG password,
                                   PKEYDATA keyData);
NTSTATUS Bus_LoadDumpsFromRegistry(IN  PDEVICE_OBJECT DeviceObject);

void _Chiper(VOID *bufPtr,
             ULONG bufSize,
             PUSHORT key1Ptr,
             PUSHORT key2Ptr);
void Chiper(VOID *buf,
            ULONG size,
            PKEYDATA pKeyData);
void EmulateKey(PKEYDATA pKeyData,
                PKEY_REQUEST request,
                PULONG outBufLen,
                PKEY_RESPONSE outBuf);
LONG GetMemorySize(PKEYDATA pKeyData);
NTSTATUS GetRegKeyData(HANDLE hkey,
                       PWCHAR subKey,
                       VOID *data,
                       ULONG needSize);
#endif