//=============================================================================
/*
* modules:
 - RCC
*/
//=============================================================================
#include "main.h"
//=============================================================================
void 
clock_change(void)
{
	// For delay
	uint32_t i;
	
  // HSE
	// Without bypassed
	RCC->CR &= ~RCC_CR_HSEBYP;
	// Delay due to conveyor
	for (i = 0; i < 2; ++i);
	// Enable HSE + confirm
	RCC->CR |= RCC_CR_HSEON;
	while (!(RCC->CR & RCC_CR_HSERDY));
	
  // PLL
	// Disable PLL + confirm
	RCC->CR &= ~RCC_CR_PLLON;
	while (RCC->CR & RCC_CR_PLLRDY);
	// Choose HSE/PREDIV as PLL input + delay due to 
	// time access (see Reference Manual -> 9.4.2. RCC_CFGR -> Access)
	RCC->CFGR |= RCC_CFGR_PLLSRC;
	for (i = 0; i < 6; ++i); 
	// Enable PLL + confirm
	RCC->CR |= RCC_CR_PLLON;
	while (!(RCC->CR & RCC_CR_PLLRDY));
	
  // CSS
	// Enable Clock Security System (CSS)
	RCC->CR |= RCC_CR_CSSON;
	
  // Change system clock source and prescalers
	// Use PLL as system clock + delay (time access) + confirm
	RCC->CFGR |= RCC_CFGR_SW_1;
	for (i = 0; i < 6; ++i); 
	while (
		!(RCC->CFGR & RCC_CFGR_SWS_1) // &&
		// (RCC->CFGR & RCC_CFGR_SWS_0)
	);
	// Set prescalers
	// CFGR register -> PPRE2 (APB 2), PPRE1 (APB 1), HPRE (AHB)
	// CFGR2 register
	// CFGR3 register
	
  // HSI
	// Disable HSI + confirm
	RCC->CR &= ~RCC_CR_HSION;
	while (RCC->CR & RCC_CR_HSIRDY);
}
//=============================================================================
// CSS handler
void 
NMI_Handler(void)
{
	// Clear CIR->CSSF flag
	RCC->CIR |= RCC_CIR_CSSC;
}
//=============================================================================
