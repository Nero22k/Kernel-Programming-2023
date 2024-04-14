#include <ntifs.h>
#include "IoCreateDevice.h"
#include "Defs.h"
#include <Aux_klib.h>

//VOID UnloadDriver(PDRIVER_OBJECT DriverObject);
NTSTATUS EnumProcesses();
NTSTATUS EnumCallbacks();

NTSTATUS DriverInitialize(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{

    DbgPrint("========== Driver Loaded ==========\n");
    DbgPrint("DriverObject:\t\t0x%p\n", DriverObject);
    DbgPrint("RegistryPath:\t\t0x%p\n", RegistryPath);
    DbgPrint("========================================\n");

    NTSTATUS status = STATUS_SUCCESS;
    /*PDEVICE_OBJECT devObj = NULL;
    UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\krnlCalls");

    status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &devObj);
    if(!NT_SUCCESS(status))
    {
        DbgPrint("Failed to create device object\n");
        return status;
    }

    DriverObject->DriverUnload = UnloadDriver;*/

    status = EnumProcesses();

    if (!NT_SUCCESS(status))
    {
        DbgPrint("Failed to enumerate processes\n");
        //UnloadDriver(DriverObject);
    }

    status = EnumCallbacks();

    if (!NT_SUCCESS(status))
    {
        DbgPrint("Failed to enumerate callbacks\n");
        //UnloadDriver(DriverObject);
    }

    return status;
}

NTSTATUS driver_entry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNICODE_STRING name = RTL_CONSTANT_STRING(L"\\Driver\\krnlCalls");
    /* This parameters are invalid due to nonstandard way of loading and should not be used. */
    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);
    NTSTATUS status = MyIoCreateDriver(&name, &DriverInitialize);
    if (!NT_SUCCESS(status))
    {
        DbgPrint("Failed to create device object\n");
        return status;
    }

    return status;
}

/*VOID UnloadDriver(PDRIVER_OBJECT DriverObject)
{
    DbgPrint("========== Driver Unloaded ==========\n");
    IoDeleteDevice(DriverObject->DeviceObject);
}*/

BOOLEAN GetModuleInfoForAddress(PVOID Address, PMODULE_INFO ModuleInfo)
{
    ULONG BufferSize = 0;
    ULONG ModulesCount = 0;
    PAUX_MODULE_EXTENDED_INFO ModuleExtendedInfo;

    // initialise library
    NTSTATUS status = AuxKlibInitialize();

    if (!NT_SUCCESS(status))
    {
        DbgPrint("[!] AuxKlibInitialize failed: 0x%08X\n", status);
        return FALSE;
    }

    status = AuxKlibQueryModuleInformation(&BufferSize, sizeof(AUX_MODULE_EXTENDED_INFO), NULL);

    if (!NT_SUCCESS(status))
    {
        DbgPrint("[!] AuxKlibQueryModuleInformation failed: 0x%08X\n", status);
        return FALSE;
    }

    ModulesCount = BufferSize / sizeof(AUX_MODULE_EXTENDED_INFO);

    if (ModulesCount == 0)
        return FALSE;

    ModuleExtendedInfo = (PAUX_MODULE_EXTENDED_INFO)ExAllocatePoolWithTag(NonPagedPool, BufferSize, 'LFNI');
    if (ModuleExtendedInfo == NULL)
        return FALSE;

    status = AuxKlibQueryModuleInformation(&BufferSize, sizeof(AUX_MODULE_EXTENDED_INFO), ModuleExtendedInfo);

    if (!NT_SUCCESS(status))
    {
        ExFreePoolWithTag(ModuleExtendedInfo, 'LFNI');
        DbgPrint("[!] AuxKlibQueryModuleInformation failed: 0x%08X\n", status);
        return FALSE;
    }

    // Find the module that contains the address
    for (ULONG i = 0; i < ModulesCount; i++)
    {
        PAUX_MODULE_EXTENDED_INFO CurrentModule = &ModuleExtendedInfo[i];
        ULONG_PTR ModuleStart = (ULONG_PTR)CurrentModule->BasicInfo.ImageBase;
        ULONG_PTR ModuleEnd = ModuleStart + CurrentModule->ImageSize;

        if ((ULONG_PTR)Address >= ModuleStart && (ULONG_PTR)Address < ModuleEnd)
        {
            ModuleInfo->BaseAddress = ModuleStart;
            ModuleInfo->ImageSize = CurrentModule->ImageSize;
            RtlCopyMemory(ModuleInfo->FullPathName, CurrentModule->FullPathName, sizeof(ModuleInfo->FullPathName));
            ExFreePoolWithTag(ModuleExtendedInfo, 'LFNI');
            return TRUE;
        }
    }

    ExFreePoolWithTag(ModuleExtendedInfo, 'LFNI');
    return FALSE;
}

