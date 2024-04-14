#pragma once
#include <ntifs.h>

#pragma warning(disable : 4201)
#pragma warning(disable : 4996)

#define TARGET_PROCESS L"notepad.exe"
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000

extern void* CallbackRegistrationHandle;

/*
* A Callback function that gets called when new process is created
*	arguments:
*		Process - Parameter that points to EPROCESS structure of newly created process
*		ProcessId - Parameter that holds the process identifier
*		CreateInfo - Parameter that holds metadata about the created process
*/

VOID OnProcessCallback(
	PEPROCESS Process,
	HANDLE ProcessId,
	PPS_CREATE_NOTIFY_INFO CreateInfo
);

/*
* A Pre-Callback function that gets called before a handle is fully created
*	argumenst:
*		RegistrationContext
*		OperationInformation
*/

OB_PREOP_CALLBACK_STATUS PreOperationCallback(
	PVOID RegistrationContext, 
	POB_PRE_OPERATION_INFORMATION OperationInformation
);

VOID PostOperationCallback(
	PVOID RegistrationContext,
	POB_POST_OPERATION_INFORMATION PostInfo
);

/*
* Function that registers the handle creation callback with specified target process
*/

VOID enableProtection();
VOID disableProtection();