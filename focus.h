//=============================================================================
#ifndef FOCUS_H
#define FOCUS_H
//=============================================================================
#include <stm32f302x8.h>
//-----------------------------------------------------------------------------
#define FOCUS_STATE_OK        0x00U
#define FOCUS_STATE_NOSTART   0x04U
#define FOCUS_STATE_ERR       0x08U
//-----------------------------------------------------------------------------
#define FOCUS_DEVIDER    100U
#define FOCUS_MASK       0x00000FFFU
//-----------------------------------------------------------------------------
void focus_init(void);
void focus_start(void);
void focus_keysEn(void);
void focus_keysDis(void);
void focus_keysForward(void);
void focus_keysBack(void);
void focus_keysStop(void);
uint32_t focus_getState(void);
//=============================================================================
#endif // FOCUS_H
//=============================================================================