PVOID FindCallbackFuncAddr(PVOID CallbackFunc)
{
    int found = 0;
    LONG displacement = 0;
    ULONG_PTR address = (ULONG_PTR)CallbackFunc;
    for (int i = 0; i < 30; i++)
    {
        volatile UCHAR opcode = *(PCHAR)(address); // Read the opcode
        if (opcode == 0xE8) // E8 is the opcode for call
        {
            found = 1;
            displacement = *(PLONG)(address + 1); // Get the displacement
            break;
        }
        address++;
    }

    if (!found)
    {
        DbgPrint("Failed to find call opcodes\n");
        return NULL;
    }

    return (PVOID)(address + 5 + displacement);
}

PVOID FindCallbackArrayAddr(PVOID CallbackAddr)
{
    int found = 0;
    LONG displacement = 0;
    ULONG_PTR address = (ULONG_PTR)CallbackAddr;
    for (int i = 0; i < 0x100; i++)
    {
        volatile ULONG opcode = *(PLONG)(address); // Read the opcode
        if (((opcode & (0x00FFFFFF)) == 0x000D8D48)   // lea rcx,
            || ((opcode & (0x00FFFFFF)) == 0x002D8D4C) // lea r13,
            || ((opcode & (0x00FFFFFF)) == 0x00058D4C) // lea r8,
            || ((opcode & (0x00FFFFFF)) == 0x000D8D4C) // lea r9,
            || ((opcode & (0x00FFFFFF)) == 0x00158D4C) // lea r10,
            || ((opcode & (0x00FFFFFF)) == 0x001D8D4C) // lea r11,
            || ((opcode & (0x00FFFFFF)) == 0x00258D4C) // lea r12,
            || ((opcode & (0x00FFFFFF)) == 0x00358D4C) // lea r14,
            || ((opcode & (0x00FFFFFF)) == 0x003D8D4C)) // lea r15,
        {
            found = 1;
            displacement = *(PLONG)(address + 3); // Get the displacement
            break;
        }
        address++; // Inc by 1
    }

    if (!found)
    {
        DbgPrint("Failed to find lea opcodes\n");
        return NULL;
    }

    return (PVOID)(address + 7 + displacement); // addr + 7 (size of lea opcodes) + displacement
}

VOID EnumerateCallbackArray(PVOID CallbackArrayAddr)
{

    // Enumerate Callback Array

    PVOID CallbackRoutineArray = CallbackArrayAddr;

    for (int i = 0; i < 64; i++)
    {
        try
        {
            PVOID callback = ((PVOID*)CallbackRoutineArray)[i];
            if (callback)
            {
                // Modify the callback pointer if it ends with 'f'
                if ((ULONG_PTR)callback & 0xf)
                {
                    callback = (PVOID)((ULONG_PTR)callback & ~0xf | 0x8);
                }

                // Dereference the modified callback pointer to get the address of the callback function
                PVOID callbackFunction = *(PVOID*)callback;

                MODULE_INFO moduleInfo = { 0 };
                if (GetModuleInfoForAddress(callbackFunction, &moduleInfo))
                {
                    DbgPrint("Callback[%d]: Function: 0x%p, Module: %s\n", i, callbackFunction, moduleInfo.FullPathName); // Ex: \SystemRoot\System32\drivers\cng.sys
                }
                else
                {
                    DbgPrint("Callback[%d]: Function: 0x%p, Module: Unknown\n", i, callbackFunction);
                }
            }
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            DbgPrint("Failed to read callback[%d]\n", i);
            break;
        }
    }
}

