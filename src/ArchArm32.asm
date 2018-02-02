; PBL_ARCH_CONTEXT ArchSwitchContext(_In_ PBL_ARCH_CONTEXT NewContext, _In_ PBL_ARCH_CONTEXT OldContext);
; Switch context on ARM32 platform.
ArchSwitchContext
	push        {r11,lr}
	mov         r11,sp
	cbz         r1,ArchSwitchContext_CTX_SWTICH
	mrc         p15,#0,r3,c1,c0
	str         r3,[r1,#8]
	mrc         p15,#0,r3,c12,c0
	str         r3,[r1,#0xC]
	mrc         p15,#0,r3,c13,c0,#4
	str         r3,[r1,#0x18]

ArchSwitchContext_CTX_SWTICH 
	cpsid       i
	mcr         p15,#0,r0,c7,c8
	isb
	mrc         p15,#0,r3,c7,c4
	ldr         r2,[r0,#0x24]
	ldr         r3,[r0,#0x20]
	orrs        r3,r3,r2
	mcr         p15,#0,r3,c2,c0
	isb
	movs        r3,#0
	mcr         p15,#0,r3,c8,c7
	mcr         p15,#0,r3,c7,c5,#6
	dsb
	isb
	mcr         p15,#0,r0,c7,c8
	isb
	mrc         p15,#0,r3,c7,c4
	ldr         r3,[r0,#0x18]
	mcr         p15,#0,r3,c13,c0,#4
	dsb
	ldr         r3,[r0,#8]
	mcr         p15,#0,r3,c1,c0
	movs        r3,#0
	mcr         p15,#0,r3,c7,c5,#6
	dsb
	isb
	ldr         r3,[r0,#0xC]
	mcr         p15,#0,r3,c12,c0
	isb
	ldr         r3,[r0,#4]
	tst         r3,#2
	beq         ArchSwitchContext_CTX_RETURN
	cpsie       i

ArchSwitchContext_CTX_RETURN
	pop         {r11,pc}
	end
