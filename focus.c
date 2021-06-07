//=============================================================================
/*
* modules:
 - GPIO A (AHB): pin 0 ("MC3_N"), pin 1 ("MC3_P")
 - GPIO B (AHB): pin 12 ("EN_3")
 - GPIO B (AHB): pin 13 ("VAR_RES_IR" for ADC 1)
 - ADC 1 (AHB)
 - TIM 2 (APB 1)
 - DMA 1 (AHB)
* scheme:
    GPIO B: pin 13
        \
 TIM -> ADC *-->* MEMORY 
         \   \ /
          -> DMA
* notes:
 - TIM 2 prescaler from RCC: Figure 14. STM32F302x6/8 clock tree
 - errata -> ADC -> forbidden instructions and calibration
 - look for "<RCC>" for code depend on system clock frequence value 
*/
//=============================================================================
#include "main.h"
#include "focus.h"
//=============================================================================
static __align(4) uint32_t adc_val[3];  // align(4) - 32 bit align for DMA 
                                        // (not need - compilator)
static volatile uint32_t focus_state;
//=============================================================================
static void
keys_init(void)
{
	// For delay
	int32_t i;
	
	// Enable clock for GPIO A, B + delay
	RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN;
	for (i = 0; i < 15; ++i);
	
	// Using pins: PA 0, PA 1, PB 12
	
	// Output mode
	GPIOA->MODER |= GPIO_MODER_MODER0_0 | GPIO_MODER_MODER1_0;
	GPIOB->MODER |= GPIO_MODER_MODER12_0;
	
	// High speed
	GPIOA->OSPEEDR |= 
		GPIO_OSPEEDER_OSPEEDR0_0 | GPIO_OSPEEDER_OSPEEDR0_1 |
		GPIO_OSPEEDER_OSPEEDR1_0 | GPIO_OSPEEDER_OSPEEDR1_1;
	GPIOB->OSPEEDR |= 
		GPIO_OSPEEDER_OSPEEDR12_0 | GPIO_OSPEEDER_OSPEEDR12_1;
	
	// Pull-down
	GPIOA->PUPDR |= GPIO_PUPDR_PUPDR0_1 | GPIO_PUPDR_PUPDR1_1;
	GPIOB->PUPDR |= GPIO_PUPDR_PUPDR12_1;
}
//-----------------------------------------------------------------------------
static void 
dma1_init(void)
{
	// For delay
	int32_t i;
	
	// Enable clock for DMA 1 + delay
	RCC->AHBENR |= RCC_AHBENR_DMA1EN;
	for (i = 0; i < 15; ++i);
	
	// Channel 1 for ADC 1 request
	DMA1_Channel1->CCR |= 	DMA_CCR_MSIZE_1 |  // Memory size = 32 bit
				DMA_CCR_MINC |     // Memory increment mode
				DMA_CCR_PSIZE_0 |  // Peripheral size = 16 bit 
				DMA_CCR_CIRC |     // Circular mode
				DMA_CCR_TCIE |     // Transfer complete 
				                   //         interrupt enable
				DMA_CCR_TEIE;      // Transfer error 
				                   //         interrupt enable
	DMA1_Channel1->CNDTR |= 3U;    // Number of data = 3
	DMA1_Channel1->CPAR = 
			(uint32_t)&(ADC1->DR);     // Peripheral address
	DMA1_Channel1->CMAR = (uint32_t)adc_val;   // Memory address
	NVIC_EnableIRQ(DMA1_Channel1_IRQn);        // Enable interrupt from 
	                                           // DMA Channel 1
}
//-----------------------------------------------------------------------------
static void 
tim2_init(void)
{
	// For delay
	int32_t i;
	
  // Enable clock for TIM 2 + delay
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
	for (i = 0; i < 15; ++i);
	
  // OC2REF (CC2) toggles when TIMx_CNT == TIMx_CCR2 (need for ADC 1 trigger)
	TIM2->CCMR1 |= TIM_CCMR1_OC2M_0 | TIM_CCMR1_OC2M_1;
	
  // Set value for CCR2 (must be equal to ARR for this application)
	TIM2->CCR2 = 1000U;  // 1 MHz -> 1 KHz;
	// ^^^^^^^^^^^^^^^^^^^^^-- OC2PE = 0 => not preloaded
	
	// <RCC>
  // Set prescaler
  // WARNING: + 4 - error HSI (2 ns on clock cycle on 8 MHz)
	TIM2->PSC = 16U - 1U;  // 16 MHz -> 1 MHz;
	// ^^^^^^^^^^^^^^^^-- preloaded => need UEV
	
  // Set auto-reload value (must be equal to CCR2 for this application)
	TIM2->ARR = 1000U;    // 1 MHz -> 1 KHz;
	// ^^^^^^^^^^^^^^^^^^^^-- ARPE = 0 => not preloaded
	
  // Set OC2 (CC2) signal as output (otherwise without interrupt)
	// NOTE: reference manual - figure 216
	TIM2->CCER |= TIM_CCER_CC2E;
	
	for (i = 0; i < 15; ++i);
  // Update generation - UEV
	TIM2->EGR |= TIM_EGR_UG;
	for (i = 0; i < 15; ++i);
  // Exclude the cause of interrupt or DMA request
	TIM2->SR &= ~TIM_SR_UIF;
	for (i = 0; i < 15; ++i);
}
//-----------------------------------------------------------------------------
static void 
adc1_gpio_init(void)
{
	// For delay
	int32_t i;
	
	// Enable clock for GPIO B + delay
	RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
	for (i = 0; i < 15; ++i);
	
	// PB 13: analog finction for ADC 1
	GPIOB->MODER |= GPIO_MODER_MODER13_0 | GPIO_MODER_MODER13_1;
	for (i = 0; i < 15; ++i);
}
//-----------------------------------------------------------------------------
// 1. Enable CLK
// 2. Enable voltage regulator
// 3. Calibration
// 4. Enable temperature sensor and internal reference voltage
// 5. Enable ADC
// 6. Order conversion and lenght
// 7. Sampling time
// 8. Set resolution
// 9. External trigger
// 10. Enable DMA mode
// 11 (disable). Setting AWD 1
// 12 (disable). Enable interrupt for EOS
// 13 (disable). Enable interrupt from ADC 1
static void 
adc1_init(void)
{
	// For delay
	int32_t i;
	
	// Enable analog function for ADC 1
	adc1_gpio_init();

  // 1. Enable CLK
	// Enable RCC for ADC 1 + delay
	RCC->AHBENR |= RCC_AHBENR_ADC1EN;
	for (i = 0; i < 15; ++i);
	// <RCC>
	// See bits description in reference manual + delay
  // WARNING: very important for calculate delay (datasheet) and sampling time
	ADC1_COMMON->CCR |= 
		ADC1_CCR_CKMODE_1     // Choose HCLK/2
		| ADC1_CCR_CKMODE_0;  // Choose HCLK/4
	// Wait t STAB == 1 t CONV (see datasheet -> 6.3.18)
	// <RCC>
  // WARNING: for 8 MHz and 'for' devide into 2 assembler operations
	for (i = 0; i < 2*(614) + 250; ++i);
	
  // 2. Enable voltage regulator
	// Enable voltage regulator sequence
	ADC1->CR &= ADC_CR_ADVREGEN;
	ADC1->CR |= ADC_CR_ADVREGEN_0;
	// Wait T ADCVREG_STUP == 10 us (see datasheet -> 6.3.18)
	// <RCC>
  // WARNING: for 8 MHz and 'for' devide into 2 assembler operations
	for (i = 0; i < 2*(40) + 15; ++i);
 
  // 3. Calibration
	// Enable calibration
	ADC1->CR |= ADC_CR_ADCAL;
	// Wait calibration complete (also see datasheet -> 6.3.18 t CAL)
	while (ADC1->CR & ADC_CR_ADCAL);
	// <RCC>
	for (i = 0; i < 2*4; ++i); // see "device errata"
	
  // 4. Enable temperature sensor and internal reference voltage
	ADC1_COMMON->CCR |= ADC_CCR_TSEN | ADC_CCR_VREFEN;
	// Wait t START - Startup time == 10 us (see datasheet -> 6.3.22)
	// <RCC>
  // WARNING: for 8 MHz and 'for' devide into 2 assembler operations
	for (i = 0; i < 2*(40) + 15; ++i);
	
  // 5. Enable ADC
	// Enable ADC 1
	ADC1->CR |= ADC_CR_ADEN;
	// Wait ready ADC 1
	while (!(ADC1->ISR & ADC_ISR_ADRDY));
	// Clear ready flag
	// ADC1->ISR |= ADC_ISR_ADRDY;
	
  // 6. Order conversion and lenght: 
	// ADCx_SQR1_1 = ADC1_IN13
	// ADCx_SQR1_2 = ADC1_IN16 
	// ADCx_SQR1_3 = ADC1_IN18 
	// lenght L = 3
	ADC1->SQR1 |= 
		ADC_SQR1_SQ1_0 | ADC_SQR1_SQ1_2 | ADC_SQR1_SQ1_3 |  // 13
		ADC_SQR1_SQ2_4 |                                    // 16
		ADC_SQR1_SQ3_1 | ADC_SQR1_SQ3_4 |                   // 18
		ADC_SQR1_L_1;                                       // L - 1
	
	// <RCC> and HCLK prescaler (see above)
  // 7. Sampling time
	// 601.5 ADC clock cycles for potentiometer 
	// 19.5 ADC clock cycles for TempSens (2.2 us; see datasheet: 6.3.22)
	// 19.5 ADC clock cycles for T_S_vrefint (2.2 us; see datasheet: 6.3.4)
	ADC1->SMPR1 |= 
		ADC_SMPR1_SMP1_0 | ADC_SMPR1_SMP1_1 | ADC_SMPR1_SMP1_2 |
		ADC_SMPR1_SMP2_2 |
		ADC_SMPR1_SMP3_2;
	
  // 8. Set resolution (T SAR depends on RES[2:0] (Table 89) and Figure 58 !!!)
	// Resolution: 12 bit (t sar == xx ADC clock cycles (Figure 58))
	// ADC1->CFGR |= ADC_CFGR_RES_1;  // 8 bit
	
  // 9. External trigger
	ADC1->CFGR |= 
		ADC_CFGR_EXTEN_0 | ADC_CFGR_EXTEN_1 |   // rising and falling
		ADC_CFGR_EXTSEL_0 | ADC_CFGR_EXTSEL_1;  // TIM2_CC2 (EXT3)
	
  // 10. Enable DMA mode
	ADC1->CFGR |= ADC_CFGR_DMAEN;
	for (i = 0; i < 10; ++i);
	// Enable DMA circular mode
	ADC1->CFGR |= ADC_CFGR_DMACFG;
	
  // 11. Setting AWD 1
	// Clear higher threshold reset value
	// ADC1->TR1 &= ~0x0FFF0000U;
	// for (i = 0; i < 3; ++i);
	// Set higher and lower threshold
	// ADC1->TR1 |= THRS_L | THRS_H << 16;
	// ADC1->CFGR |= 
		// Choose channel 13
		// ADC_CFGR_AWD1CH_0 | ADC_CFGR_AWD1CH_2 | ADC_CFGR_AWD1CH_3 | 
		// ADC_CFGR_AWD1EN |  // Enable AWD 1
		// ADC_CFGR_AWD1SGL;  // Single channel
	
  // 12. Enable interrupt for EOS
	// ADC1->IER |= ADC_IER_EOSIE;
	
  // 13. Enable interrupt from ADC 1
	// NVIC_EnableIRQ(ADC1_IRQn);
}
//-----------------------------------------------------------------------------
void 
focus_init(void)
{
	focus_pos = &adc_val[0];
	temp = &adc_val[1];
	vref = &adc_val[2];
	
	focus_state = FOCUS_STATE_NOSTART;
	
	keys_init();
	dma1_init();
	tim2_init();
	adc1_init();
}
//=============================================================================
static void 
dma1_start(void)
{
	// Activate DMA 1 Channel 1
	DMA1_Channel1->CCR |= DMA_CCR_EN;
}
//-----------------------------------------------------------------------------
static void 
tim2_start(void)
{
	// Activate TIM 2
	TIM2->CR1 |= TIM_CR1_CEN;
}
//-----------------------------------------------------------------------------
static void 
adc1_start(void)
{
	// Activate ADC 1
	ADC1->CR |= ADC_CR_ADSTART;
}
//-----------------------------------------------------------------------------
void 
focus_start(void)
{
	dma1_start();
	adc1_start();
	tim2_start();
}
//=============================================================================
void 
focus_keysEn(void)
{
	// Enable EN_3
	GPIOB->BSRR |= GPIO_BSRR_BS_12;
}
//-----------------------------------------------------------------------------
void 
focus_keysDis(void)
{
	// Disable EN_3
	GPIOB->BSRR |= GPIO_BSRR_BR_12;
}
//-----------------------------------------------------------------------------
void 
focus_keysForward(void)
{
	// Set MC3_N, reset MC3_P
	GPIOA->BSRR |= GPIO_BSRR_BS_0 | GPIO_BSRR_BR_1;
}
//-----------------------------------------------------------------------------
void 
focus_keysBack(void)
{
	// Reset MC3_N, set MC3_P
	GPIOA->BSRR |= GPIO_BSRR_BR_0 | GPIO_BSRR_BS_1;
}
//-----------------------------------------------------------------------------
void 
focus_keysStop(void)
{
	// Reset MC3_N, reset MC3_P
	GPIOA->BSRR |= GPIO_BSRR_BR_0 | GPIO_BSRR_BR_1;
}
//=============================================================================
uint32_t 
focus_getState(void)
{
	return focus_state;
}
//=============================================================================
void 
ADC1_IRQHandler(void)
{
	/*
	if (ADC1->ISR & ADC_ISR_AWD1) {
		// Clear flag
		ADC1->ISR |= ADC_ISR_AWD1;
		state = STATE_AWD;
	} else {
		state = STATE_OK;
	}
	*/
	/*
	// Exclude the cause of the interrupt
	ADC1->ISR |= ADC_ISR_EOS;
	// Delay due to conveyor
	__nop();
	__nop();
	__nop();
	*/
}
//=============================================================================
void 
DMA1_Channel1_IRQHandler(void)
{
	// Start flag
	static uint32_t first_time;
	
	if (DMA1->ISR & DMA_ISR_TCIF1) { 
		// Exclude the cause of the interrupt
		DMA1->IFCR |= DMA_IFCR_CTCIF1;
		// Enable keys
		focus_keysEn();
		
		if (!first_time) {
			// Clear NOSTART flag (for main loop)
			focus_state &= ~FOCUS_STATE_NOSTART;
			// Set focus_target as focus_pos
			focus_target = 
				(*focus_pos & FOCUS_MASK) / FOCUS_DEVIDER;
			// Set first_time flag
			first_time = 1;
		}
	}
	
	if (DMA1->ISR & DMA_ISR_TCIF1) {
		// Exclude the cause of the interrupt
		DMA1->IFCR |= DMA_IFCR_CTEIF1;
		// Disable keys
		focus_keysDis();
		// Set ERR flag
		focus_state |= FOCUS_STATE_ERR;
	} else {
		// Clear ERR flag
		focus_state &= ~FOCUS_STATE_ERR;
	}
}
//=============================================================================