NTSTATUS EnumCallbacks()
{

    DbgPrint("========== Enumerating Callbacks ==========\n");

    UNICODE_STRING funcName, funcName2, funcName3;
    RtlInitUnicodeString(&funcName, L"PsSetCreateProcessNotifyRoutine");
    RtlInitUnicodeString(&funcName2, L"PsSetLoadImageNotifyRoutine");
    RtlInitUnicodeString(&funcName3, L"PsSetCreateThreadNotifyRoutine");

    PVOID fPsSetCreateCallbackArray = MmGetSystemRoutineAddress(&funcName);
    PVOID fPsSetLoadImageNotifyRoutine = MmGetSystemRoutineAddress(&funcName2);
    PVOID fPsSetCreateThreadNotifyRoutine = MmGetSystemRoutineAddress(&funcName3);
    if (!fPsSetCreateCallbackArray && !fPsSetLoadImageNotifyRoutine && !fPsSetCreateThreadNotifyRoutine)
    {
        return STATUS_UNSUCCESSFUL;
    }

    // Read memory of PsSetCreateProcessNotifyRoutine function to find the call opcodes
    
    DbgPrint("PsSetCreateProcessNotifyRoutine: 0x%p\n", fPsSetCreateCallbackArray);

    PVOID PspSetCreateProcessNotifyRoutine = FindCallbackFuncAddr(fPsSetCreateCallbackArray);

    if(PspSetCreateProcessNotifyRoutine == NULL)
    {
        return STATUS_UNSUCCESSFUL;
    }

    DbgPrint("PspSetCreateProcessNotifyRoutine: 0x%p\n", PspSetCreateProcessNotifyRoutine);

    PVOID PspCreateProcessNotifyRoutine = FindCallbackArrayAddr(PspSetCreateProcessNotifyRoutine);

    if(PspCreateProcessNotifyRoutine == NULL)
    {
        return STATUS_UNSUCCESSFUL;
    }

    DbgPrint("PspCreateProcessNotifyRoutine: 0x%p\n", PspCreateProcessNotifyRoutine);

    EnumerateCallbackArray(PspCreateProcessNotifyRoutine);

    DbgPrint("========================================\n");

    // Read memory of PsSetLoadImageNotifyRoutine function to find the call opcodes

    DbgPrint("PsSetLoadImageNotifyRoutine: 0x%p\n", fPsSetLoadImageNotifyRoutine);

    PVOID PsSetLoadImageNotifyRoutineEx = FindCallbackFuncAddr(fPsSetLoadImageNotifyRoutine);

    if (PsSetLoadImageNotifyRoutineEx == NULL)
    {
        return STATUS_UNSUCCESSFUL;
    }

    DbgPrint("PsSetLoadImageNotifyRoutineEx: 0x%p\n", PsSetLoadImageNotifyRoutineEx);

    PVOID PspLoadImageNotifyRoutine = FindCallbackArrayAddr(PsSetLoadImageNotifyRoutineEx);

    if (PspLoadImageNotifyRoutine == NULL)
    {
        return STATUS_UNSUCCESSFUL;
    }

    DbgPrint("PspLoadImageNotifyRoutine: 0x%p\n", PspLoadImageNotifyRoutine);

    EnumerateCallbackArray(PspLoadImageNotifyRoutine);

    DbgPrint("========================================\n");

    // Read memory of PsSetCreateThreadNotifyRoutine function to find the call opcodes

    DbgPrint("PsSetCreateThreadNotifyRoutine: 0x%p\n", fPsSetCreateThreadNotifyRoutine);

    PVOID PspSetCreateThreadNotifyRoutine = FindCallbackFuncAddr(fPsSetCreateThreadNotifyRoutine);

    if (PspSetCreateThreadNotifyRoutine == NULL)
    {
        return STATUS_UNSUCCESSFUL;
    }

    DbgPrint("PspSetCreateThreadNotifyRoutine: 0x%p\n", PspSetCreateThreadNotifyRoutine);

    PVOID PspCreateThreadNotifyRoutine = FindCallbackArrayAddr(PspSetCreateThreadNotifyRoutine);

    if (PspCreateThreadNotifyRoutine == NULL)
    {
        return STATUS_UNSUCCESSFUL;
    }

    DbgPrint("PspCreateThreadNotifyRoutine: 0x%p\n", PspCreateThreadNotifyRoutine);

    EnumerateCallbackArray(PspCreateThreadNotifyRoutine);

    DbgPrint("========================================\n");

    return STATUS_SUCCESS;
}

