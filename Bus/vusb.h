/*++

Copyright (c) 2004 Chingachguk & Denger2k All Rights Reserved

Module Name:

    VUSB.H

Abstract:

    This module contains the common private declarations 
    for the virtual USB Bus enumerator.

Environment:

    kernel mode only

Notes:


Revision History:


--*/
#ifndef VUSB_H
#define VUSB_H
#include "USBKeyEmu.h"

//
// Memory allocation tag
//
#define VUSB_POOL_TAG (ULONG) 'bsUV'

//
// Debugging Output Levels
//

#define BUS_DBG_ALWAYS                  0x00000000

#define BUS_DBG_STARTUP_SHUTDOWN_MASK   0x0000000F
#define BUS_DBG_SS_NOISE                0x00000001
#define BUS_DBG_SS_TRACE                0x00000002
#define BUS_DBG_SS_INFO                 0x00000004
#define BUS_DBG_SS_ERROR                0x00000008

#define BUS_DBG_PNP_MASK                0x000000F0
#define BUS_DBG_PNP_NOISE               0x00000010
#define BUS_DBG_PNP_TRACE               0x00000020
#define BUS_DBG_PNP_INFO                0x00000040
#define BUS_DBG_PNP_ERROR               0x00000080

#define BUS_DBG_IOCTL_MASK              0x00000F00
#define BUS_DBG_IOCTL_NOISE             0x00000100
#define BUS_DBG_IOCTL_TRACE             0x00000200
#define BUS_DBG_IOCTL_INFO              0x00000400
#define BUS_DBG_IOCTL_ERROR             0x00000800

#define BUS_DBG_POWER_MASK              0x0000F000
#define BUS_DBG_POWER_NOISE             0x00001000
#define BUS_DBG_POWER_TRACE             0x00002000
#define BUS_DBG_POWER_INFO              0x00004000
#define BUS_DBG_POWER_ERROR             0x00008000


#if DBG
#define BUS_DEFAULT_DEBUG_OUTPUT_LEVEL 0x000FFFFF

#define Bus_KdPrint(_d_,_l_, _x_) \
            if (!(_l_) || (_d_)->DebugLevel & (_l_)) { \
               DbgPrint ("vusbbus.sys: "); \
               DbgPrint _x_; \
            }

#define Bus_KdPrint_Cont(_d_,_l_, _x_) \
            if (!(_l_) || (_d_)->DebugLevel & (_l_)) { \
               DbgPrint _x_; \
            }

#define Bus_KdPrint_Def(_l_, _x_) \
            if (!(_l_) || VUsbDebugLevel & (_l_)) { \
               DbgPrint ("vusbbus.sys: "); \
               DbgPrint _x_; \
            }

#define DbgRaiseIrql(_x_,_y_) KeRaiseIrql(_x_,_y_)
#define DbgLowerIrql(_x_) KeLowerIrql(_x_)
#else

#define BUS_DEFAULT_DEBUG_OUTPUT_LEVEL 0x0
#define Bus_KdPrint(_d_, _l_, _x_)
#define Bus_KdPrint_Cont(_d_, _l_, _x_)
#define Bus_KdPrint_Def(_l_, _x_)
#define DbgRaiseIrql(_x_,_y_)
#define DbgLowerIrql(_x_)

#endif

extern ULONG    VUsbDebugLevel;

//
// These are the states a PDO or FDO transition upon
// receiving a specific PnP Irp. Refer to the PnP Device States
// diagram in DDK documentation for better understanding.
//

typedef enum _DEVICE_PNP_STATE {
    NotStarted                                                                  = 0,         // Not started yet
    Started,                // Device has received the START_DEVICE IRP
    StopPending,            // Device has received the QUERY_STOP IRP
    Stopped,                // Device has received the STOP_DEVICE IRP
    RemovePending,          // Device has received the QUERY_REMOVE IRP
    SurpriseRemovePending,  // Device has received the SURPRISE_REMOVE IRP
    Deleted,                // Device has received the REMOVE_DEVICE IRP
    UnKnown                 // Unknown state
}    DEVICE_PNP_STATE;


typedef struct _GLOBALS {
    // 
    // Path to the driver's Services Key in the registry
    //

    UNICODE_STRING  RegistryPath;
} GLOBALS;


extern GLOBALS  Globals;


//
// A common header for the device extensions of the PDOs and FDO
//

typedef struct _COMMON_DEVICE_DATA {
    // A back pointer to the device object for which this is the extension

    PDEVICE_OBJECT      Self;

    // This flag helps distinguish between PDO and FDO

    BOOLEAN             IsFDO;

    // We track the state of the device with every PnP Irp
    // that affects the device through these two variables.

    DEVICE_PNP_STATE    DevicePnPState;

    DEVICE_PNP_STATE    PreviousPnPState;


    ULONG               DebugLevel;

    // Stores the current system power state

    SYSTEM_POWER_STATE  SystemPowerState;

    // Stores current device power state

    DEVICE_POWER_STATE  DevicePowerState;
} COMMON_DEVICE_DATA, *PCOMMON_DEVICE_DATA;

