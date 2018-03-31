; ARM Platform support for bootstrapping LK

CTRL_M_BIT      EQU     (1 << 0)
CTRL_C_BIT      EQU     (1 << 2)
CTRL_B_BIT      EQU     (1 << 7)
CTRL_I_BIT      EQU     (1 << 12)

	AREA s_ArmDeInitialize, CODE, READONLY
	EXPORT ArmDeInitialize

ArmDeInitialize
	; Enter critical section: disable IRQ
	cpsid   if
	isb

	; Disable Branch Prediction
	mrc     p15, 0, r0, c1, c0, 0
	bic     r0, r0, #0x00000800
	mcr     p15, 0, r0, c1, c0, 0
	dsb
	isb

	; Turn off MMU, I-Cache, D-Cache
	mrc		p15, 0, r0, c1, c0, 0           ; Get control register
	bic		r0, r0, #CTRL_M_BIT             ; Disable MMU
	bic		r0, r0, #CTRL_C_BIT             ; Disable D Cache
	bic		r0, r0, #CTRL_I_BIT             ; Disable I Cache
	mcr		p15, 0, r0, c1, c0, 0           ; Write control register
	dsb
	isb

	; Invalidates I-Cache
	mcr     p15, 0, R0, c7, c5, 0			; Invalidate entire instruction cache
	dsb
	isb

	; Flush TLB
	mov     r0, #0
	mcr     p15, 0, r0, c8, c7, 0
	mcr     p15, 0, R9, c7, c5, 6			; BPIALL Invalidate Branch predictor array. R9 == NoOp
	dsb
	isb

	; Return
	bx		lr

	END
