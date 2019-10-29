#include "stdafx.h"
#include "vtasm.h"
#include "exithandler.h"

VMX_CPU g_VMXCPU;
ULONG ExitEip;

BOOLEAN IsVTEnabled()
{
	ULONG uRet_EAX, uRet_RCX,uRet_EDX,uRet_EBX;
	_CPUID_ECX uCPUID;
	_CR0  uCr0;
	_CR4  uCr4;
	IA32_FEATURE_CONTROL_MSR msr;

	// 1. check CPUID .[5] VMXON  is enabled?
	Asm_CPUID(1,&uRet_EAX,&uRet_EBX,&uRet_RCX,&uRet_EDX); // eax-->1 ; cpuid; check retRegValues;
	*((PULONG)&uCPUID) = uRet_RCX;
	if(uCPUID.VMX !=1)
	{
		Log("ERROR:当前 CPU 不支持VT",0);
		return FALSE;
	}
	
	// 2. check MSR 3ah  
	*((PULONG)&msr) = (ULONG) Asm_ReadMsr(MSR_IA32_FEATURE_CONTROL);// 0x3ah
	if(msr.Lock!=1)
	{
		Log("ERROR:VT 指令未被锁定",0);
		return FALSE;
	}

	// 3. check CR0\CR4
	*((PULONG)&uCr0) = Asm_GetCr0();
	*((PULONG)&uCr4) = Asm_GetCr4();

	if(uCr0.PE!=1 || uCr0.PG!=1 || uCr0.NE!=1)
	{
		Log("ERROR:这个CPU 所处的环境不是页保护模式",0);
		
		return FALSE;
	}
	if(uCr4.VMXE ==1)
	{
		Log("ERROR:这个CPU 已经开启了VT，可能有别的驱动占用；请检查关闭再试！",0);
		return FALSE;
	
	}
	// At here, means the machine environment is prepared and no other driver use already!
	else
	{
		// maybe ,we can add the ON cr4.[VMXE] code here;
		// but ,i set the code after this function called; 
		// its  same as that;
	}
	Log("Checked, the Env is Prepared!",0);
	return TRUE;
}
static ULONG VMxAdjustControls(ULONG Ctl,ULONG Msr)
{
	LARGE_INTEGER MsrValue;
	MsrValue.QuadPart = Asm_ReadMsr(Msr);
	// highpart 1 means which can be 1;   63:32h   allow-1 -- could be 1 or 0; others  must be zero;
	// and
	Ctl &= MsrValue.HighPart; // the bigger range // can be
	// lowpart 1 means which must be 1;  31:0h	allow-0 -- could be 0 or 1;others must be 1;
	// or
	Ctl |= MsrValue.LowPart;  // the smaller range // must be 
	return Ctl;
}
// 客户机 VM 执行的咧程
extern ULONG g_RetDriver_Esp;
extern ULONG g_RetDriver_Eip;// DriverEntry中的保存的VT 返回位置 esp、eip

// MUST BE NAKED!!!
void _declspec(naked) GuestEntry(void)
{
	__asm{
		mov ax, es
		mov es, ax

		mov ax, ds
		mov ds, ax

		mov ax, fs
		mov fs, ax

		mov ax, gs
		mov gs, ax

		mov ax, ss
		mov ss, ax
	}
	// 直接将 VT_Guest 执行流给DriverEntry继续执行；★？
	// 这个时候, 整个系统都被 VT 了？？ 
	__asm{
		mov esp,g_RetDriver_Esp;
		jmp g_RetDriver_Eip;
	}

		
}

