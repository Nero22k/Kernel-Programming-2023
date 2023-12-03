#include <Windows.h>
#include <stdio.h>
#include <ntddbeep.h>
#include <winternl.h>

#pragma comment(lib, "ntdll")

typedef struct _Note
{
	DWORD Freq;
	DWORD Duration;
} Note;

int main()
{
	wprintf(L"[*] Simple program for communicating with Beep.sys driver [*]\n");
	NTSTATUS status = NULL;
	HANDLE hDevice, eventHandle = NULL;
	OBJECT_ATTRIBUTES deviceAttr;
	UNICODE_STRING deviceName;
	IO_STATUS_BLOCK ioStatus;
	RtlInitUnicodeString(&deviceName, L"\\Device\\Beep");
	InitializeObjectAttributes(&deviceAttr, &deviceName, 0, NULL, NULL);
	wprintf(L"[^] Trying to open a handle to %ws\n", deviceName.Buffer);
	status = NtOpenFile(&hDevice, GENERIC_WRITE, &deviceAttr, &ioStatus, 0, 0);

	if (status == 0)
	{
		wprintf(L"[^] Successfully got a handle => %p\n", hDevice);
		DWORD written = 0;
		Note note;
		note.Freq = 800;
		note.Duration = 1000;
		status = NtDeviceIoControlFile(hDevice, eventHandle, NULL, NULL, &ioStatus, IOCTL_BEEP_SET, &note, sizeof(Note), &written, 0);

		if (status == STATUS_PENDING)
		{
			NtWaitForSingleObject(eventHandle, FALSE, NULL);
			status = ioStatus.Status;
		}

		if (status == 0)
		{
			wprintf(L"[^] Beep successful\n");
			Sleep(500);
		}
		else
		{
			wprintf(L"[!] NtDeviceIoControlFile failed with 0x%0x\n", status);
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