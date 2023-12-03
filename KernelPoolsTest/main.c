#include <ntifs.h>
#pragma warning(disable : 4201)
#pragma warning(disable : 4996)

// Global pointers to store allocated memory addresses
PVOID g_NonPagedMemory = NULL;
PVOID g_PagedMemory = NULL;
PVOID g_PagedMemorySmall = NULL;

void DriverUnload(PDRIVER_OBJECT);

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	DbgPrint("HelloWorld from the Kernel Land!\n");
	DbgPrint("Driver Object:\t\t0x%p\n", DriverObject);
	DbgPrint("Registry Path:\t\t0x%p\n", RegistryPath);

	// Allocate memory from the Non-Paged Pool
	g_NonPagedMemory = ExAllocatePoolWithTag(NonPagedPool, 5800, 'Tag1');
	if (g_NonPagedMemory != NULL) {
		DbgPrint("Allocated Non-Paged Pool memory at address: %p\n", g_NonPagedMemory);
	}

	// Allocate memory from the Paged Pool
	g_PagedMemory = ExAllocatePoolWithTag(PagedPool, 5800, 'Tag2');
	if (g_PagedMemory != NULL) {
		DbgPrint("Allocated Paged Pool memory at address: %p\n", g_PagedMemory);
	}

	// Allocate memory from the Paged Pool
	g_PagedMemorySmall = ExAllocatePoolWithTag(PagedPool, 500, 'Tag2');
	if (g_PagedMemorySmall != NULL) {
		DbgPrint("Allocated Second Paged Pool (500 bytes) memory at address: %p\n", g_PagedMemorySmall);
	}

	DbgPrint("Sample driver loaded\n");

	DriverObject->DriverUnload = DriverUnload;

	return STATUS_SUCCESS;
}

void DriverUnload(PDRIVER_OBJECT DriverObject)
{
	UNREFERENCED_PARAMETER(DriverObject);
	// Free Non-Paged Pool memory
	if (g_NonPagedMemory != NULL) {
		ExFreePool(g_NonPagedMemory);
		DbgPrint("Freed Non-Paged Pool memory\n");
	}

	// Free Paged Pool memory
	if (g_PagedMemory != NULL && g_PagedMemorySmall != NULL) {
		ExFreePool(g_PagedMemorySmall);
		ExFreePool(g_PagedMemory);
		DbgPrint("Freed Paged Pool memory\n");
	}

	DbgPrint("Sample driver unloaded\n");
}