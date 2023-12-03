#include <Windows.h>
#include <stdio.h>
#include "..\ProcMon\ProcMonCommon.h"

void DisplayTime(ULONG64 time)
{
	FILETIME ft;
	ft.dwLowDateTime = (DWORD)time;
	ft.dwHighDateTime = (DWORD)(time >> 32);
	FileTimeToLocalFileTime(&ft, &ft);
	SYSTEMTIME st;
	FileTimeToSystemTime(&ft, &st);
	wprintf(L"%02d:%02d:%02d.%03d ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

void DisplayData(const BYTE* buffer, DWORD size)
{
	while (size > 0)
	{
		EventData* data = (EventData*)buffer;
		DisplayTime(data->Header.TimeStamp);
		switch (data->Header.Type)
		{
		case ProcessExit:
		{
			ProcessExitInfo* info = &data->ProcessExit;
			wprintf(L"Process Exit: PID: %u Exit code: %u\n", info->ProcessId, info->ExitCode);
			break;
		}
		case ProcessCreate:
		{
			ProcessCreateInfo* info = &data->ProcessCreate;
			wprintf(L"Process Create: PID: %u PPID: %u CPID: %u Command Line: %.*ws\n", info->ProcessId, info->ParentProcessId, info->CreatingProcessId, info->CommandLineLength, info->CommandLine);
			break;
		}
		case ThreadCreate:
		{
			ThreadCreateInfo* info = &data->ThreadCreate;
			wprintf(L"Thread Create: TID: %u PID: %u\n", info->ThreadId, info->ProcessId);
			break;
		}
		case ThreadExit:
		{
			ThreadExitInfo* info = &data->ThreadExit;
			wprintf(L"Thread Exit: TID: %u PID: %u Exit code: %u\n", info->CommonInfo.ThreadId, info->CommonInfo.ProcessId, info->ExitCode);
			break;
		}
		}
		buffer += data->Header.Size;
		size -= data->Header.Size;
	}
}

int main()
{
	HANDLE hDevice = CreateFileW(L"\\\\.\\ProcMon", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);

	if (hDevice == INVALID_HANDLE_VALUE)
	{
		wprintf(L"Error in CreateFile (%u)\n", GetLastError());
	}

	BYTE buffer[1 << 16]; // 2^16 = 64KB

	for (;;)
	{
		DWORD readBytes;
		if (!ReadFile(hDevice, buffer, sizeof(buffer), &readBytes, NULL))
			break;

		if(readBytes)
			DisplayData(buffer, readBytes);


		Sleep(500); // Delay for pulling data
	}

	CloseHandle(hDevice);
	return 0;
}