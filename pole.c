//=============================================================================
/*
* modules:
 - GPIO B (AHB): pin 4 ("MC1_P"), pin 5 ("MC1_N")
 - GPIO C (AHB): pin 14 ("EN_1")
 - GPIO A (AHB): pin 2 ("MC2_P"), pin 3 ("MC2_N")
 - GPIO B (AHB): pin 10 ("EN_2")
 - TIM 6 (APB 1)
* notes:
 - TIM 6 prescaler from RCC: Figure 14. STM32F302x6/8 clock tree
 - look for "<RCC>" for code depend on system clock frequence value
 - GPIO B, pin 4 - using after reset => clear related bits in GPIO B
*/
//=============================================================================
#include "main.h"
#include "pole.h"
//=============================================================================
static volatile uint32_t pole_state;
static uint32_t pole_current;
//=============================================================================
static void 
keys_init(void)
{
	// For delay
	int32_t i;
	
	// Enable clock for GPIO A, B, C + delay
	RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN | 
		RCC_AHBENR_GPIOCEN;
	for (i = 0; i < 15; ++i);
	
	// Using pins: PA 2, PA 3, PB 4, PB 5, PB 10, PC 14
	//                         ^^^^-- DANGER
	
	// Output mode
	  // Clear PB4 bits
	GPIOB->MODER &= ~GPIO_MODER_MODER4_Msk;
	
	GPIOA->MODER |= GPIO_MODER_MODER2_0 | GPIO_MODER_MODER3_0;
	GPIOB->MODER |= GPIO_MODER_MODER4_0 | GPIO_MODER_MODER5_0 | 
		GPIO_MODER_MODER10_0;
	GPIOC->MODER |= GPIO_MODER_MODER14_0; 
	
	// High speed
	  // Clear PB4 bits
	GPIOB->OSPEEDR &= ~GPIO_OSPEEDER_OSPEEDR4_Msk;
	
	GPIOA->OSPEEDR |= 
		GPIO_OSPEEDER_OSPEEDR2_0 | GPIO_OSPEEDER_OSPEEDR2_1 |
		GPIO_OSPEEDER_OSPEEDR3_0 | GPIO_OSPEEDER_OSPEEDR3_1;
	GPIOB->OSPEEDR |= 
		GPIO_OSPEEDER_OSPEEDR4_0 | GPIO_OSPEEDER_OSPEEDR4_1 |
		GPIO_OSPEEDER_OSPEEDR5_0 | GPIO_OSPEEDER_OSPEEDR5_1 |
		GPIO_OSPEEDER_OSPEEDR10_0 | GPIO_OSPEEDER_OSPEEDR10_1;
	GPIOC->OSPEEDR |= 
		GPIO_OSPEEDER_OSPEEDR14_0 | GPIO_OSPEEDER_OSPEEDR14_1;
	
	// Pull-down
	  // Clear PB4 bits
	GPIOB->PUPDR &= ~GPIO_PUPDR_PUPDR4_Msk;
	
	GPIOA->PUPDR |= GPIO_PUPDR_PUPDR2_1 | GPIO_PUPDR_PUPDR3_1;
	GPIOB->PUPDR |= GPIO_PUPDR_PUPDR4_1 | GPIO_PUPDR_PUPDR5_1 | 
		GPIO_PUPDR_PUPDR10_1;
	GPIOC->PUPDR |= GPIO_PUPDR_PUPDR14_1;
}
//-----------------------------------------------------------------------------
void
tim6_init(void)
{
	// For delay
	int32_t i;
	
  // Enable clock for TIM 6 + delay
	RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;
	for (i = 0; i < 15; ++i);
	
	// <RCC>
  // Set prescaler
	TIM6->PSC = 16000U - 1U;  // 16 MHz -> 1 KHz;
	// ^^^^^^^^^^^^^^^^-- preloaded => need UEV
	
  // Set auto-reload value
	TIM6->ARR = 1000U;  // 1 KHz -> 1 Hz;
	// ^^^^^^^^^^^^^^^^-- ARPE = 0 => not preloaded
	
  // One-pulse mode
	TIM6->CR1 |= TIM_CR1_OPM;
	
  // Generate an update event (for prescaler update value)
	TIM6->EGR |= TIM_EGR_UG;
	// Wait re-initializes the timer
	while (TIM6->EGR & TIM_EGR_UG);
	// Clear interrupt flag
	TIM6->SR &= ~TIM_SR_UIF;
	for (i = 0; i < 4; ++i);
	
  // Enable UEV interrupt
	TIM6->DIER |= TIM_DIER_UIE;
	
  // Enable interrupt from TIM 6
	NVIC_EnableIRQ(TIM6_DAC_IRQn);
}
//-----------------------------------------------------------------------------
void 
pole_init(void)
{
	pole_state = POLE_STATE_NOSTART;
	
	keys_init();
	tim6_init();
}
//=============================================================================
void 
pole_keysEn(void)
{
	// Enable EN_1
	GPIOC->BSRR |= GPIO_BSRR_BS_14;
	// Enable EN_2
	GPIOB->BSRR |= GPIO_BSRR_BS_10;
}
//-----------------------------------------------------------------------------
void 
pole_keysDis(void)
{
	// Disable EN_1
	GPIOC->BSRR |= GPIO_BSRR_BR_14;
	// Disable EN_2
	GPIOB->BSRR |= GPIO_BSRR_BR_10;
}
//-----------------------------------------------------------------------------
void 
pole_keysSetDir(int32_t m_1, int32_t m_2)
{
	if (m_1 == 0)
		// Reset MC1_P, reset MC1_N
		GPIOB->BSRR |= GPIO_BSRR_BR_4 | GPIO_BSRR_BR_5;
	if (m_2 == 0)
		// Reset MC2_P, reset MC2_N
		GPIOA->BSRR |= GPIO_BSRR_BR_2 | GPIO_BSRR_BR_3;
	
	if (m_1 == -1)
		// Set MC1_P, reset MC1_N
		GPIOB->BSRR |= GPIO_BSRR_BS_4 | GPIO_BSRR_BR_5;
	if (m_2 == 1)
		// Set MC2_P, reset MC2_N
		GPIOA->BSRR |= GPIO_BSRR_BS_2 | GPIO_BSRR_BR_3;
	
	if (m_1 == 1)
		// Set MC1_P, reset MC1_N
		GPIOB->BSRR |= GPIO_BSRR_BR_4 | GPIO_BSRR_BS_5;
	if (m_2 == -1)
		// Set MC2_P, reset MC2_N
		GPIOA->BSRR |= GPIO_BSRR_BR_2 | GPIO_BSRR_BS_3;
}
//=============================================================================
void 
pole_start(void)
{
	// Set target pole == current pole
	pole_target = POLE_0;
	// Set current pole == target pole
	pole_current = POLE_0;
	// Enable keys
	pole_keysEn();
	// Reset all poles
	pole_keysSetDir(-1, -1);
	// Run TIM 6
	TIM6->CR1 |= TIM_CR1_CEN;
}
//=============================================================================
void 
pole_setPole(uint32_t pole)
{
	if (pole_current == pole)
		return;
	
	// Disable TIM 6 (if was be run)
	TIM6->CR1 &= ~TIM_CR1_CEN;
	// Clear counter regiset
	TIM6->CNT &= 0xFFFF0000U;
	
	if (pole_current == POLE_1 && pole == POLE_2) {
		// 1
		pole_keysSetDir(-1, 1);
	} else if (pole_current == POLE_2 && pole == POLE_1) {
		// 2
		pole_keysSetDir(1, -1);
	} else if (pole_current == POLE_1 && pole == POLE_0) {
		// 3
		pole_keysSetDir(-1, 0);
	} else if (pole_current == POLE_2 && pole == POLE_0) {
		// 4
		pole_keysSetDir(0, -1);
	} else if (pole_current == POLE_0 && pole == POLE_1) {
		// 5
		pole_keysSetDir(1, 0);
	} else if (pole_current == POLE_0 && pole == POLE_2) {
		// 6
		pole_keysSetDir(0, 1);
	}
	
	// Set current pole as pole get from main loop
	pole_current = pole;
	
	// Run TIM 6
	TIM6->CR1 |= TIM_CR1_CEN;
}
//=============================================================================
uint32_t 
pole_getState(void)
{
	return pole_state;
}
//=============================================================================
void 
TIM6_DAC_IRQHandler(void)
{
	// Stop all pole motors
	pole_keysSetDir(0, 0);
	
	// Exclude the cause of the interrupt
	TIM6->SR &= ~TIM_SR_UIF;
	
	// Clear NOSTART flag (for main loop)
	pole_state &= ~POLE_STATE_NOSTART;
}
//=============================================================================
