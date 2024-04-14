#include "Defs.h"

void* CallbackRegistrationHandle = NULL;
BOOLEAN CallbackIsSet = FALSE;
int ProtectedPid = 0;

typedef PCHAR(*PFN_PsGetProcessImageFileName)(PEPROCESS Process); // This is a function pointer to PsGetProcessImageFileName
/*
* This function is called when a process is created or terminated
*/
VOID OnProcessCallback(PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo) 
{
	if (CreateInfo) // If not null then it means new process
	{
		if (!CallbackIsSet)
		{
			WCHAR buffer[260];
			UNICODE_STRING targetProcess;
			RtlInitUnicodeString(&targetProcess, TARGET_PROCESS);

			// Copy and null-terminate the string
			ULONG length = min(CreateInfo->ImageFileName->Length / sizeof(WCHAR), 260 - 1); // This gets the length of the string
			RtlCopyMemory(buffer, CreateInfo->ImageFileName->Buffer, length * sizeof(WCHAR)); // This copies the string to our buffer
			buffer[length] = L'\0'; // Null-terminate

			if (wcsstr(buffer, targetProcess.Buffer) != NULL) // Check if the process name is the same as our target process
			{
				CallbackIsSet = TRUE;
				ProtectedPid = (int)HandleToULong(ProcessId);
				DbgPrint("Target found! EPROCESS: 0x%p => id: %d cid: 0x%X\n", Process, ProtectedPid, ProcessId);
				enableProtection();
			}
		}
	}
	else
	{
		if (CallbackIsSet && ProtectedPid == (int)HandleToULong(ProcessId))
		{
			DbgPrint("Target process exited %d\n", ProtectedPid);
			CallbackIsSet = FALSE;
			disableProtection();
		}
	}
	// Add code to remove ObRegisterCallbacks when notepad is closed otherwise we will get "(NTSTATUS) 0xc01c0011 - An instance already exists at this altitude on the volume specified."

}

OB_PREOP_CALLBACK_STATUS PreOperationCallback(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION OperationInformation) {
	PEPROCESS	TargetProcess = OperationInformation->Object; // Points to our notepad.exe process
	PEPROCESS	CurrentProcess = PsGetCurrentProcess(); // Points to processes that open handles to our Target Process
	HANDLE		TargetPid = PsGetProcessId(TargetProcess);
	HANDLE      CurrentProcessPid = PsGetProcessId(CurrentProcess);

	//Allow operations from the process itself
	if (CurrentProcess == TargetProcess) {
		return OB_PREOP_SUCCESS;
	}

	//Allow operations from the kernel
	if (OperationInformation->KernelHandle == 1) {
		return OB_PREOP_SUCCESS;
	}

	//Ignore other processes
	if (TargetPid != (HANDLE)(*(int*)RegistrationContext)) {
		return OB_PREOP_SUCCESS;
	}
	else 
	{
		/*
		* We need to figure it out how to check only for handles that are open to terminate our process
		* probably we need to check the access type or something that will tell us that this handle is the terminate handle
		* We also recieve false positives from explorer.exe and csrss.exe because they also open handles with full access rights
		*/
		UNICODE_STRING routineName;
		RtlInitUnicodeString(&routineName, L"PsGetProcessImageFileName");
		PFN_PsGetProcessImageFileName pPsGetProcessImageFileName = (PFN_PsGetProcessImageFileName)MmGetSystemRoutineAddress(&routineName);

		if (pPsGetProcessImageFileName != NULL)
		{
			PCHAR processName = pPsGetProcessImageFileName(CurrentProcess);
			if (strcmp(processName, "explorer.exe") != 0 && strcmp(processName, "csrss.exe") != 0)
			{
				DbgPrint("Attempt to terminate process by PID: %lu\n", HandleToULong(CurrentProcessPid));
				//OperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_TERMINATE;
				OperationInformation->Parameters->CreateHandleInformation.DesiredAccess = SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION;
				OperationInformation->Parameters->DuplicateHandleInformation.DesiredAccess = SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION;
			}
		}
	}

	return OB_PREOP_SUCCESS;
}

VOID enableProtection() // Double check this to make sure its setup properly
{
    UNICODE_STRING ustrAltitude = { 0 };
	OB_OPERATION_REGISTRATION OperationRegistrations[1] = { { 0 } };
	OB_CALLBACK_REGISTRATION CallbackRegistration = { 0 };

    // Set up operation registration for process handles
    OperationRegistrations[0].ObjectType = PsProcessType;
    OperationRegistrations[0].Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;
    OperationRegistrations[0].PreOperation = PreOperationCallback;
    OperationRegistrations[0].PostOperation = PostOperationCallback;

    // Set up callback registration
    RtlInitUnicodeString(&ustrAltitude, L"1000");
    CallbackRegistration.Version = OB_FLT_REGISTRATION_VERSION;
    CallbackRegistration.OperationRegistrationCount = 1;
    CallbackRegistration.Altitude = ustrAltitude;
    CallbackRegistration.RegistrationContext = (PVOID)&ProtectedPid;
    CallbackRegistration.OperationRegistration = OperationRegistrations;

    NTSTATUS status = ObRegisterCallbacks(&CallbackRegistration, &CallbackRegistrationHandle); // save the registration handle to remove callbacks later
    if (!NT_SUCCESS(status)) {
        DbgPrint("ObRegisterCallbacks() failed! status: 0x%p", status);
    }
}

VOID disableProtection()
{
	if (CallbackRegistrationHandle != NULL) 
	{
		ObUnRegisterCallbacks(CallbackRegistrationHandle);
		CallbackRegistrationHandle = NULL;
	}
	DbgPrint("Disabled Protection!");
}

VOID PostOperationCallback( PVOID RegistrationContext, POB_POST_OPERATION_INFORMATION PostInfo)
{
	UNREFERENCED_PARAMETER(RegistrationContext);
	UNREFERENCED_PARAMETER(PostInfo);
}