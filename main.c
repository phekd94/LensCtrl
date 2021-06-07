/*
 cd C:\Keil_v5\ARM\ARMCC\bin\
fromelf.exe --bin --output lensInfOphir.bin C:\Users\ekd\Documents\stm\stm32f302x8\projects\lensInfOphir\Objects\lensInfOphir.axf
http://www.keil.com/support/docs/3213.htm
*/
//=============================================================================
#include <stm32f302x8.h>
//-----------------------------------------------------------------------------
#include "main.h"
#include "focus.h"
#include "pole.h"
#include "can.h"
#include "clock.h"
//=============================================================================
volatile uint32_t *focus_pos;
volatile uint32_t *temp;
volatile uint32_t *vref;
volatile uint32_t focus_target;
volatile uint32_t pole_target;
//=============================================================================
int 
main(void)
{
	uint32_t i, focus_pos_v;
	
	clock_change();
	
	debug_init();
	
	focus_init();
	pole_init();
	can_init();
	
	focus_start();
	pole_start();
	can_start();
	
	for (;;) {
		
		if (pole_getState() == POLE_STATE_OK) {
			pole_setPole(pole_target);
		}
		
		if (focus_getState() == FOCUS_STATE_OK) {
			
			focus_pos_v = 
				(*focus_pos & FOCUS_MASK) / FOCUS_DEVIDER;
			
			if (focus_pos_v > focus_target) {
				focus_keysBack();
			} else if (focus_pos_v < focus_target) {
				focus_keysForward();
			} else { 
				focus_keysStop();
			}
		}
	}
}
//=============================================================================
void 
send_state(void)
{
	can_send(CAN_ID_CTRL, 8, 
			((*focus_pos & FOCUS_MASK) / FOCUS_DEVIDER) << 8 | 
			pole_target,
		0, 0);
}
//=============================================================================
