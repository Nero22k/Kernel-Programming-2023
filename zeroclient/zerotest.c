#include <Windows.h>
#include <stdio.h>
#include <winternl.h>
#include "..\zerodriver\ZeroCommon.h"

#pragma comment(lib, "ntdll")

void DisplayBuffer(const BYTE *buff, SIZE_T size)
{
	for (int i = 0; i < size; i++)
	{
		wprintf(L"%02X ", buff[i]);
		if (i % 16 == 15)
			wprintf(L"\n");
	}
	wprintf(L"\n");
}

int main(int argc, const char* argv[])
{
	NTSTATUS status;
	HANDLE hDevice, eventHandle = NULL;
	OBJECT_ATTRIBUTES deviceAttr;
	UNICODE_STRING deviceName;
	IO_STATUS_BLOCK ioStatus;
	RtlInitUnicodeString(&deviceName, L"\\Device\\Zero");
	InitializeObjectAttributes(&deviceAttr, &deviceName, 0, NULL, NULL);
	wprintf(L"[^] Trying to open a handle to %ws\n", deviceName.Buffer);
	status = NtCreateFile(&hDevice, GENERIC_WRITE | GENERIC_READ, &deviceAttr, &ioStatus, NULL, FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);

	if (status == 0)
	{
		wprintf(L"[^] Successfully got a handle => %p\n", hDevice);
		DWORD written = 0;
		BYTE buffer[1000];
		

		if (WriteFile(hDevice, buffer, sizeof(buffer), &written, NULL))
		{
			wprintf(L"[^] Written %u bytes\n", written);
		}
		else
		{
			wprintf(L"[!] Error in WriteFile (%u)\n", GetLastError());
		}

		BYTE buffer2[64];
		for (int i = 0; i < sizeof(buffer2); i++)
			buffer2[i] = (BYTE)i + 1;
		DisplayBuffer(buffer2, sizeof(buffer2));
		if (ReadFile(hDevice, buffer2, sizeof(buffer2), &written, NULL))
		{
			wprintf(L"[^] Bytes read: %u\n", written);
			DisplayBuffer(buffer2, sizeof(buffer2));
		}
		else
		{
			wprintf(L"[!] Error in ReadFile (%u)\n", GetLastError());
		}

		ZeroStats stats;
		status = NtDeviceIoControlFile(hDevice, NULL, NULL, NULL, &ioStatus, IOCTL_ZERO_GET_STATS, NULL, NULL, &stats, sizeof(stats));
		if (status == 0)
			wprintf(L"[^] Total read: %lld Total Written: %lld\n", stats.BytesRead, stats.BytesWritten);
	}
	else
	{
		wprintf(L"[!] NtCreateFile failed with 0x%0x\n", status);
		return -1;
	}

	return 0;
}