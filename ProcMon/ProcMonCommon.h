#pragma once
// Event types that our driver supports
typedef enum _EventType {
	ProcessCreate,
	ProcessExit,
	ThreadCreate,
	ThreadExit
}EventType;
// Header for every event
typedef struct _EventHeader {
	EventType Type;
	ULONG Size;
	ULONG64 TimeStamp;
}EventHeader;
// Data structure for event
typedef struct _ProcessCreateInfo {
	ULONG ProcessId;
	ULONG ParentProcessId;
	ULONG CreatingProcessId;
	ULONG CommandLineLength;
	WCHAR CommandLine[1];
}ProcessCreateInfo;
// Data structure for event
typedef struct _ProcessExitInfo {
	ULONG ProcessId;
	ULONG ExitCode;
}ProcessExitInfo;
// Data structure for event
typedef struct _ThreadCreateInfo {
	ULONG ProcessId;
	ULONG ThreadId;
}ThreadCreateInfo;
// Data structure for event
typedef struct _ThreadExitInfo {
	ThreadCreateInfo CommonInfo;
	ULONG ExitCode;
}ThreadExitInfo;
// Event data structure for storing data and sharing with client
typedef struct _EventData {
	EventHeader Header;
	union {
		ProcessCreateInfo ProcessCreate;
		ProcessExitInfo ProcessExit;
		ThreadCreateInfo ThreadCreate;
		ThreadExitInfo ThreadExit;
	};
}EventData;

