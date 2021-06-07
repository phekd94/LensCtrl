//=============================================================================
/*
* modules:
 - GPIO B (AHB): pin 8 ("CAN_RX"), pin 9 ("CAN_TX")
 - CAN (APB 1)
* notes:
 - look for "<RCC>" for code depend on system clock frequence value
*/
//=============================================================================
#include "main.h"
#include "can.h"
//=============================================================================
static uint32_t can_state;
//=============================================================================
// 1. Enable clock for GPIO B
// 2. Alternative function 9 (CAN) for pin 8 and 9
void 
can_gpio_init(void)
{
	#define CAN_ALT_FUNC 9U
	
	// For delay
	int32_t i;
	
  // 1. Enable clock for GPIO B + delay
	RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
	for (i = 0; i < 15; ++i);
	
  // 2. Alternative function 9 (CAN) for pin 8 and 9
	GPIOB->MODER |= GPIO_MODER_MODER8_1 | GPIO_MODER_MODER9_1;
	GPIOB->AFR[1] |= 
		CAN_ALT_FUNC << GPIO_AFRH_AFRH0_Pos | 
		CAN_ALT_FUNC << GPIO_AFRH_AFRH1_Pos;
	for (i = 0; i < 15; ++i);
	
	#undef CAN_ALT_FUNC
}
//-----------------------------------------------------------------------------
// 1. Enable clock for CAN
// 2. Sleep mode -> Initialization mode + confirm
// 3. Transmit priority by the request order
// 4. Non automatic retransmission mode
// 5. Enable interrupt for reception message (FIFO 0)
// 6 (disable). Enable interrupt when full (FIFO 0)
// 7. Enable interrupt for overrun (FIFO 0)
// 8 (disable). Enable Buss-Off interrupt
// 9 (disable). Enable error interrupt
// 10. Auto exit from Bus-Off state
// 11. Bit-timeing setting
// 12 (disable). Enable time triggered communication mode

// X. Enter in filter setting mode
// X+1. Identifier list mode for filter bank 0, 1
// X+2. Single 32-bit scale configuration for filter bank 0, 1
// X+3. Set id list for filter bank 0, 1

// Y. Exit from filter setting mode
// Y+1. Activate filter bank 0, 1

