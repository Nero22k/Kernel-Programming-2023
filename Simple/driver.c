#include <ntddk.h>
#include <ntstrsafe.h>

#define DRIVER_TAG 'lpms'

#pragma warning(disable : 4996)

UNICODE_STRING g_RegPath;

void UnloadRoutine(PDRIVER_OBJECT DriverObject);
NTSTATUS SetRegKey(PUNICODE_STRING RegistryPath, ULONG MajorVer, ULONG MinorVer, ULONG BuildNumber);

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(DriverObject);
    RTL_OSVERSIONINFOW versionInfo = { 0 };

    KdPrint(("Driver Entry!\n"));
    KdPrint(("Registry path => %wZ\n", RegistryPath));

    NTSTATUS status = RtlGetVersion(&versionInfo);

    KdPrint(("Windows Version is %lu.%lu.%lu\n", versionInfo.dwMajorVersion, versionInfo.dwMinorVersion, versionInfo.dwBuildNumber));

    status = SetRegKey(RegistryPath, versionInfo.dwMajorVersion, versionInfo.dwMinorVersion, versionInfo.dwBuildNumber);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    if (NT_SUCCESS(status) &&
        (versionInfo.dwMajorVersion > 10 ||
            (versionInfo.dwMajorVersion == 10 && versionInfo.dwBuildNumber >= 19041))) // Check if minimum OS version is windows 10 version 2004 or later
    {
        // Use ExAllocatePool2 for newer versions of Windows
        //g_RegPath.Buffer = ExAllocatePool2(POOL_FLAG_NON_PAGED | POOL_FLAG_UNINITIALIZED, RegistryPath->Length, DRIVER_TAG);
    }
    else
    {
        // Use ExAllocatePoolWithTag for older versions of Windows
        g_RegPath.Buffer = ExAllocatePoolWithTag(NonPagedPool, RegistryPath->Length, DRIVER_TAG);
    }

    if (g_RegPath.Buffer == NULL)
    {
        KdPrint(("Error allocating memory!\n"));
        return STATUS_NO_MEMORY;
    }

    RtlCopyMemory(g_RegPath.Buffer, RegistryPath->Buffer, RegistryPath->Length);
    g_RegPath.Length = g_RegPath.MaximumLength = RegistryPath->Length;

    KdPrint(("Registry copy path => %wZ\n", g_RegPath));

    DriverObject->DriverUnload = UnloadRoutine;

    return STATUS_SUCCESS;
}

void UnloadRoutine(PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);
    KdPrint(("Driver Unload!\n"));
    ExFreePoolWithTag(g_RegPath.Buffer, DRIVER_TAG);
    //IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS SetRegKey(PUNICODE_STRING RegistryPath, ULONG MajorVer, ULONG MinorVer, ULONG BuildNumber)
{
    HANDLE regKey;
    OBJECT_ATTRIBUTES attributes;
    NTSTATUS status;

    InitializeObjectAttributes(&attributes, RegistryPath,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL, NULL);

    status = ZwOpenKey(&regKey, KEY_WRITE, &attributes);
    if (!NT_SUCCESS(status)) {
        KdPrint(("Failed to open registry key\n"));
        return status;
    }
    // Use the keyHandle to write a value
    UNICODE_STRING valueName;
    UNICODE_STRING data;
    WCHAR buffer[260];
    RtlStringCbPrintfW(buffer, sizeof(buffer), L"%lu.%lu.%lu", MajorVer, MinorVer, BuildNumber);
    RtlInitUnicodeString(&data, buffer);
    RtlInitUnicodeString(&valueName, L"OS Version");
    status = ZwSetValueKey(regKey, &valueName, 0, REG_SZ, data.Buffer, data.Length);
    if (!NT_SUCCESS(status)) 
    {
        KdPrint(("Failed to write registry value\n"));
        ZwClose(regKey);
        return status;
    }

    ZwClose(regKey);

    return STATUS_SUCCESS;
}