void SetupVMCS()
{
	ULONG GdtBase,IdtBase;
	GdtBase = Asm_GetGdtBase();
	IdtBase = Asm_GetIdtBase();
	// VMXwrite() ; like sdk GUI programming - register a WNDCLASS;fill the items needed;
	//   --- supply this Interface for follows versions compality;Not Opreate Memory Indirect; in case The struct changes, eg.GDT;

	//  RUNS BLANK;; 0x7 ERROR CODE THROWS;---★
	// so   -- LOOK FOR ITS REASON AND DEAL: volume 3 .chapter 30.4 < VM instruction error numbers >
	//      --  FOUNDED :  VM entry invalid control field(s)  --  Control Structure  -- VMCS -- < volume 3 . chapter 24 >
	// then  deal with that! there are fields as follows:
	// 1. Guest state fields
	Vmx_VmWrite(GUEST_CR0, Asm_GetCr0());
	Vmx_VmWrite(GUEST_CR3, Asm_GetCr3());
	Vmx_VmWrite(GUEST_CR4, Asm_GetCr4());

	Vmx_VmWrite(GUEST_DR7, 0x400);
	Vmx_VmWrite(GUEST_RFLAGS, Asm_GetEflags() & ~0x200);//cli

	Vmx_VmWrite(GUEST_ES_SELECTOR, Asm_GetEs() & 0xFFF8);
	Vmx_VmWrite(GUEST_CS_SELECTOR, Asm_GetCs() & 0xFFF8);
	Vmx_VmWrite(GUEST_DS_SELECTOR, Asm_GetDs() & 0xFFF8);
	Vmx_VmWrite(GUEST_FS_SELECTOR, Asm_GetFs() & 0xFFF8);
	Vmx_VmWrite(GUEST_GS_SELECTOR, Asm_GetGs() & 0xFFF8);
	Vmx_VmWrite(GUEST_SS_SELECTOR, Asm_GetSs() & 0xFFF8);
	Vmx_VmWrite(GUEST_TR_SELECTOR, Asm_GetTr() & 0xFFF8);

	Vmx_VmWrite(GUEST_ES_AR_BYTES,      0x10000);// 设置成不可用；然后进入GuestEntry 刷新 
	Vmx_VmWrite(GUEST_FS_AR_BYTES,      0x10000);
	Vmx_VmWrite(GUEST_DS_AR_BYTES,      0x10000);
	Vmx_VmWrite(GUEST_SS_AR_BYTES,      0x10000);
	Vmx_VmWrite(GUEST_GS_AR_BYTES,      0x10000);
	Vmx_VmWrite(GUEST_LDTR_AR_BYTES,    0x10000);

	Vmx_VmWrite(GUEST_CS_AR_BYTES,  0xc09b);// CS 和 TR 不能 像 前面一样设置成不可用，然后进入GuestEntry 刷新；因为GuestEntry以来CS和TR
	Vmx_VmWrite(GUEST_CS_BASE,      0);		//  所以需要手动设置。
	Vmx_VmWrite(GUEST_CS_LIMIT,     0xffffffff);

	Vmx_VmWrite(GUEST_TR_AR_BYTES,  0x008b);
	Vmx_VmWrite(GUEST_TR_BASE,      0x80042000);
	Vmx_VmWrite(GUEST_TR_LIMIT,     0x20ab);


	Vmx_VmWrite(GUEST_GDTR_BASE,    GdtBase);
	Vmx_VmWrite(GUEST_GDTR_LIMIT,   Asm_GetGdtLimit());
	Vmx_VmWrite(GUEST_IDTR_BASE,    IdtBase);
	Vmx_VmWrite(GUEST_IDTR_LIMIT,   Asm_GetIdtLimit());

	Vmx_VmWrite(GUEST_IA32_DEBUGCTL,        Asm_ReadMsr(MSR_IA32_DEBUGCTL)&0xFFFFFFFF);
	Vmx_VmWrite(GUEST_IA32_DEBUGCTL_HIGH,   Asm_ReadMsr(MSR_IA32_DEBUGCTL)>>32);

	Vmx_VmWrite(GUEST_SYSENTER_CS,          Asm_ReadMsr(MSR_IA32_SYSENTER_CS)&0xFFFFFFFF);
	Vmx_VmWrite(GUEST_SYSENTER_ESP,         Asm_ReadMsr(MSR_IA32_SYSENTER_ESP)&0xFFFFFFFF);
	Vmx_VmWrite(GUEST_SYSENTER_EIP,         Asm_ReadMsr(MSR_IA32_SYSENTER_EIP)&0xFFFFFFFF); // KiFastCallEntry

	Vmx_VmWrite(GUEST_RSP,  ((ULONG)g_VMXCPU.pStack) + 0x1000);     //Guest 临时栈
	Vmx_VmWrite(GUEST_RIP,  (ULONG)GuestEntry);                     // 客户机的入口点

	Vmx_VmWrite(VMCS_LINK_POINTER, 0xffffffff);// referrence volume 3. 24.10
	Vmx_VmWrite(VMCS_LINK_POINTER_HIGH, 0xffffffff);

	// 2. Host state fields
	Vmx_VmWrite(HOST_CR0, Asm_GetCr0());
	Vmx_VmWrite(HOST_CR3, Asm_GetCr3());
	Vmx_VmWrite(HOST_CR4, Asm_GetCr4());

	Vmx_VmWrite(HOST_ES_SELECTOR, Asm_GetEs() & 0xFFF8);// the reason for reset the RPL and TI ,See: volume 3 26.2.3
	Vmx_VmWrite(HOST_CS_SELECTOR, Asm_GetCs() & 0xFFF8);
	Vmx_VmWrite(HOST_DS_SELECTOR, Asm_GetDs() & 0xFFF8);
	Vmx_VmWrite(HOST_FS_SELECTOR, Asm_GetFs() & 0xFFF8);
	Vmx_VmWrite(HOST_GS_SELECTOR, Asm_GetGs() & 0xFFF8);
	Vmx_VmWrite(HOST_SS_SELECTOR, Asm_GetSs() & 0xFFF8);
	Vmx_VmWrite(HOST_TR_SELECTOR, Asm_GetTr() & 0xFFF8);

	Vmx_VmWrite(HOST_TR_BASE, 0x80042000);//TR

	Vmx_VmWrite(HOST_GDTR_BASE, GdtBase);
	Vmx_VmWrite(HOST_IDTR_BASE, IdtBase);

	Vmx_VmWrite(HOST_IA32_SYSENTER_CS,  Asm_ReadMsr(MSR_IA32_SYSENTER_CS)&0xFFFFFFFF);
	Vmx_VmWrite(HOST_IA32_SYSENTER_ESP, Asm_ReadMsr(MSR_IA32_SYSENTER_ESP)&0xFFFFFFFF);
	Vmx_VmWrite(HOST_IA32_SYSENTER_EIP, Asm_ReadMsr(MSR_IA32_SYSENTER_EIP)&0xFFFFFFFF); // KiFastCallEntry

	Vmx_VmWrite(HOST_RSP,   ((ULONG)g_VMXCPU.pStack) + 0x2000);     //Host 临时栈
	Vmx_VmWrite(HOST_RIP,   (ULONG)VMMEntryPoint); //这里定义我们的VMM处理程序入口


	// 3. VM control fields
	// ---- 3.1 VM execution control: 
	// ---------1. Pin-Based VM-Execution Control (is Hardware Interruption)
	//				: Describes whether hook the the interruption and deals by itself IDT callbacks or pass it to host Machine 
	// ---------2. Processor-based VM-Execution Control(synchronous events,specific instructions)
	//				: Describes whether JMP-OUT to VMM when execute IO Instructions, store/load cr3 etc...
	//				Processor-based VM-E constitute TWO 32-bit vectors: 
	//				---+----primary processor-based VM-execution controls
	//				---+----secondary processor-based VM-execution controls
	//			* 1Step : set defult1 class bits;  ((DWORD)(BASE_msr_481h>>32) &0) | (DWORD)(BASE_msr_481h&0x00000000ffffffff)
	//			 PS: for pinBASE_msr_481h values include some default class1 --MUST BE 1 bits 1,2,4  == 2 + 4+16 = 0x16

	Vmx_VmWrite(PIN_BASED_VM_EXEC_CONTROL,VMxAdjustControls(0,MSR_IA32_VMX_PINBASED_CTLS));
	
	//			* 2Step : set Msr482 default 1 class...
	//           Details  see  chapter 24.6.2 and Appendix A.3.2;
	//			 PS: for processorBASE_msr_482h ,there are many default class1 --MUST BE 1 bits .... == .. = 0x401e172
	Vmx_VmWrite(CPU_BASED_VM_EXEC_CONTROL,VMxAdjustControls(0,MSR_IA32_VMX_PROCBASED_CTLS));


	// -- 3.2 3.3 is no as important as 3.1; 3.1 control fields  handles much
	//		They are some default response,when action triggered 
	// ---- 3.2 VM exit control.. References the Intel Guide books-- Volume 3 chapter 24.7
	Vmx_VmWrite(VM_EXIT_CONTROLS,VMxAdjustControls(0, MSR_IA32_VMX_EXIT_CTLS));

	// ---- 3.3 VM entry control.. References the Intel Guide books-- Volume 3 chapter 24.8
	 Vmx_VmWrite(VM_ENTRY_CONTROLS, VMxAdjustControls(0, MSR_IA32_VMX_ENTRY_CTLS));

}

