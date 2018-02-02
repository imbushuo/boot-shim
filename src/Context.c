#include "application.h"

#ifdef _ARM
#include <armintr.h>

// Switch to "real" mode and never look back.
// And we do not need boot application context in this application.
void SwitchToRealModeContext(int ptr3, int ptr7)
{

	// Disable IRQ
	_disable();

	int ptr9 = *(DWORD *)(ptr3 + 44) | *(DWORD *)(ptr3 + 40);
	_MoveToCoprocessor(ptr9, CONTROL_COPROCESSPOR_CP15, 0, 2, 0, 0);
	__isb(_ARM_BARRIER_SY);
	_MoveToCoprocessor(0, CONTROL_COPROCESSPOR_CP15, 0, 8, 7, 0);
	_MoveToCoprocessor(0, CONTROL_COPROCESSPOR_CP15, 0, 7, 5, 6);
	__dsb(_ARM_BARRIER_SY);
	__isb(_ARM_BARRIER_SY);
	// We probably should see the "developermenu.efi" in depth to decide whether
	// enable the following code on or remove it.
#if MICROSOFT_EXPERIMENTAL
	_MoveToCoprocessor(ptr9, CONTROL_COPROCESSPOR_CP15, 0, 7, 8, 0);
	__isb(_ARM_BARRIER_SY);
	_MoveFromCoprocessor(CONTROL_COPROCESSPOR_CP15, 0, 7, 4, 0);
#endif
	_MoveToCoprocessor(*(DWORD *)(ptr3 + 32), CONTROL_COPROCESSPOR_CP15, 0, 13, 0, 4);
	__dsb(_ARM_BARRIER_SY);
	_MoveToCoprocessor(*(DWORD *)(ptr3 + 16), CONTROL_COPROCESSPOR_CP15, 0, 1, 0, 0);
	_MoveToCoprocessor(0, CONTROL_COPROCESSPOR_CP15, 0, 7, 5, 6);
	__dsb(_ARM_BARRIER_SY);
	__isb(_ARM_BARRIER_SY);
	_MoveToCoprocessor(*(DWORD *)(ptr3 + 20), CONTROL_COPROCESSPOR_CP15, 0, 12, 0, 0);
	__isb(_ARM_BARRIER_SY);

	// Restore IRQ if necessary
	if (ptr7) _enable();

}

#endif