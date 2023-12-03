#include "pch.h"
#include "ProcMon.h"

ProcMonState g_State;

void OnThreadCallback(
	HANDLE ProcessId,
	HANDLE ThreadId,
	BOOLEAN Create
);

void OnProcessCallback(
	PEPROCESS Process,
	HANDLE ProcessId,
	PPS_CREATE_NOTIFY_INFO CreateInfo
);

void ProcMonUnload(PDRIVER_OBJECT DriverObject);
void AddItem(FullEventData* item);
NTSTATUS ProcMonRead(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS ProcMonCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status, ULONG_PTR info);

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);
	NTSTATUS status = STATUS_SUCCESS;
	PDEVICE_OBJECT devObj = NULL;

	BOOLEAN symLinkCreated = FALSE , procNotifyCreated = FALSE;
	UNICODE_STRING symName = RTL_CONSTANT_STRING(DRIVER_SYMLINKNAME);

	// Initilize and create our device driver
	do {
		UNICODE_STRING name = RTL_CONSTANT_STRING(DRIVER_DEVICENAME);
		status = IoCreateDevice(DriverObject, 0, &name, FILE_DEVICE_UNKNOWN, 0, FALSE, &devObj);
		if (!NT_SUCCESS(status))
		{
			KdPrint((DRIVER_PREFIX "Failed in IoCreateDevice (0x%X)\n", status));
			break;
		}
		devObj->Flags |= DO_DIRECT_IO; // Flag to support direct I/O

		status = IoCreateSymbolicLink(&symName, &name);
		if (!NT_SUCCESS(status))
		{
			KdPrint((DRIVER_PREFIX "Failed in IoCreateSymbolicLink (0x%X)\n", status));
			break;
		}
		symLinkCreated = TRUE;
		status = PsSetCreateProcessNotifyRoutineEx(OnProcessCallback, FALSE); // Registers a callback for process creation
		if (!NT_SUCCESS(status))
		{
			break;
		}
		procNotifyCreated = TRUE;
		status = PsSetCreateThreadNotifyRoutine(OnThreadCallback); // Registers a callback for thread creation
		if (!NT_SUCCESS(status))
		{
			break;
		}
	} while (FALSE);

	if (!NT_SUCCESS(status)) // Clean up if something failed
	{
		KdPrint((DRIVER_PREFIX "Failed in DriverEntry (0x%X)\n", status));
		if (devObj)
			IoDeleteDevice(devObj);
		if (symLinkCreated)
			IoDeleteSymbolicLink(&symName);
		if (procNotifyCreated)
			PsSetCreateProcessNotifyRoutineEx(OnProcessCallback, TRUE);
		return status;
	}

	FastMutex_Init(&g_State.Lock); // Initializes fast mutex
	InitializeListHead(&g_State.ItemsHead); // Initializes linked list

	DriverObject->DriverUnload = ProcMonUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverObject->MajorFunction[IRP_MJ_CLOSE] =
		ProcMonCreateClose;
	DriverObject->MajorFunction[IRP_MJ_READ] = ProcMonRead;

	return status;
}

void OnProcessCallback(PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo)
{
	if (CreateInfo) // If not null then process is being created otherwise process is being destroyed
	{
		KdPrint((DRIVER_PREFIX "Process %u created\n", HandleToULong(ProcessId)));
		USHORT commandLineLength = 0;
		if (CreateInfo->CommandLine)
			commandLineLength = CreateInfo->CommandLine->Length; // All unicode strings are limited to 32k bytes because unicode strings are 16 bit

		SIZE_T size = sizeof(FullEventData) + commandLineLength;
		FullEventData* item = (FullEventData*)ExAllocatePoolWithTag(PagedPool, size, DRIVER_TAG);
		if (item == NULL) // If system runs out of memory then simply exit, we cannot do anything at this point
		{
			KdPrint((DRIVER_PREFIX "Out of memory\n"));
			return;
		}
		EventHeader* header = &item->Data.Header;
		KeQuerySystemTimePrecise((PLARGE_INTEGER)&header->TimeStamp); // Gets system time
		header->Size = sizeof(EventHeader) + sizeof(ProcessCreateInfo) + commandLineLength; // The size that gets copied to the client and not the size that we allocated!
		header->Type = ProcessCreate; // enum
		// Event structure that will hold the info
		ProcessCreateInfo* data = &item->Data.ProcessCreate;
		data->ProcessId = HandleToULong(ProcessId);
		data->ParentProcessId = HandleToULong(CreateInfo->ParentProcessId);
		data->CreatingProcessId = HandleToULong(CreateInfo->CreatingThreadId.UniqueProcess);
		data->CommandLineLength = commandLineLength / sizeof(WCHAR); // Instead of providing the size in bytes we provide it in the number of characters
		if (commandLineLength)
			memcpy(data->CommandLine, CreateInfo->CommandLine->Buffer, commandLineLength);

		AddItem(item);
	}
	else 
	{
		KdPrint((DRIVER_PREFIX "Process %u exited\n", HandleToULong(ProcessId)));
		SIZE_T size = sizeof(FullEventData);
		FullEventData* item = (FullEventData*)ExAllocatePoolWithTag(PagedPool, size, DRIVER_TAG);
		if (item == NULL)
		{
			KdPrint((DRIVER_PREFIX "Out of memory\n"));
			return;
		}
		EventHeader* header = &item->Data.Header;
		KeQuerySystemTimePrecise((PLARGE_INTEGER)&header->TimeStamp);
		header->Size = sizeof(ProcessExitInfo) + sizeof(EventHeader);
		header->Type = ProcessExit;
		ProcessExitInfo* data = &item->Data.ProcessExit;
		data->ProcessId = HandleToULong(ProcessId);
		data->ExitCode = PsGetProcessExitStatus(Process);

		AddItem(item);
	}
}