//
// The device extension for the PDOs.
// That's of the USB HASP device which this bus driver enumerates.
//

typedef struct _PDO_DEVICE_DATA {
    COMMON_DEVICE_DATA;

    // A back pointer to the bus

    PDEVICE_OBJECT  ParentFdo;

    // An array of (zero terminated wide character strings).
    // The array itself also null terminated

    PWCHAR          HardwareIDs;

    // Unique serail number of the device on the bus

    ULONG           SerialNo;

    // Link point to hold all the PDOs for a single bus together

    LIST_ENTRY      Link;

    //
    // Present is set to TRUE when the PDO is exposed via PlugIn IOCTL,
    // and set to FALSE when a UnPlug IOCTL is received. 
    // We will delete the PDO in IRP_MN_REMOVE only after we have reported 
    // to the Plug and Play manager that it's missing.
    //

    BOOLEAN         Present;
    BOOLEAN         ReportedMissing;
    UCHAR           Reserved[2]; // for 4 byte alignment

    //
    // Used to track the intefaces handed out to other drivers.
    // If this value is non-zero, we fail query-remove.
    //
    ULONG           VUsbInterfaceRefCount;

    //
    // In order to reduce the complexity of the driver, I chose not 
    // to use any queues for holding IRPs when the system tries to 
    // rebalance resources to accommodate a new device, and stops our 
    // device temporarily. But in a real world driver this is required. 
    // If you hold Irps then you should also support Cancel and 
    // Cleanup functions. The function driver demonstrates these techniques.
    //    
    // The queue where the incoming requests are held when
    // the device is stopped for resource rebalance.

    //LIST_ENTRY          PendingQueue;     

    // The spin lock that protects access to  the queue

    //KSPIN_LOCK          PendingQueueLock;     

    // Данные, специфичные для HASP-ключей
    KEY_DATA        keyData;
} PDO_DEVICE_DATA, *PPDO_DEVICE_DATA;


//
// The device extension of the bus itself.  From whence the PDO's are born.
//

typedef struct _FDO_DEVICE_DATA {
    COMMON_DEVICE_DATA;

    PDEVICE_OBJECT          UnderlyingPDO;

    // The underlying bus PDO and the actual device object to which our
    // FDO is attached

    PDEVICE_OBJECT          NextLowerDriver;

    // List of PDOs created so far

    LIST_ENTRY              ListOfPDOs;

    // The PDOs currently enumerated.

    ULONG                   NumPDOs;

    // A synchronization for access to the device extension.

    FAST_MUTEX              Mutex;

    //
    // The number of IRPs sent from the bus to the underlying device object
    //

    ULONG                   OutstandingIO; // Biased to 1

    //
    // On remove device plug & play request we must wait until all outstanding
    // requests have been completed before we can actually delete the device
    // object. This event is when the Outstanding IO count goes to zero
    //

    KEVENT                  RemoveEvent;

    //
    // This event is set when the Outstanding IO count goes to 1.
    //

    KEVENT                  StopEvent;

    // The name returned from IoRegisterDeviceInterface,
    // which is used as a handle for IoSetDeviceInterfaceState.

    UNICODE_STRING          InterfaceName;

    //
    // Auto insert all dumps at startup flag
    //
    LONG                    isAutoInsert;
} FDO_DEVICE_DATA, *PFDO_DEVICE_DATA;

#define FDO_FROM_PDO(pdoData) \
          ((PFDO_DEVICE_DATA) (pdoData)->ParentFdo->DeviceExtension)

#define INITIALIZE_PNP_STATE(_Data_)    \
        (_Data_)->DevicePnPState =  NotStarted;\
        (_Data_)->PreviousPnPState = NotStarted;

#define SET_NEW_PNP_STATE(_Data_, _state_) \
        (_Data_)->PreviousPnPState =  (_Data_)->DevicePnPState;\
        (_Data_)->DevicePnPState = (_state_);

#define RESTORE_PREVIOUS_PNP_STATE(_Data_)   \
        (_Data_)->DevicePnPState =   (_Data_)->PreviousPnPState;\

//
// Prototypes of functions
//
//
// Defined in DriverEntry.C
//

NTSTATUS DriverEntry(IN PDRIVER_OBJECT, PUNICODE_STRING);

NTSTATUS Bus_CreateClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);

NTSTATUS Bus_IoCtl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);

VOID Bus_DriverUnload(IN PDRIVER_OBJECT DriverObject);

VOID Bus_IncIoCount(PFDO_DEVICE_DATA   Data);

VOID Bus_DecIoCount(PFDO_DEVICE_DATA   Data);

//
// Defined in PNP.C
//

