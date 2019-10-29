#include "stdafx.h"
#include "exithandler.h"
#include "vtsystem.h"
#include "vtasm.h"

GUEST_REGS g_GuestRegs;
extern ULONG ExitEip;

// Handle CPUID
void HandleCPUID()// Details on Instruction Execution VM Exits!!
{
	if (g_GuestRegs.eax == 'Mini')// just test the param
	{
		g_GuestRegs.ebx = 0x88888888;
		g_GuestRegs.ecx = 0x11111111;
		g_GuestRegs.edx = 0x12345678;
	}
	// get the CPUID Info!(param,return1,return2,return3,return4) see IA32-指令集
	else Asm_CPUID(g_GuestRegs.eax, &g_GuestRegs.eax, &g_GuestRegs.ebx, &g_GuestRegs.ecx, &g_GuestRegs.edx);
}
extern ULONG g_VmCall_Arg;
extern ULONG g_Stop_Esp,g_Stop_Eip;
// Handle VmCall()
void HandleVmCall()
{
	if (g_VmCall_Arg == 'SVT')
	{
		Vmx_VmClear(g_VMXCPU.pVMCSRegion_PA.LowPart,g_VMXCPU.pVMCSRegion_PA.HighPart);
		Vmx_VmxOff();
		__asm{
			mov esp, g_Stop_Esp
				jmp g_Stop_Eip
		}
	} else {
		__asm int 3
	}
}


// Handle  the CR0~7  -- ControlRegiters! 
void HandleCrAccess()// details about EXIT_QUALIFICATION, refrence Chapter 27.2 VM exits.. Table 27-3
{
	ULONG		movcrControlRegister;
	ULONG		movcrAccessType;
	ULONG		movcrOperandType;
	ULONG		movcrGeneralPurposeRegister;
	ULONG		movcrLMSWSourceData;
	ULONG		ExitQualification;

	ExitQualification = Vmx_VmRead(EXIT_QUALIFICATION) ;
	movcrControlRegister = ( ExitQualification & 0x0000000F );// which CR
	movcrAccessType = ( ( ExitQualification & 0x00000030 ) >> 4 );// AccessType -- which Action triggered in （like mov to CR or mov from CR）
	movcrOperandType = ( ( ExitQualification & 0x00000040 ) >> 6 );// LMSW operand type，  register or memory?
	movcrGeneralPurposeRegister = ( ( ExitQualification & 0x00000F00 ) >> 8 );// for mov CR, operand  purpose Reg. (like mov eax,cr0, the Purpose Reg isEax)
	if( movcrControlRegister != 3 ){    // not for cr3
		__asm int 3
	}

	if (movcrAccessType == 0) {         // CR3 <-- reg32
		Vmx_VmWrite(GUEST_CR3, *(PULONG)((ULONG)&g_GuestRegs + 4 * movcrGeneralPurposeRegister));
	} else {                            // reg32 <-- CR3
		*(PULONG)((ULONG)&g_GuestRegs + 4 * movcrGeneralPurposeRegister) = Vmx_VmRead(GUEST_CR3);
	}
}

static void VMMEntryPointEbd(void)
{
	ULONG ExitReason;
	ULONG ExitInstructionLength;
	ULONG GuestResumeEIP;

	// 获取 进入 VMExit 的 原因
	ExitReason = Vmx_VmRead(VM_EXIT_REASON);
	// 获取导致  VMExit 的 Opcode 的长度
	ExitInstructionLength = Vmx_VmRead(VM_EXIT_INSTRUCTION_LEN);
	// 获取关键 现场环境
	g_GuestRegs.eflags = Vmx_VmRead(GUEST_RFLAGS);
	g_GuestRegs.esp = Vmx_VmRead(GUEST_RSP);
	g_GuestRegs.eip = Vmx_VmRead(GUEST_RIP);

	// 根据 Exit 原因类型，进行处理：
	switch(ExitReason)
	{
	case EXIT_REASON_CPUID:// Handle CPUID
		HandleCPUID();
		Log("EXIT_REASON_CPUID",0);
		break;
	case EXIT_REASON_VMCALL:// Handle VMCALL
		HandleVmCall();
		Log("EXIT_REASON_VMCALL",0);
		break;
	case EXIT_REASON_CR_ACCESS:
		HandleCrAccess();
		Log("EXIT_REASON_CR_ACCESS!",0);
		break;
	default:// As so far, the Exit_Reasons not gonna deal!
		Log("No setted Handler of Reason：%p",ExitReason);
		__asm int 3;
	}
	//Resume: 设置Guest 继续执行的位置环境
	// prepare for VM Resuming
	GuestResumeEIP = g_GuestRegs.eip + ExitInstructionLength;
	Vmx_VmWrite(GUEST_RIP,      GuestResumeEIP);
	Vmx_VmWrite(GUEST_RSP,      g_GuestRegs.esp);
	Vmx_VmWrite(GUEST_RFLAGS,   g_GuestRegs.eflags);
}

void __declspec(naked) VMMEntryPoint(void)
	// 注意：1. 裸函数里面不要用局部变量；因为使用到ebp；而裸函数不维护，如果手动维护，那么和普通函数有什么却别；
	//       2. 裸函数 是为了我们更好的控制进来那一刻 寄存器等的 获取、设置
	//		 3. 裸函数 最好不要太冗余；所以 有很多操作的话，最好 另外封装一个函数，在裸函数中调用即可。(这里封装了VMMEntryPointEbd()函数)
{
	// Refresh selector -- >underneath part-- gdtinfo;
	//do Exchange itself can refresh the VM TLB when selector right ;
	__asm{
		// 保存来时的环境
		mov g_GuestRegs.eax, eax
		mov g_GuestRegs.ecx, ecx
		mov g_GuestRegs.edx, edx
		mov g_GuestRegs.ebx, ebx
		mov g_GuestRegs.esp, esp
		mov g_GuestRegs.ebp, ebp
		mov g_GuestRegs.esi, esi
		mov g_GuestRegs.edi, edi

		pushfd
		pop eax
		mov g_GuestRegs.eflags, eax

		mov ax, fs// 通过切换段选择子，刷新段选择子对应的隐藏部分
		mov fs, ax
		mov ax, gs
		mov gs, ax
	}
	// **************************************************

	// between save and restore！
	// we can run the handler!!!*******************
	VMMEntryPointEbd();// In this function , it concluds the main operations.

	// **************************************************
	// Resume 
	__asm{
		// 恢复来时的环境
		mov  eax, g_GuestRegs.eax
		mov  ecx, g_GuestRegs.ecx
		mov  edx, g_GuestRegs.edx
		mov  ebx, g_GuestRegs.ebx
		mov  esp, g_GuestRegs.esp
		mov  ebp, g_GuestRegs.ebp
		mov  esi, g_GuestRegs.esi
		mov  edi, g_GuestRegs.edi

		// vmresume -- Guest 继续执行 ， 这里硬编码而不是call vmx_vmresume()的好处是可以避免栈的变化
		__emit 0x0f
		__emit 0x01
		__emit 0xc3
	}

}
