//=============================================================================
/*
* modules:
 - GPIO A (AHB): pin 2 ("USART_2_TX"), pin 3 ("USART_2_RX")
 - USART 2 (APB 1)
* notes:
 - USART 2 connect to ST-LINK/V2-1 for virtual COM port interface on USB
 - look for "<RCC>" for code depend on system clock frequence value 
*/
//=============================================================================
#include "main.h"
//=============================================================================
static uint32_t prb;
static uint32_t tdr;
//=============================================================================
void uart2_gpio_init(void)
{
	#define USART_ALT_FUNC 7U
	
	// For delay
	int32_t i;
	
  // 1. Enable clock for GPIO A + delay
	RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
	for (i = 0; i < 15; ++i);
	
  // 2. Alternative function 7 (USART) for pin 2 and 3
	GPIOA->MODER |= GPIO_MODER_MODER2_1 | GPIO_MODER_MODER3_1;
	GPIOA->AFR[0] |= USART_ALT_FUNC << GPIO_AFRL_AFRL2_Pos |
			USART_ALT_FUNC << GPIO_AFRL_AFRL3_Pos;
	for (i = 0; i < 15; ++i);
	
	#undef USART_ALT_FUNC
}
//-----------------------------------------------------------------------------
void usart2_init(void)
{
	// For delay
	int32_t i;
	
	// Save reset state from transmit data register
	prb = USART2->TDR & 0xFFFFFF00U;
	
	// Enable alternative function for USART 2
	uart2_gpio_init();
	
	// Enable clock for USART 2 + delay
	RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
	for (i = 0; i < 15; ++i);
	
	// <RCC>
	  // 9600 baud for 8 MHz on AHB 1
	  // 19200 baud for 16 MHz on AHB 1
	USART2->BRR |= 0x0341U;  
	
	// Enable USART 2
	USART2->CR1 |= USART_CR1_UE;
	for (i = 0; i < 15; ++i);
	
	// Transmitter enable
	USART2->CR1 |= USART_CR1_TE;
	for (i = 0; i < 15; ++i);
}
//-----------------------------------------------------------------------------
void 
debug_init(void)
{
	#ifdef DEBUG
	usart2_init();
	#endif
}
//=============================================================================
void 
debug_send(uint8_t c)
{
	#ifdef DEBUG
	// Bits OR data and reset bits
	tdr = prb | c;
	// Wait end of previous transmit
	while (!(USART2->ISR & USART_ISR_TXE));
	// Set data to transmit
	USART2->TDR = tdr;
	#endif
}
//=============================================================================