NTSTATUS StartVirtualTechnology()
{
	_CR4 uCr4;
	_EFLAGS uEflags;
	// 1 check is supported ?IsEnable? And allocateMemory of VMXONRegion for VMM ,then VMXON() ;
	// 1.1 check  Is Machine supported ? already on  by other app?
	if(!IsVTEnabled())
	{
		return STATUS_UNSUCCESSFUL;
	}
	// 1.2 on cr4.[VMXE]  -- lock bit on
	*((PULONG)&uCr4) = Asm_GetCr4();//get cr4；
	uCr4.VMXE =1;// VMXE =1 enable
	Asm_SetCr4(*(PULONG)&uCr4); // set cr4 value..
	// 1.3 Allocate VMXONRegion. And do Prepare in needed
	g_VMXCPU.pVMXONRegion = ExAllocatePoolWithTag(NonPagedPool,0x1000,'vmx');// para@3 is digit value
	RtlZeroMemory(g_VMXCPU.pVMXONRegion,0x1000);// initial memory
	*(PULONG)g_VMXCPU.pVMXONRegion =1;//set revision; -- the first 32bits of VMXONRegion; clear the no.32bit
	g_VMXCPU.pVMXONRegion_PA = MmGetPhysicalAddress(g_VMXCPU.pVMXONRegion);// get physical address

	// 1.4 VMXON
	Vmx_VmxOn(g_VMXCPU.pVMXONRegion_PA.LowPart,g_VMXCPU.pVMXONRegion_PA.HighPart);
	*((PULONG)&uEflags) = Asm_GetEflags();
	
	if(uEflags.CF!=0)// the flag to identify state of  success or not
	{
		Log("ERROR:VMXON指令调用失败!",0);
		ExFreePool(g_VMXCPU.pVMXONRegion);
		return STATUS_UNSUCCESSFUL;
	}
	__asm int 3
	//  2. Create VMCS for one VM ,and prepared it;
	g_VMXCPU.pVMCSRegion = ExAllocatePoolWithTag(NonPagedPool,0x1000,'vmcs');
	RtlZeroMemory(g_VMXCPU.pVMCSRegion,0x1000);
	*(PULONG)g_VMXCPU.pVMCSRegion = 1;
	//  2.1 Clear  VMCS -- 
	g_VMXCPU.pVMCSRegion_PA = MmGetPhysicalAddress(g_VMXCPU.pVMCSRegion);// get physical Address
	Vmx_VmClear(g_VMXCPU.pVMCSRegion_PA.LowPart,g_VMXCPU.pVMCSRegion_PA.HighPart);
	//  2.2 choose VM  -- Vmptrld()--：choose which VM to RUN
	Vmx_VmPtrld(g_VMXCPU.pVMCSRegion_PA.LowPart,g_VMXCPU.pVMCSRegion_PA.HighPart);

	
	// setting for VMCSRegion
	//  2.3 init some  bits of control Structure;
	//  so many instructions  needed, so  put it all in a function

	// allocate the the Stack
	g_VMXCPU.pStack = ExAllocatePoolWithTag(NonPagedPool,0x2000,'Stak');
	RtlZeroMemory(g_VMXCPU.pStack,0x2000);

	// -- init VMCS 
	SetupVMCS();
	//-- VM  Launch（） -------**;
	g_VMXCPU.bVTStartSuccess = TRUE;
	Vmx_VmLaunch();
	//-------------- if VM runs right , here is never execute!
	g_VMXCPU.bVTStartSuccess = FALSE;
	ULONG X = Vmx_VmRead(VM_INSTRUCTION_ERROR);
	DbgPrint("ERROR:%x",X);
	__asm int 3;
	Log("ERROR:VmLaunch指令调用失败!!!!", Vmx_VmRead(VM_INSTRUCTION_ERROR));
	
	StopVirtualTechnology();


	return STATUS_SUCCESS;
}
ULONG g_VmCall_Arg;
ULONG g_Stop_Esp,g_Stop_Eip;

