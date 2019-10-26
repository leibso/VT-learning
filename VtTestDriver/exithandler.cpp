#include "stdafx.h"
#include "exithandler.h"
#include "vtsystem.h"
#include "vtasm.h"

GUEST_REGS g_GuestRegs;

static void VMMEntryPointEbd(void)
{
	ULONG ExitReason;
	ExitReason = Vmx_VmRead(VM_EXIT_REASON);
	g_GuestRegs.esp = Vmx_VmRead(GUEST_RSP);
	g_GuestRegs.eip = Vmx_VmRead(GUEST_RIP);
	Log("g_GuestRegs.eip:",g_GuestRegs.eip);
	__asm int 3;// Interrupt Here;Give the chance to view the Info
}

void __declspec(naked) VMMEntryPoint(void)
	// 注意：1. 裸函数里面不要用局部变量；因为使用到ebp；而裸函数不维护，如果手动维护，那么和普通函数有什么却别；
	//       2. 裸函数 是为了我们更好的控制进来那一刻 寄存器等的 获取、设置
	//		 3. 裸函数 最好不要太冗余；所以 有很多操作的话，最好 另外封装一个函数，在裸函数中调用即可。(这里封装了VMMEntryPointEbd()函数)
{
	// Refresh selector -- >underneath part-- gdtinfo;
	//do Exchange itself can refresh the VM TLB when selector right ;
	__asm{
		mov ax,fs;
		mov fs,ax;
		
		mov ax,gs;
		mov gs,ax;
		
	}

	// Call  the Func  to show the ExitReason and regs!
	VMMEntryPointEbd();

}
