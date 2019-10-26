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
	// ע�⣺1. �㺯�����治Ҫ�þֲ���������Ϊʹ�õ�ebp�����㺯����ά��������ֶ�ά������ô����ͨ������ʲôȴ��
	//       2. �㺯�� ��Ϊ�����Ǹ��õĿ��ƽ�����һ�� �Ĵ����ȵ� ��ȡ������
	//		 3. �㺯�� ��ò�Ҫ̫���ࣻ���� �кܶ�����Ļ������ �����װһ�����������㺯���е��ü��ɡ�(�����װ��VMMEntryPointEbd()����)
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
