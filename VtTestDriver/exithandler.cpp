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
	// ע�⣺1. �㺯�����治Ҫ�þֲ���������Ϊʹ�õ�ebp�����㺯����ά��������ֶ�ά������ô����ͨ������ʲôȴ��
	//       2. �㺯�� ��Ϊ�����Ǹ��õĿ��ƽ�����һ�� �Ĵ����ȵ� ��ȡ������
	//		 3. �㺯�� ��ò�Ҫ̫���ࣻ���� �кܶ�����Ļ������ �����װһ�����������㺯���е��ü��ɡ�(�����װ��VMMEntryPointEbd()����)
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
