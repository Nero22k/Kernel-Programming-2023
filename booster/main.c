#include <ntifs.h>
#include "boosterCommon.h"

void BoosterUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS BoosterCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS BoosterWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp);

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);
	DriverObject->MajorFunction[IRP_MJ_CREATE] =
		DriverObject->MajorFunction[IRP_MJ_CLOSE] = BoosterCreateClose;
	DriverObject->MajorFunction[IRP_MJ_WRITE] = BoosterWrite;

	DriverObject->DriverUnload = BoosterUnload;

	UNICODE_STRING devName;
	RtlInitUnicodeString(&devName, L"\\Device\\Booster");
	PDEVICE_OBJECT devObj;
	NTSTATUS status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &devObj);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("Error creating device (0x%X)\n", status));
		return status;
	}
	//devObj->Flags |= DO_DIRECT_IO; // Works only for MJ_READ and MJ_WRITE

	UNICODE_STRING symName;
	RtlInitUnicodeString(&symName, L"\\??\\Booster");
	status = IoCreateSymbolicLink(&symName, &devName);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("Error creating symbolic link (0x%X)\n", status));
		IoDeleteDevice(devObj);
		return status;
	}

	return status;
}

void BoosterUnload(PDRIVER_OBJECT DriverObject)
{
	KdPrint(("Unload (0x%p)\n", DriverObject));
	UNICODE_STRING symName;
	RtlInitUnicodeString(&symName, L"\\??\\Booster");
	IoDeleteSymbolicLink(&symName);
	IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS BoosterCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IofCompleteRequest(Irp, 0);

	return STATUS_SUCCESS;
}

NTSTATUS BoosterWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	IO_STACK_LOCATION* irpSp = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = STATUS_SUCCESS;
	ULONG info = 0;
	do
	{
		if (irpSp->Parameters.Write.Length < sizeof(ThreadData))
		{
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}

		ThreadData *data = (ThreadData*)Irp->AssociatedIrp.SystemBuffer; //Irp->UserBuffer;
		if (data == NULL || data->Priority < 1 || data->Priority > 31)
		{
			status = STATUS_INVALID_PARAMETER;
			break;
		}
		PETHREAD thread;
		//(HANDLE)(ULONG_PTR) or ULongToHandle()
		status = PsLookupThreadByThreadId(ULongToHandle(data->ThreadId), &thread); // This function will increment threads reference count so that the thread will not die
		if (!NT_SUCCESS(status))
			break;
		KPRIORITY oldPriority = KeSetPriorityThread(thread, data->Priority);
		KdPrint(("Changed priority of thread %u from %d to %d\n", data->ThreadId, oldPriority, data->Priority));
		ObDereferenceObject(thread); // Once we are done we have to dereference the thread object to decrement the ref count so that it doesn't leak
		info = sizeof(ThreadData);
	} while (FALSE);

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = info;
	IofCompleteRequest(Irp, 0);

	return status;
}
