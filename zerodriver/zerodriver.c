#include <ntifs.h>
#include "ZeroCommon.h"

#define DRIVER_PREFIX "Zero: "

void ZeroUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS ZeroCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS ZeroRead(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS ZeroWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS ZeroDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status, ULONG_PTR info);

ZeroStats g_Stats; // Non-paged memory (Safe To Access)

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);
	NTSTATUS status;
	PDEVICE_OBJECT devObj = NULL;
	do {
		UNICODE_STRING name = RTL_CONSTANT_STRING(L"\\Device\\Zero");
		status = IoCreateDevice(DriverObject, 0, &name, FILE_DEVICE_UNKNOWN, 0, FALSE, &devObj);
		if (!NT_SUCCESS(status))
		{
			KdPrint((DRIVER_PREFIX "Failed in IoCreateDevice (0x%X)\n", status));
			break;
		}
		devObj->Flags |= DO_DIRECT_IO;
		UNICODE_STRING link = RTL_CONSTANT_STRING(L"\\??\\Zero");
		status = IoCreateSymbolicLink(&link, &name);
		if (!NT_SUCCESS(status))
		{
			KdPrint((DRIVER_PREFIX "Failed in IoCreateSymbolicLink (0x%X)\n", status));
			break;
		}
	} while (FALSE);

	if (!NT_SUCCESS(status))
	{
		if (devObj)
			IoDeleteDevice(devObj);
		return status;
	}

	DriverObject->DriverUnload = ZeroUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] =
		DriverObject->MajorFunction[IRP_MJ_CLOSE] =
		ZeroCreateClose;
	DriverObject->MajorFunction[IRP_MJ_READ] = ZeroRead;
	DriverObject->MajorFunction[IRP_MJ_WRITE] = ZeroWrite;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ZeroDeviceControl;

	return status;
}

NTSTATUS ZeroDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	IO_STACK_LOCATION* irpSp = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
	ULONG info = 0;
	switch (irpSp->Parameters.DeviceIoControl.IoControlCode){
	case IOCTL_ZERO_ZERO_STATS:
		g_Stats.BytesRead = g_Stats.BytesWritten = 0;
		status = STATUS_SUCCESS;
		break;
	case IOCTL_ZERO_GET_STATS:
		if (irpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof(ZeroStats))
		{
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}
		ZeroStats *buffer = (ZeroStats*)Irp->AssociatedIrp.SystemBuffer;
		if (buffer == NULL)
		{
			status = STATUS_INVALID_PARAMETER;
			break;
		}
		*buffer = g_Stats;
		status = STATUS_SUCCESS;
		info = sizeof(ZeroStats);
		break;
	}
	return CompleteRequest(Irp, status, info);
}

void ZeroUnload(PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING link = RTL_CONSTANT_STRING(L"\\??\\Zero");
	IoDeleteSymbolicLink(&link);
	IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS ZeroCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	return CompleteRequest(Irp, STATUS_SUCCESS, 0);
}

NTSTATUS ZeroRead(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	NT_ASSERT(Irp->MdlAddress);
	IO_STACK_LOCATION* irpSp = IoGetCurrentIrpStackLocation(Irp);
	ULONG len = irpSp->Parameters.Read.Length;

	if(len == 0)
		return CompleteRequest(Irp, STATUS_SUCCESS, 0);

	PVOID buffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
	if (buffer == NULL)
		return CompleteRequest(Irp, STATUS_INSUFFICIENT_RESOURCES, 0);

	RtlZeroMemory(buffer, len);
	_InlineInterlockedAdd64(&g_Stats.BytesRead, len);
	return CompleteRequest(Irp, STATUS_SUCCESS, len);
}

NTSTATUS ZeroWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	ULONG len = IoGetCurrentIrpStackLocation(Irp)->Parameters.Write.Length;
	_InlineInterlockedAdd64(&g_Stats.BytesWritten, len); //Thread safe function
	return CompleteRequest(Irp, STATUS_SUCCESS, len);
}

NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status, ULONG_PTR info)
{
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = info;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}