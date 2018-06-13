/*++
Copyright (c) 2004 Chingachguk & Denger2k All Rights Reserved

Module Name:

    driver.h

Abstract:

    This module contains the common declarations for the 
    bus, function and filter drivers.

Environment:

    kernel mode only

Notes:


Revision History:


--*/

#include "public.h"

//
// Define an Interface Guid to access the proprietary virtual USB bus interface.
// This guid is used to identify a specific interface in IRP_MN_QUERY_INTERFACE
// handler.
//

DEFINE_GUID(GUID_VUSB_INTERFACE_STANDARD, 
    0xf5e40fd0, 0x74d2, 0x40b1, 0xb2, 0xb9, 0xde, 0xe2, 0x93, 0xf0, 0x87, 0x73);
// {F5E40FD0-74D2-40b1-B2B9-DEE293F08773}

//
// Define a Guid for virtual USB bus type. This is returned in response to
// IRP_MN_QUERY_BUS_INTERFACE on PDO.
//

DEFINE_GUID(GUID_VUSB_BUS_TYPE, 
    0xe4fce7ac, 0x9f92, 0x4620, 0x8b, 0xd4, 0xa9, 0x13, 0xeb, 0xec, 0x9a, 0xe4);
// {E4FCE7AC-9F92-4620-8BD4-A913EBEC9AE4}

//
// GUID definition are required to be outside of header inclusion pragma to avoid
// error during precompiled headers.
//

#ifndef __DRIVER_H
#define __DRIVER_H

//
// Define Interface reference/dereference routines for
//  Interfaces exported by IRP_MN_QUERY_INTERFACE
//

typedef VOID (*PINTERFACE_REFERENCE)(PVOID Context);
typedef VOID (*PINTERFACE_DEREFERENCE)(PVOID Context);

typedef
BOOLEAN
(*PVUSB_GET_CRISPINESS_LEVEL)(
                           IN   PVOID Context,
                           OUT  PUCHAR Level
                               );

typedef
BOOLEAN
(*PVUSB_SET_CRISPINESS_LEVEL)(
                           IN   PVOID Context,
                           OUT  UCHAR Level
                               );

typedef
BOOLEAN
(*PVUSB_IS_CHILD_PROTECTED)(
                             IN PVOID Context
                             );

//
// Interface for getting and setting power level etc.,
//
typedef struct _VUSB_INTERFACE_STANDARD {
   USHORT                           Size;
   USHORT                           Version;
   PINTERFACE_REFERENCE             InterfaceReference;
   PINTERFACE_DEREFERENCE           InterfaceDereference;
   PVOID                            Context;
   PVUSB_GET_CRISPINESS_LEVEL       GetCrispinessLevel;
   PVUSB_SET_CRISPINESS_LEVEL       SetCrispinessLevel;
   PVUSB_IS_CHILD_PROTECTED         IsSafetyLockEnabled;
} VUSB_INTERFACE_STANDARD, *PVUSB_INTERFACE_STANDARD;


#endif


