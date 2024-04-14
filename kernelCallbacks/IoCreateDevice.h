#pragma once
#include <ntifs.h>
#include <ntstrsafe.h>
#include <windef.h>

/* Definitions */
__declspec(dllimport) POBJECT_TYPE IoDriverObjectType;		// IoDriverObjectType
NTSTATUS __fastcall MyIoCreateDriver(
    _In_opt_ PUNICODE_STRING DriverName,
    _In_ PDRIVER_INITIALIZE InitializationFunction);

// ObCreateObject
NTSTATUS NTAPI ObCreateObject(IN KPROCESSOR_MODE ProbeMode 	OPTIONAL,
    IN POBJECT_TYPE 	Type,
    IN POBJECT_ATTRIBUTES ObjectAttributes 	OPTIONAL,
    IN KPROCESSOR_MODE 	AccessMode,
    IN OUT PVOID ParseContext 	OPTIONAL,
    IN ULONG 	ObjectSize,
    IN ULONG PagedPoolCharge 	OPTIONAL,
    IN ULONG NonPagedPoolCharge 	OPTIONAL,
    OUT PVOID* Object
);

typedef struct _IO_CLIENT_EXTENSION
{
    struct _IO_CLIENT_EXTENSION* NextExtension;
    PVOID ClientIdentificationAddress;
} IO_CLIENT_EXTENSION, * PIO_CLIENT_EXTENSION;

typedef struct _EXTENDED_DRIVER_EXTENSION
{
    struct _DRIVER_OBJECT* DriverObject;
    PDRIVER_ADD_DEVICE AddDevice;
    ULONG Count;
    UNICODE_STRING ServiceKeyName;
    PIO_CLIENT_EXTENSION ClientDriverExtension;
    PFS_FILTER_CALLBACKS FsFilterCallbacks;
} EXTENDED_DRIVER_EXTENSION, * PEXTENDED_DRIVER_EXTENSION;