#include "IoCreateDevice.h"
#pragma warning(disable: 4996)

/*
nt!MiProcessLoaderEntry+0x77:
fffff803`558bffff 488b1562559a00  mov     rdx,qword ptr [nt!MiState+0x4e8 (fffff803`56265568)]
2: kd> k
 # Child-SP          RetAddr               Call Site
00 ffffdb87`2dcd9e30 fffff803`55d019f0     nt!MiProcessLoaderEntry+0x77
01 ffffdb87`2dcd9e70 fffff803`55d021aa     nt!MiConstructLoaderEntry+0x48c
02 ffffdb87`2dcd9fc0 fffff803`55d0154e     nt!MmLoadSystemImageEx+0x2de
03 ffffdb87`2dcda170 fffff803`55d1c9d3     nt!MmLoadSystemImage+0x2e
04 ffffdb87`2dcda1c0 fffff803`55d39617     nt!IopLoadDriver+0x24b
05 ffffdb87`2dcda380 fffff803`55953665     nt!IopLoadUnloadDriver+0x57
06 ffffdb87`2dcda3c0 fffff803`558ed757     nt!ExpWorkerThread+0x155
07 ffffdb87`2dcda5b0 fffff803`55a1c944     nt!PspSystemThreadStartup+0x57
08 ffffdb87`2dcda600 00000000`00000000     nt!KiStartSystemThread+0x34

When NtLoadDriver is called, it will call IopLoadDriverImage which will lead to calling IopLoadDriver which will trigger ETW anyways.
*/

NTSTATUS __fastcall MyIopInvalidDeviceRequest(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_INVALID_DEVICE_REQUEST;
}
// reactos/drivers/base/ntoskrnl/io/iomgr/driver.c
NTSTATUS
__fastcall
MyIoCreateDriver(
    _In_opt_ PUNICODE_STRING DriverName,
    _In_ PDRIVER_INITIALIZE InitializationFunction)
{
    WCHAR NameBuffer[100];
    USHORT NameLength;
    UNICODE_STRING LocalDriverName;
    NTSTATUS Status;
    OBJECT_ATTRIBUTES ObjectAttributes;
    ULONG ObjectSize;
    PDRIVER_OBJECT DriverObject;
    UNICODE_STRING ServiceKeyName;
    HANDLE hDriver;
    ULONG i, RetryCount = 0;
    ULONG Seed = 0xdeadbeef;

try_again:
    /* First, create a unique name for the driver if we don't have one */
    if (!DriverName)
    {
        /* Create a random name and set up the string */
        NameLength = (USHORT)swprintf(NameBuffer,
            L"\\Driver\\%08u",
            RtlRandomEx(&Seed));
        LocalDriverName.Length = NameLength * sizeof(WCHAR);
        LocalDriverName.MaximumLength = LocalDriverName.Length + sizeof(UNICODE_NULL);
        LocalDriverName.Buffer = NameBuffer;
    }
    else
    {
        /* So we can avoid another code path, use a local var */
        LocalDriverName = *DriverName;
    }

    /* Initialize the Attributes */
    ObjectSize = sizeof(DRIVER_OBJECT) + sizeof(EXTENDED_DRIVER_EXTENSION);
    InitializeObjectAttributes(&ObjectAttributes,
        &LocalDriverName,
        OBJ_PERMANENT | OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        NULL);

    /* Create the Object */
    Status = ObCreateObject(KernelMode,
        IoDriverObjectType,
        &ObjectAttributes,
        KernelMode,
        NULL,
        ObjectSize,
        0,
        0,
        (PVOID*)&DriverObject);
    if (!NT_SUCCESS(Status)) return Status;

    /* Set up the Object */
    RtlZeroMemory(DriverObject, ObjectSize);
    DriverObject->Type = IO_TYPE_DRIVER;
    DriverObject->Size = sizeof(DRIVER_OBJECT);
    DriverObject->Flags = DRVO_BUILTIN_DRIVER;
    DriverObject->DriverExtension = (PDRIVER_EXTENSION)(DriverObject + 1);
    DriverObject->DriverExtension->DriverObject = DriverObject;
    DriverObject->DriverInit = InitializationFunction;
    /* Loop all Major Functions */
    for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
    {
        /* Invalidate each function */
        DriverObject->MajorFunction[i] = MyIopInvalidDeviceRequest;
    }

    /* Set up the service key name buffer */
    ServiceKeyName.MaximumLength = LocalDriverName.Length + sizeof(UNICODE_NULL);
    ServiceKeyName.Buffer = ExAllocatePoolWithTag(PagedPool, LocalDriverName.MaximumLength, 'xCoI');
    if (!ServiceKeyName.Buffer)
    {
        /* Fail */
        ObMakeTemporaryObject(DriverObject);
        ObDereferenceObject(DriverObject);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /* For builtin drivers, the ServiceKeyName is equal to DriverName */
    RtlCopyUnicodeString(&ServiceKeyName, &LocalDriverName);
    ServiceKeyName.Buffer[ServiceKeyName.Length / sizeof(WCHAR)] = UNICODE_NULL;
    DriverObject->DriverExtension->ServiceKeyName = ServiceKeyName;

    /* Make a copy of the driver name to store in the driver object */
    DriverObject->DriverName.MaximumLength = LocalDriverName.Length;
    DriverObject->DriverName.Buffer = ExAllocatePoolWithTag(PagedPool,
        DriverObject->DriverName.MaximumLength,
        'xCoI');
    if (!DriverObject->DriverName.Buffer)
    {
        /* Fail */
        ObMakeTemporaryObject(DriverObject);
        ObDereferenceObject(DriverObject);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyUnicodeString(&DriverObject->DriverName, &LocalDriverName);

    /* Add the Object and get its handle */
    Status = ObInsertObject(DriverObject,
        NULL,
        FILE_READ_DATA,
        0,
        NULL,
        &hDriver);

    /* Eliminate small possibility when this function is called more than
       once in a row, and KeTickCount doesn't get enough time to change */
    if (!DriverName && (Status == STATUS_OBJECT_NAME_COLLISION) && (RetryCount < 100))
    {
        RetryCount++;
        goto try_again;
    }

    if (!NT_SUCCESS(Status)) return Status;

    /* Now reference it */
    Status = ObReferenceObjectByHandle(hDriver,
        0,
        IoDriverObjectType,
        KernelMode,
        (PVOID*)&DriverObject,
        NULL);

    /* Close the extra handle */
    ZwClose(hDriver);

    if (!NT_SUCCESS(Status))
    {
        /* Fail */
        ObMakeTemporaryObject(DriverObject);
        ObDereferenceObject(DriverObject);
        return Status;
    }

    /* Finally, call its init function */
    Status = InitializationFunction(DriverObject, NULL);
    if (!NT_SUCCESS(Status))
    {
        /* If it didn't work, then kill the object */
        ObMakeTemporaryObject(DriverObject);
        ObDereferenceObject(DriverObject);
        return Status;
    }

    /* Windows does this fixup, keep it for compatibility */
    for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
    {
        /*
         * Make sure the driver didn't set any dispatch entry point to NULL!
         * Doing so is illegal; drivers shouldn't touch entry points they
         * do not implement.
         */

         /* Check if it did so anyway */
        if (!DriverObject->MajorFunction[i])
        {
            /* Fix it up */
            DriverObject->MajorFunction[i] = MyIopInvalidDeviceRequest;
        }
    }

    /* Return the Status */
    return Status;
}