void OnThreadCallback(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN Create)
{
	FullEventData* item = (FullEventData*)ExAllocatePoolWithTag(PagedPool, sizeof(FullEventData), DRIVER_TAG);
	if (item == NULL)
	{
		KdPrint((DRIVER_PREFIX "Out of memory\n"));
		return;
	}
	EventHeader* header = &item->Data.Header;
	KeQuerySystemTimePrecise((PLARGE_INTEGER)&header->TimeStamp);
	header->Size = (Create ? sizeof(ThreadCreateInfo) : sizeof(ThreadExitInfo)) + sizeof(EventHeader);
	header->Type = Create ? ThreadCreate : ThreadExit;
	ThreadExitInfo* data = &item->Data.ThreadExit;
	data->CommonInfo.ProcessId = HandleToUlong(ProcessId);
	data->CommonInfo.ThreadId = HandleToUlong(ThreadId);

	if (!Create)
	{
		PETHREAD thread;
		NTSTATUS status = PsLookupThreadByThreadId(ThreadId, &thread);
		if (NT_SUCCESS(status)) {
			data->ExitCode = PsGetThreadExitStatus(thread);
			ObfDereferenceObject(thread);
		}
	}
	AddItem(item);
}

NTSTATUS ProcMonRead(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = STATUS_SUCCESS;
	ULONG_PTR info = 0;
	do {
		ULONG len = irpSp->Parameters.Read.Length;
		if (len < sizeof(FullEventData)) {
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}
		NT_ASSERT_ASSUME(Irp->MdlAddress);
		PUCHAR buffer = (PUCHAR)MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
		if (!buffer) {
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		FastMutex_Lock(&g_State.Lock);
		while (!IsListEmpty(&g_State.ItemsHead)) {
			PLIST_ENTRY link = g_State.ItemsHead.Flink;
			FullEventData* item = CONTAINING_RECORD(link, FullEventData, Link);
			ULONG size = item->Data.Header.Size;
			if (size > len)
				break;

			memcpy(buffer, &item->Data, size);
			buffer += size;
			len -= size;
			info += size;
			g_State.ItemCount--;
			link = RemoveHeadList(&g_State.ItemsHead);
			ExFreePoolWithTag(CONTAINING_RECORD(link, FullEventData, Link), 0);
		}
		FastMutex_Unlock(&g_State.Lock);

	} while (FALSE);

	return CompleteRequest(Irp, status, info);
}

void ProcMonUnload(PDRIVER_OBJECT DriverObject)
{
	KdPrint((DRIVER_PREFIX "Driver Unloaded\n"));
	PsRemoveCreateThreadNotifyRoutine(OnThreadCallback);
	PsSetCreateProcessNotifyRoutineEx(OnProcessCallback, TRUE);
	UNICODE_STRING symName = RTL_CONSTANT_STRING(DRIVER_SYMLINKNAME);

	IoDeleteSymbolicLink(&symName);
	IoDeleteDevice(DriverObject->DeviceObject);

	while (!IsListEmpty(&g_State.ItemsHead))
	{
		PLIST_ENTRY link = RemoveHeadList(&g_State.ItemsHead);
		ExFreePoolWithTag(CONTAINING_RECORD(link, FullEventData, Link), 0);
	}
}

void AddItem(FullEventData* item)
{
	FastMutex_Lock(&g_State.Lock);
	InsertTailList(&g_State.ItemsHead, &item->Link);
	g_State.ItemCount++;
	FastMutex_Unlock(&g_State.Lock); // If you forget about unlocking the mutex you will get deadlocks
}

NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status, ULONG_PTR info)
{
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = info;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

NTSTATUS ProcMonCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	return CompleteRequest(Irp, STATUS_SUCCESS, 0);
}