// Z. Enable interrupt using NVIC
void 
can_init(void)
{
	// For delay
	int32_t i;
	
	can_state = CAN_STATE_OK;
	
	// Enable alternative function for CAN
	can_gpio_init();

  // 1. Enable clock for CAN + delay	
	RCC->APB1ENR |= RCC_APB1ENR_CANEN;
	for (i = 0; i < 15; ++i);
	
  // 2. Sleep mode -> Initialization mode + confirm
	// Only for reset value in register
	CAN->MCR ^= CAN_MCR_INRQ | CAN_MCR_SLEEP;
	while (!(CAN->MSR & CAN_MSR_INAK) && CAN->MSR & CAN_MSR_SLAK);
	
  // 3. Transmit priority by the request order
  // 4. Non automatic retransmission mode
	CAN->MCR |= CAN_MCR_TXFP | CAN_MCR_NART;
	
  // 5. Enable interrupt for reception message (FIFO 0)
  // 6 (disable). Enable interrupt when full (FIFO 0)
  // 7. Enable interrupt for overrun (FIFO 0)
	CAN->IER |= CAN_IER_FMPIE0 | CAN_IER_FOVIE0; // 6: | CAN_IER_FFIE0
	
  // 8 (disable). Enable Bus-Off interrupt
  // 9 (disable). Enable error interrupt
	// CAN->IER |= CAN_IER_BOFIE | CAN_IER_ERRIE;
	
  // 10. Auto exit from Bus-Off state
	CAN->MCR |= CAN_MCR_ABOM;
	
  // 11. Bit-timing setting
	// <RCC>
	// 8 MHz system clock - 500 Kbit/s
	// 16 MHz system clock - 1000 Kbit/s
	#define TS1 11U // BS1 = 11 + 1 = 12
	#define TS2 2U  // BS2 =  2 + 1 =  3
	#define SJW 1U  // SJW =  1 + 1 =  2
	CAN->BTR &= ~CAN_BTR_SJW_Msk | ~CAN_BTR_TS2_Msk | ~CAN_BTR_TS1_Msk;
	for (i = 0; i < 2; ++i);
	CAN->BTR |= TS1 << CAN_BTR_TS1_Pos | TS2 << CAN_BTR_TS2_Pos | 
		SJW << CAN_BTR_SJW_Pos;
	#undef TS1
	#undef TS2
	#undef SJW
	
  // 12 (disable). Enable time triggered communication mode
	// CAN->MCR |= CAN_MCR_TTCM;
	
  // X. Enter in filter setting mode
	CAN->FMR |= CAN_FMR_FINIT;
	
  // X+1. Identifier list mode for filter bank 0, 1
	CAN->FM1R |= CAN_FM1R_FBM0 | CAN_FM1R_FBM1;
	
  // X+2. Single 32-bit scale configuration for filter bank 0, 1
	CAN->FS1R |= CAN_FS1R_FSC0 | CAN_FS1R_FSC1;
	
  // X+3. Set id list for filter bank 0, 1
	CAN->sFilterRegister[0].FR1 = CAN_ID_CMD << 21;
	CAN->sFilterRegister[0].FR2 = 0; // CAN_ID_CMD << 21 | 1 << 1; // + rtr
	
	CAN->sFilterRegister[1].FR1 = CAN_ID_CTRL << 21;
	CAN->sFilterRegister[1].FR2 = 0;
	
  // Y. Exit from filter setting mode
	CAN->FMR &= ~CAN_FMR_FINIT;
	
  // Y+1. Activate filter bank 0, 1
	CAN->FA1R |= CAN_FA1R_FACT0 | CAN_FA1R_FACT1;
	
  // Z. Enable interrupt from CAN RX0 (filter bank 0, 1)
	NVIC_EnableIRQ(USB_LP_CAN_RX0_IRQn);
	
}
//=============================================================================
void 
can_start(void)
{
	// Initialization mode -> Normal mode + confirm
	CAN->MCR &= ~CAN_MCR_INRQ;
	while (CAN->MSR & CAN_MSR_INAK);
}
//=============================================================================
int32_t 
can_send(uint32_t id, uint8_t dlc, uint32_t l, uint32_t h, uint8_t thread)
{
	#define CAN_TIxR_STID_Pos  CAN_TI0R_STID_Pos
	#define CAN_TDTxR_DLC_Pos  CAN_TDT0R_DLC_Pos
	#define CAN_TDTxR_DLC_Msk  CAN_TDT0R_DLC_Msk
	#define CAN_TIxR_TXRQ      CAN_TI0R_TXRQ
	
	// For delay
	int32_t i;
	
	// Current transmit FIFO (0, 1 or 2) and coresponding control bit for 
	// each FIFO; depend on thread input parameter
	uint32_t currRQCP_Pos, currALST_Pos, currTERR_Pos;
	CAN_TxMailBox_TypeDef *currMailBox;
	
	if (thread == 0) {
		currMailBox = &CAN->sTxMailBox[0];
		currRQCP_Pos = CAN_TSR_RQCP0;
		currALST_Pos = CAN_TSR_ALST0;
		currTERR_Pos = CAN_TSR_TERR0;
	} else if (thread == 1) {
		currMailBox = &CAN->sTxMailBox[1];
		currRQCP_Pos = CAN_TSR_RQCP1;
		currALST_Pos = CAN_TSR_ALST1;
		currTERR_Pos = CAN_TSR_TERR1;
	} else if (thread == 2) {
		currMailBox = &CAN->sTxMailBox[2];
		currRQCP_Pos = CAN_TSR_RQCP2;
		currALST_Pos = CAN_TSR_ALST2;
		currTERR_Pos = CAN_TSR_TERR2;
	}
	
	// Clear old DLC
	currMailBox->TDTR &= ~CAN_TDTxR_DLC_Msk;
	// Set ID
	currMailBox->TIR = id << CAN_TIxR_STID_Pos;
	// Set data
	currMailBox->TDLR = l;
	currMailBox->TDHR = h;
	// Set DLC
	currMailBox->TDTR |= dlc << CAN_TDTxR_DLC_Pos;
	
	// Transmit request
	currMailBox->TIR |= CAN_TIxR_TXRQ;
	// Delay (TXRQ clear RQCP)
	for (i = 0; i < 2; ++i);
	// Wait end of transmit
	while (!(CAN->TSR & currRQCP_Pos)); // || !(CAN->TSR & CAN_TSR_TME0)
	
	// Check transmit status
	if (CAN->TSR & currALST_Pos || CAN->TSR & currTERR_Pos)
		// && !(CAN->TSR & CAN_TSR_TXOK0)
		return -1;
	return 0;
	
	#undef CAN_TIxR_STID_Pos
	#undef CAN_TDTxR_DLC_Pos
	#undef CAN_TDTxR_DLC_Msk
	#undef CAN_TIxR_TXRQ
}
//=============================================================================
uint32_t
can_getState(void)
{
	uint32_t err = CAN_STATE_OK;
	uint32_t ret;
	// Error if TEC or REC > 0
	if (CAN->ESR & (CAN_ESR_REC_Msk | CAN_ESR_TEC_Msk))
		err = CAN_STATE_ERR;
	ret = can_state | err;
	// Clear OVR state
	if (can_state & CAN_STATE_OVR)
		can_state &= ~CAN_STATE_OVR;
	return ret;
}
//=============================================================================
// FIFO 0
void 
USB_LP_CAN_RX0_IRQHandler(void)
{
	uint32_t fovr, l, focus_target_, pole_target_;
	uint8_t id;
	// uint8_t fmp, full, fmi, id, rtr;
	// uint32_t time, h;
	
	// fmp = (CAN->RF0R & CAN_RF0R_FMP0_Msk) >> CAN_RF0R_FMP0_Pos;
	fovr = (CAN->RF0R & CAN_RF0R_FOVR0_Msk) >> CAN_RF0R_FOVR0_Pos;
	// full = (CAN->RF0R & CAN_RF0R_FULL0_Msk) >> CAN_RF0R_FULL0_Pos;
	
	if (fovr) {
		can_state |= CAN_STATE_OVR;
		// Exclude the cause of the interrupt: clear FOVR bit
		CAN->RF0R |= CAN_RF0R_FOVR0;
	}
	
	id = (CAN->sFIFOMailBox[0].RIR & CAN_RI0R_STID_Msk) 
		>> CAN_RI0R_STID_Pos;
	// rtr = (CAN->sFIFOMailBox[0].RIR & CAN_RI0R_RTR_Msk) 
	//	>> CAN_RI0R_RTR_Pos;
	// fmi = (CAN->sFIFOMailBox[0].RDTR & CAN_RDT0R_FMI_Msk)
	//	>> CAN_RDT0R_FMI_Pos;
	// time = (CAN->sFIFOMailBox[0].RDTR & CAN_RDT0R_TIME_Msk)
	//	>> CAN_RDT0R_TIME_Pos;
	l = CAN->sFIFOMailBox[0].RDLR;
	// h = CAN->sFIFOMailBox[0].RDHR;
	
	// Exclude the cause of the interrupt: release a message in FIFO 0
	CAN->RF0R |= CAN_RF0R_RFOM0;
	
	if (id == CAN_ID_CTRL) {
		send_state();
	} else if (id == CAN_ID_CMD) {
		focus_target_ = (l & CAN_FOCUS_MSK) >> CAN_FOCUS_POS;
		if (focus_target_ <= FOCUS_MAX /*- 1 && focus_target_ >= 0 + 1*/)
			focus_target = focus_target_;
		// else
			// err focus val
		pole_target_ = (l & CAN_POLE_MSK) >> CAN_POLE_POS;
		switch (pole_target_) {
		case POLE_0:
		case POLE_1:
		case POLE_2:
			pole_target = pole_target_;
			break;
		default:
			// err pole val
			break;
		}
	}
	
	// Wait release the message from FIFO 0 (see above)
	while (CAN->RF0R & CAN_RF0R_RFOM0);
}
//=============================================================================
