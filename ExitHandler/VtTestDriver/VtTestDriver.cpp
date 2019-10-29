#include "stdafx.h"
EXTERN_C void Asm_xx();
EXTERN_C BOOLEAN IsVTEnabled();
EXTERN_C NTSTATUS StartVirtualTechnology();
EXTERN_C NTSTATUS StopVirtualTechnology();
VOID DriverUnLoad(PDRIVER_OBJECT driver)
{

	StopVirtualTechnology();

	DbgPrint("VT stopping Success! ....!\r\n");
	DbgPrint("Driver is unloading...r\n");
}
// the return location
ULONG g_RetDriver_Esp;
ULONG g_RetDriver_Eip;


NTSTATUS DriverEntry(
	PDRIVER_OBJECT driver ,
	PUNICODE_STRING RegistryPath)
{
	Log("Driver Entry In.....!",0);
	Asm_xx();
	Log("VT running.....!",0);

	// Store the context before entry!!!
	__asm{
		pushad;
		pushfd;
		mov g_RetDriver_Esp,esp;
		mov g_RetDriver_Eip,offset RET_EIP;
	}

	StartVirtualTechnology();

	// =============== If runs well  ,VT running is not run the under codings, 
	//					 until  it ends or come Exception!
	__asm{
RET_EIP:
		popfd;
		popad;
	}

	Log("VT ending....!",0);
	Log("DriverEntry ending....!",0);
	driver->DriverUnload = DriverUnLoad;
	return STATUS_SUCCESS;
}
