#include <Windows.h>
#include <stdio.h>
#include <winternl.h>
#include "..\booster\boosterCommon.h"

#pragma comment(lib, "ntdll")

int main(int argc, const char* argv[])
{
	if (argc < 3)
	{
		wprintf(L"Usage: boost <tid> <priority>\n");
		return 0;
	}

	int tid = atoi(argv[1]);
	int priority = atoi(argv[2]);

	NTSTATUS status;
	HANDLE hDevice, eventHandle = NULL;
	OBJECT_ATTRIBUTES deviceAttr;
	UNICODE_STRING deviceName;
	IO_STATUS_BLOCK ioStatus;
	RtlInitUnicodeString(&deviceName, L"\\Device\\Booster");
	InitializeObjectAttributes(&deviceAttr, &deviceName, 0, NULL, NULL);
	wprintf(L"[^] Trying to open a handle to %ws\n", deviceName.Buffer);
	status = NtCreateFile(&hDevice, GENERIC_WRITE | GENERIC_READ, &deviceAttr, &ioStatus, NULL, FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);

	if (status == 0)
	{
		wprintf(L"[^] Successfully got a handle => %p\n", hDevice);
		DWORD written = 0;
		ThreadData data;
		data.ThreadId = tid;
		data.Priority = priority;

		status = NtDeviceIoControlFile(hDevice, eventHandle, NULL, NULL, &ioStatus, 0x3005, &data, sizeof(ThreadData), &written, 0);

		if (status == 0)
		{
			wprintf(L"[^] Success!!\n");
		}
		else
		{
			wprintf(L"[^] Unsuccessful!!\n");
		}

		// Close Handle when finished
		NtClose(hDevice);
	}
	else
	{
		wprintf(L"[!] NtOpenFile failed with 0x%0x\n", status);
		return -1;
	}

	return 0;
}