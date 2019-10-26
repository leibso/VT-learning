#include "stdafx.h"
EXTERN_C void Asm_xx();
EXTERN_C BOOLEAN IsVTEnabled();
EXTERN_C NTSTATUS StartVirtualTechnology();
EXTERN_C NTSTATUS StopVirtualTechnology();
VOID DriverUnLoad(PDRIVER_OBJECT driver)
{

	StopVirtualTechnology();
	DbgPrint("VT stopping ....!\r\n");
	DbgPrint("Driver is unloading...r\n");
}
NTSTATUS DriverEntry(
	PDRIVER_OBJECT driver ,
	PUNICODE_STRING RegistryPath)
{
	DbgPrint("Driver Entered!\r\n");
	Asm_xx();
	
	StartVirtualTechnology();
	DbgPrint("VT running....!\r\n");
	driver->DriverUnload = DriverUnLoad;
	return STATUS_SUCCESS;
}
