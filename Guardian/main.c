#include "Defs.h"

VOID DriverUnload(PDRIVER_OBJECT);

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	DbgPrint("========== Guardian Lunched ==========\n");
	DbgPrint("Driver Object:\t\t0x%p\n", DriverObject);
	DbgPrint("Registry Path:\t\t0x%p\n", RegistryPath);
	DbgPrint("========================================\n");

	DriverObject->DriverUnload = DriverUnload;

	NTSTATUS status = STATUS_SUCCESS;

	status = PsSetCreateProcessNotifyRoutineEx(OnProcessCallback, FALSE); // Registers a callback for process creation
	if (!NT_SUCCESS(status)) // Check if failed
	{
		DbgPrint("PsSetCreateProcessNotifyRoutineEx Error:\t\t0x%p\n", status);
		PsSetCreateProcessNotifyRoutineEx(OnProcessCallback, TRUE); // Cleanup if failed
	}

	return status;
}

VOID DriverUnload(PDRIVER_OBJECT DriverObject)
{
	UNREFERENCED_PARAMETER(DriverObject);
	PsSetCreateProcessNotifyRoutineEx(OnProcessCallback, TRUE);
	if (CallbackRegistrationHandle != NULL) {
		ObUnRegisterCallbacks(CallbackRegistrationHandle);
		CallbackRegistrationHandle = NULL;
	}
	DbgPrint("========== Guardian Unload ==========\n");
}