PCHAR PnPMinorFunctionString(UCHAR MinorFunction);

NTSTATUS Bus_CompletionRoutine(IN PDEVICE_OBJECT   DeviceObject,
                               IN PIRP             Pirp,
                               IN PVOID            Context);

NTSTATUS Bus_SendIrpSynchronously(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);

NTSTATUS Bus_PnP(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);


NTSTATUS Bus_AddDevice(IN PDRIVER_OBJECT DriverObject,
                       IN PDEVICE_OBJECT BusDeviceObject);

VOID Bus_InitializePdo(PDEVICE_OBJECT      Pdo, PFDO_DEVICE_DATA    FdoData);

NTSTATUS Bus_PlugInDevice(PVUSB_PLUGIN_HARDWARE   PlugIn,
                          ULONG                       PlugInLength,
                          PFDO_DEVICE_DATA            DeviceData);


NTSTATUS Bus_UnPlugDevice(PVUSB_UNPLUG_HARDWARE   UnPlug,
                          PFDO_DEVICE_DATA            DeviceData);

NTSTATUS Bus_EjectDevice(PVUSB_EJECT_HARDWARE     Eject,
                         PFDO_DEVICE_DATA            FdoData);

void Bus_RemoveFdo(PFDO_DEVICE_DATA    FdoData);

NTSTATUS Bus_DestroyPdo(PDEVICE_OBJECT      Device,
                        PPDO_DEVICE_DATA    PdoData);


NTSTATUS Bus_FDO_PnP(IN PDEVICE_OBJECT       DeviceObject,
                     IN PIRP                 Irp,
                     IN PIO_STACK_LOCATION   IrpStack,
                     IN PFDO_DEVICE_DATA     DeviceData);


NTSTATUS Bus_StartFdo(IN  PFDO_DEVICE_DATA            FdoData, IN  PIRP   Irp);

PCHAR DbgDeviceIDString(BUS_QUERY_ID_TYPE Type);

PCHAR DbgDeviceRelationString(IN DEVICE_RELATION_TYPE Type);

//
// Defined in Power.c
//

NTSTATUS Bus_FDO_Power(PFDO_DEVICE_DATA    FdoData, PIRP                Irp);

NTSTATUS Bus_PDO_Power(PPDO_DEVICE_DATA    PdoData, PIRP                Irp);

NTSTATUS Bus_Power(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);

PCHAR PowerMinorFunctionString(UCHAR MinorFunction);

PCHAR DbgSystemPowerString(IN SYSTEM_POWER_STATE Type);
PCHAR DbgDevicePowerString(IN DEVICE_POWER_STATE Type);

//
// Defined in BusPDO.c
//

NTSTATUS Bus_PDO_PnP(IN PDEVICE_OBJECT       DeviceObject,
                     IN PIRP                 Irp,
                     IN PIO_STACK_LOCATION   IrpStack,
                     IN PPDO_DEVICE_DATA     DeviceData);

NTSTATUS Bus_PDO_QueryDeviceCaps(IN PPDO_DEVICE_DATA     DeviceData,
                                 IN  PIRP   Irp);

NTSTATUS Bus_PDO_QueryDeviceId(IN PPDO_DEVICE_DATA     DeviceData,
                               IN  PIRP   Irp);


NTSTATUS Bus_PDO_QueryDeviceText(IN PPDO_DEVICE_DATA     DeviceData,
                                 IN  PIRP   Irp);

NTSTATUS Bus_PDO_QueryResources(IN PPDO_DEVICE_DATA     DeviceData,
                                IN  PIRP   Irp);

NTSTATUS Bus_PDO_QueryResourceRequirements(IN PPDO_DEVICE_DATA     DeviceData,
                                           IN  PIRP   Irp);

NTSTATUS Bus_PDO_QueryDeviceRelations(IN PPDO_DEVICE_DATA     DeviceData,
                                      IN  PIRP   Irp);

NTSTATUS Bus_PDO_QueryBusInformation(IN PPDO_DEVICE_DATA     DeviceData,
                                     IN  PIRP   Irp);

NTSTATUS Bus_GetDeviceCapabilities(IN  PDEVICE_OBJECT          DeviceObject,
                                   IN  PDEVICE_CAPABILITIES    DeviceCapabilities);

NTSTATUS Bus_PDO_QueryInterface(IN PPDO_DEVICE_DATA     DeviceData,
                                IN  PIRP   Irp);

VOID Bus_InterfaceReference(IN PVOID Context);
VOID Bus_InterfaceDereference(IN PVOID Context);

#ifdef DEBUG_FULL
void PrintBufferContent(OUT PWCHAR outStr, UCHAR *buf, LONG length);
NTSTATUS LogMessage(PCHAR szFormat, ...);
#else
#define PrintBufferContent(exp)         ((void) 0)
#define LogMessage(exp)         ((void) 0)
#pragma warning(disable:4002)
#endif

#endif