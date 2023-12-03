#pragma once
// Driver only header that is used only by our driver
#include "ProcMonCommon.h"
#include "FastMutex.h"

#define DRIVER_PREFIX "ProcMon: "
#define DRIVER_TAG 'nmrp'
#define DRIVER_SYMLINKNAME L"\\??\\ProcMon"
#define DRIVER_DEVICENAME L"\\Device\\ProcMon"

typedef struct _FullEventData {
	LIST_ENTRY Link;
	EventData Data;
}FullEventData;

typedef struct _ProcMonState {
	LIST_ENTRY ItemsHead; // Linked list
	ULONG ItemCount; // Item count
	FastMutex Lock; // Lock for synchronization
}ProcMonState;