NTSTATUS EnumProcesses()
{
    DbgPrint("========== Enumerating Processes ==========\n");

    KGUARDED_MUTEX PspActiveProcessMutex;
    PLIST_ENTRY activeProcessLinks = (PLIST_ENTRY)((ULONG_PTR)PsInitialSystemProcess + ActiveProcessLinksOffset);

    if(!activeProcessLinks)
    {
        DbgPrint("Failed to get ActiveProcessLinks\n");
        return STATUS_UNSUCCESSFUL;
    }

    KeInitializeGuardedMutex(&PspActiveProcessMutex); // Initialize the guarded mutex which are faster then fast mutex 
    KeEnterGuardedRegion(); // Enter the guarded region
    KeAcquireGuardedMutexUnsafe(&PspActiveProcessMutex); // Acquire the mutex

    PLIST_ENTRY listEntry = activeProcessLinks->Flink;
    while (listEntry != activeProcessLinks) {
        PEPROCESS process = (PEPROCESS)((ULONG_PTR)listEntry - ActiveProcessLinksOffset);
        PUCHAR processName = (PUCHAR)((ULONG_PTR)process + ImageFileNameOffset);
        PPS_PROTECTION protection = (PPS_PROTECTION)((ULONG_PTR)process + ProcessProtectionOffset);
        HANDLE processId = *(PHANDLE)((ULONG_PTR)process + UniqueProcessIdOffset);
        listEntry = listEntry->Flink;
        if (processId && processName)
        {
            DbgPrint("[%04X] Process Name: %s - (0x%p)\n", processId, processName, process);
            if (protection->Level)
            {
                DbgPrint("\t[+] Protection Level: %02X\n", protection->Level);
                PS_PROTECTED_SIGNER signer = (PS_PROTECTED_SIGNER)protection->Signer;
                PS_PROTECTED_TYPE type = (PS_PROTECTED_TYPE)protection->Type;

                if (type >= PsProtectedTypeNone && type <= PsProtectedTypeProtected)
                {
                    DbgPrint("\t\t[+] %s Process\n", ProtectedTypeStrings[type]);
                }
                else
                {
                    DbgPrint("\t\t[+] Unknown Protection\n");
                }

                if (signer >= PsProtectedSignerNone && signer < PsProtectedSignerMax)
                {
                    DbgPrint("\t\t[+] PsProtectedSigner%s\n", ProtectedSignerStrings[signer]);
                }
                else
                {
                    DbgPrint("\t\t[+] Unknown Signer\n");
                }
            }
        }
    }

    KeReleaseGuardedMutexUnsafe(&PspActiveProcessMutex); // Release the mutex
    KeLeaveGuardedRegion(); // Leave the guarded region

    DbgPrint("========================================\n");

    return STATUS_SUCCESS;
}