#include "stdafx.h"
#include "exithandler.h"
#include "vtsystem.h"
#include "vtasm.h"

GUEST_REGS g_GuestRegs;
extern ULONG ExitEip;

static void VMMEntryPointEbd(void)
{
	ULONG ExitReason;
	ExitReason = Vmx_VmRead(VM_EXIT_REASON);
	g_GuestRegs.esp = Vmx_VmRead(GUEST_RSP);
	g_GuestRegs.eip = Vmx_VmRead(GUEST_RIP);
	Log("g_GuestRegs.eip:",g_GuestRegs.eip);
	ExitEip = g_GuestRegs.eip;

	// load the exit info and jmp to continue handle
	__asm
	{
		mov  eax, g_GuestRegs.eax
		mov  ecx, g_GuestRegs.ecx
		mov  edx, g_GuestRegs.edx
		mov  ebx, g_GuestRegs.ebx
		mov  esp, g_GuestRegs.esp
		mov  ebp, g_GuestRegs.ebp
		mov  esi, g_GuestRegs.esi
		mov  edi, g_GuestRegs.edi
		push g_GuestRegs.eflags
		popfd;

		jmp g_GuestRegs.eip
	}
}

void __declspec(naked) VMMEntryPoint(void)
	// 注意：1. 裸函数里面不要用局部变量；因为使用到ebp；而裸函数不维护，如果手动维护，那么和普通函数有什么却别；
	//       2. 裸函数 是为了我们更好的控制进来那一刻 寄存器等的 获取、设置
	//		 3. 裸函数 最好不要太冗余；所以 有很多操作的话，最好 另外封装一个函数，在裸函数中调用即可。(这里封装了VMMEntryPointEbd()函数)
{
	// Refresh selector -- >underneath part-- gdtinfo;
	//do Exchange itself can refresh the VM TLB when selector right ;
	__asm{

		mov g_GuestRegs.eax, eax//  store the Exit regs
		mov g_GuestRegs.ecx, ecx
		mov g_GuestRegs.edx, edx
		mov g_GuestRegs.ebx, ebx
		mov g_GuestRegs.esp, esp
		mov g_GuestRegs.ebp, ebp
		mov g_GuestRegs.esi, esi
		mov g_GuestRegs.edi, edi
		pushfd
		pop eax
		mov g_GuestRegs.eflags,eax


		mov ax,fs;
		mov fs,ax;
		
		mov ax,gs;
		mov gs,ax;
		
	}

	// Call  the Func  to show the ExitReason and regs!
	VMMEntryPointEbd();

}