NTSTATUS StopVirtualTechnology()
{
	_CR4 uCr4;
	// ** VMXOff;
	if(g_VMXCPU.bVTStartSuccess)
	{
		g_VmCall_Arg = 'SVT';
		__asm
		{
			pushad;
			pushfd;
			mov g_Stop_Esp,esp;
			mov g_Stop_Eip,offset StopVt;
		}

	}
	Vmx_VmCall();// to query VmOff() from Handler System
				// when it handled ,it ret to
StopVt:
	__asm{
		popfd;
		popad;
	}

	g_VMXCPU.bVTStartSuccess = FALSE;

	// ** off the cr4 bit
	*((PULONG)&uCr4) = Asm_GetCr4();// get cr4；
	uCr4.VMXE =0;// VMXE =0 disable
	Asm_SetCr4(*(PULONG)&uCr4); // set;


	// * free page; in kernel nonpage  its not freed automatic;
	// free VMXONRegion;
	ExFreePool(g_VMXCPU.pVMXONRegion);
	// free VMCSRegion
	ExFreePool(g_VMXCPU.pVMCSRegion);
	// free VMCSRegion.pStack
	ExFreePool(g_VMXCPU.pStack);
	Log("[MinVT]:正常退出VT。。",0);
	
	return  STATUS_SUCCESS;
}