//=============================================================================
#ifndef CAN_H
#define CAN_H
//=============================================================================
#include <stm32f302x8.h>
//-----------------------------------------------------------------------------
#define CAN_STATE_OK   0x00U
#define CAN_STATE_OVR  0x01U
#define CAN_STATE_ERR  0x02U
//-----------------------------------------------------------------------------
#define CAN_ID_CTRL  0x93U
#define CAN_ID_CMD   0x92U
//-----------------------------------------------------------------------------
#define CAN_POLE_POS   0U
#define CAN_FOCUS_POS  8U

#define CAN_POLE_MSK   0xFFU
#define CAN_FOCUS_MSK  0xFF00U
//-----------------------------------------------------------------------------
void can_init(void);
void can_start(void);
uint32_t can_getState(void);
int32_t can_send(uint32_t id, uint8_t dlc, uint32_t l, uint32_t h, 
		uint8_t thread);
//=============================================================================
#endif // CAN_H
//=============================================================================