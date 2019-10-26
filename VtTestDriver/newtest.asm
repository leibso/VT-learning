; Test34_8.asm
.386
.model flat, stdcall

Asm_xx Proto
;自定义的宏
mPrint macro Text
    PrintText '* &Text& *'
endm

.code
Asm_xx proc
	
	pushad
	popad
	ret
Asm_xx endp

END
