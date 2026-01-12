/*
 * File: hal.c
 * Description: Hardware Abstraction Layer for the
 * STM32F407 Discovery and STM32L4R5ZI Nucleo boards.
 *
 * Author: CryptoExperts
 * 
 * Copyright CryptoExperts, 2024
 *
 */

#include "hal.h"

#if defined(STM32F407VG)
/* 24 MHz */
const struct rcc_clock_scale benchmarkclock = {
  .pllm = 8, //VCOin = HSE / PLLM = 1 MHz
  .plln = 192, //VCOout = VCOin * PLLN = 192 MHz
  .pllp = 8, //PLLCLK = VCOout / PLLP = 24 MHz (low to have 0WS)
  .pllq = 4, //PLL48CLK = VCOout / PLLQ = 48 MHz (required for USB, RNG)
  .pllr = 0,
  .hpre = RCC_CFGR_HPRE_DIV_NONE,
  .ppre1 = RCC_CFGR_PPRE_DIV_2,
  .ppre2 = RCC_CFGR_PPRE_DIV_NONE,
  .pll_source = RCC_CFGR_PLLSRC_HSE_CLK,
  .voltage_scale = PWR_SCALE1,
  .flash_config = FLASH_ACR_LATENCY_0WS,
  .ahb_frequency = 24000000,
  .apb1_frequency = 12000000,
  .apb2_frequency = 24000000,
};
#elif defined(STM32F437)
const struct rcc_clock_scale benchmarkclock = {
  .pllm = 16, 
  .plln = 336, 
  .pllp = 8,
  .pllq = 4,
  .pllr = 0,
  .hpre = RCC_CFGR_HPRE_DIV_NONE,
  .ppre1 = RCC_CFGR_PPRE_DIV_2,
  .ppre2 = RCC_CFGR_PPRE_DIV_2,
  .pll_source = RCC_CFGR_PLLSRC_HSE_CLK,
  .voltage_scale = PWR_SCALE1,
  .flash_config = FLASH_ACR_LATENCY_0WS,
  .ahb_frequency = 21000000,
  .apb1_frequency = 21000000,
  .apb2_frequency = 21000000,
};
#elif defined(STM32L4R5ZI) 
/* Patched function for newer PLL not yet supported by opencm3 */
void _rcc_set_main_pll(uint32_t source, uint32_t pllm, uint32_t plln, uint32_t pllp,
                       uint32_t pllq, uint32_t pllr)
{   
        RCC_PLLCFGR = (RCC_PLLCFGR_PLLM(pllm) << RCC_PLLCFGR_PLLM_SHIFT) |
                (plln << RCC_PLLCFGR_PLLN_SHIFT) |
                ((pllp & 0x1Fu) << 27u) | /* NEWER PLLP */
                (source << RCC_PLLCFGR_PLLSRC_SHIFT) |
                (pllq << RCC_PLLCFGR_PLLQ_SHIFT) |
                (pllr << RCC_PLLCFGR_PLLR_SHIFT) | RCC_PLLCFGR_PLLREN;
}   
#else
#error "Unsupported board"
#endif

uint32_t _clock_freq;

static void clock_setup(enum clock_mode clock)
{   
#if defined(DISCOVERY_BOARD)
  switch(clock) {
  case CLOCK_BENCHMARK:
    rcc_clock_setup_pll(&benchmarkclock);
    _clock_freq = 24000000; 
    break;
  case CLOCK_FAST:
  default:
    rcc_clock_setup_pll(&rcc_hse_8mhz_3v3[RCC_CLOCK_3V3_168MHZ]);
    _clock_freq = 168000000;
    break;
  }

  rcc_periph_clock_enable(RCC_RNG);
  rng_enable();

  flash_prefetch_enable();
#elif defined(LEIA_BOARD)
  switch(clock) {
  case CLOCK_BENCHMARK:
    rcc_clock_setup_pll(&benchmarkclock);
    _clock_freq = 21000000; 
    break;
  case CLOCK_FAST:
  default:
    rcc_clock_setup_pll(&rcc_hse_16mhz_3v3[RCC_CLOCK_3V3_168MHZ]);
    _clock_freq = 168000000;
    break;
  }

  rcc_periph_clock_enable(RCC_RNG);
  rng_enable();

  rcc_periph_clock_enable(RCC_CRYP);

  flash_prefetch_enable();
#elif defined(NUCLEO_L4R5_BOARD)
  rcc_periph_clock_enable(RCC_PWR);
  rcc_periph_clock_enable(RCC_SYSCFG);
  pwr_set_vos_scale(PWR_SCALE1);
  switch (clock) {
  case CLOCK_BENCHMARK:
    /* Benchmark straight from the HSI16 without prescaling */
    rcc_osc_on(RCC_HSI16);
    rcc_wait_for_osc_ready(RCC_HSI16);
    rcc_ahb_frequency = 16000000;
    rcc_apb1_frequency = 16000000;
    rcc_apb2_frequency = 16000000;
    _clock_freq = 16000000;
    rcc_set_hpre(RCC_CFGR_HPRE_NODIV);
    rcc_set_ppre1(RCC_CFGR_PPRE1_NODIV);
    rcc_set_ppre2(RCC_CFGR_PPRE2_NODIV);
    flash_dcache_enable();
    flash_icache_enable();
    flash_set_ws(FLASH_ACR_LATENCY_0WS);
    flash_prefetch_enable();
    rcc_set_sysclk_source(RCC_CFGR_SW_HSI16);
    rcc_wait_for_sysclk_status(RCC_HSI16);
    break;
  case CLOCK_FAST:
  default:
    rcc_osc_on(RCC_HSI16);
    rcc_wait_for_osc_ready(RCC_HSI16);
    rcc_ahb_frequency = 120000000;
    rcc_apb1_frequency = 120000000;
    rcc_apb2_frequency = 120000000;
    _clock_freq = 120000000;
    rcc_set_hpre(RCC_CFGR_HPRE_NODIV);
    rcc_set_ppre1(RCC_CFGR_PPRE1_NODIV);
    rcc_set_ppre2(RCC_CFGR_PPRE2_NODIV);
    rcc_osc_off(RCC_PLL);
    while(rcc_is_osc_ready(RCC_PLL));
    /* Configure the PLL oscillator (use CUBEMX tool -> scale HSI16 to 120MHz). */
    _rcc_set_main_pll(RCC_PLLCFGR_PLLSRC_HSI16, 2, 30, 2u, RCC_PLLCFGR_PLLQ_DIV2, RCC_PLLCFGR_PLLR_DIV2);
    /* Enable PLL oscillator and wait for it to stabilize. */
    rcc_osc_on(RCC_PLL);
    rcc_wait_for_osc_ready(RCC_PLL);
    flash_dcache_enable();
    flash_icache_enable();
    flash_set_ws(0x05);
    flash_prefetch_enable();
    rcc_set_sysclk_source(RCC_CFGR_SW_PLL);
    rcc_wait_for_sysclk_status(RCC_PLL);
    break;
  }
  rcc_osc_on(RCC_HSI48); /* HSI48 must always be on for RNG */
  rcc_periph_clock_enable(RCC_RNG);
  rng_enable();
#else
#error "Unsupported platform"
#endif
}

void usart_setup(void)
{
#if defined(DISCOVERY_BOARD)
  rcc_periph_clock_enable(RCC_GPIOA);
  rcc_periph_clock_enable(RCC_USART2);
#elif defined(LEIA_BOARD)
  rcc_periph_clock_enable(RCC_GPIOB);
  rcc_periph_clock_enable(RCC_USART1);
#elif defined(NUCLEO_L4R5_BOARD)
  rcc_periph_clock_enable(RCC_GPIOG);
  rcc_periph_clock_enable(RCC_LPUART1);

  PWR_CR2 |= PWR_CR2_IOSV;
  gpio_set_output_options(SERIAL_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ, SERIAL_PINS);
  gpio_set_af(SERIAL_GPIO, GPIO_AF8, SERIAL_PINS);
  gpio_mode_setup(SERIAL_GPIO, GPIO_MODE_AF, GPIO_PUPD_NONE, SERIAL_PINS);
  usart_set_baudrate(SERIAL_USART, SERIAL_BAUD);
  usart_set_databits(SERIAL_USART, 8);
  usart_set_stopbits(SERIAL_USART, USART_STOPBITS_1);
  usart_set_mode(SERIAL_USART, USART_MODE_TX_RX);
  usart_set_parity(SERIAL_USART, USART_PARITY_NONE);
  usart_set_flow_control(SERIAL_USART, USART_FLOWCONTROL_NONE);
  usart_disable_rx_interrupt(SERIAL_USART);
  usart_disable_tx_interrupt(SERIAL_USART);
  usart_enable(SERIAL_USART);
#else
#error "Unsupported platform"
#endif

#if defined(DISCOVERY_BOARD) || defined(LEIA_BOARD)
  gpio_set_output_options(SERIAL_GPIO, GPIO_OTYPE_OD, GPIO_OSPEED_100MHZ, SERIAL_PINS);
  gpio_set_af(SERIAL_GPIO, GPIO_AF7, SERIAL_PINS);
  gpio_mode_setup(SERIAL_GPIO, GPIO_MODE_AF, GPIO_PUPD_PULLUP, SERIAL_PINS);
  usart_set_baudrate(SERIAL_USART, SERIAL_BAUD);
  usart_set_databits(SERIAL_USART, 8);
  usart_set_stopbits(SERIAL_USART, USART_STOPBITS_1);
  usart_set_mode(SERIAL_USART, USART_MODE_TX_RX);
  usart_set_parity(SERIAL_USART, USART_PARITY_NONE);
  usart_set_flow_control(SERIAL_USART, USART_FLOWCONTROL_NONE);
  usart_disable_rx_interrupt(SERIAL_USART);
  usart_disable_tx_interrupt(SERIAL_USART);
  usart_enable(SERIAL_USART);
#endif
}

void trigger_setup(void)
{
#if !defined(LEIA_BOARD)
  // For both the Discovery and Nucleo board, the trig is on PA1
  // NOTE: on the Nucleo board, PA1 is on connector CN10, connector 11 (A8),
  // i.e. the 6th connector on the left column of CN10 from up to bottom
  rcc_periph_clock_enable(RCC_GPIOA);
  gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO1);
#endif
}

void led_setup(void)
{
#if defined(DISCOVERY_BOARD)
  // For the Discovery board, we use the PD15 blue led
  rcc_periph_clock_enable(RCC_GPIOD);
  gpio_mode_setup(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO15);
#elif defined(LEIA_BOARD)
  rcc_periph_clock_enable(RCC_GPIOC);
  gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO5);
#elif defined(NUCLEO_L4R5_BOARD)
  // For the Nucleo board, we use the PB7 blue led
  rcc_periph_clock_enable(RCC_GPIOB);
  gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO7);
#else
#error "Unkown board"
#endif
}

void led_set(void)
{
#if defined(DISCOVERY_BOARD)
  gpio_set(GPIOD, GPIO15);
#elif defined(LEIA_BOARD)
  gpio_set(GPIOC, GPIO5);
#elif defined(NUCLEO_L4R5_BOARD)
  gpio_set(GPIOB, GPIO7);
#else
#error "Unkown board"
#endif
}

void led_clear(void)
{
#if defined(DISCOVERY_BOARD)
  gpio_clear(GPIOD, GPIO15);
#elif defined(LEIA_BOARD)
  gpio_clear(GPIOC, GPIO5);
#elif defined(NUCLEO_L4R5_BOARD)
  gpio_clear(GPIOB, GPIO7);
#else
#error "Unkown board"
#endif
}

void led_toggle(void)
{
#if defined(DISCOVERY_BOARD)
  gpio_toggle(GPIOD, GPIO15);
#elif defined(LEIA_BOARD)
  gpio_toggle(GPIOC, GPIO5);
#elif defined(NUCLEO_L4R5_BOARD)
  gpio_toggle(GPIOB, GPIO7);
#else
#error "Unkown board"
#endif
}


void trigger_set(void)
{
#if !defined(LEIA_BOARD)
  gpio_set(GPIOA, GPIO1);
#endif
}

void trigger_clear(void)
{
#if !defined(LEIA_BOARD)
  gpio_clear(GPIOA, GPIO1);
#endif
}

volatile bool setup_ok = false;

void systick_setup(void)
{
  // Set systick frequency to 100 Hz
  systick_set_frequency(100, rcc_ahb_frequency);
  systick_interrupt_enable();
  systick_counter_enable();
  // Enable the cycle counter
  dwt_enable_cycle_counter();
  //
  setup_ok = true;
}


static volatile uint64_t last_dwt_sampling = 0;
static volatile uint64_t global_cycles_counter = 0;
static volatile uint64_t global_systick_counter = 0;

// "Freeze" the cycle counting to avoid counting extra cycles
#include <libopencm3/cm3/scs.h>
#include <libopencm3/cm3/dwt.h>
static uint32_t saved_cycles_counter = 0;
static uint8_t cycles_paused = 0;
void hal_pause_cycles(void)
{
	saved_cycles_counter = DWT_CYCCNT;
	cycles_paused = 1;
}
void hal_unpause_cycles(void)
{
	DWT_CYCCNT = saved_cycles_counter;
	cycles_paused = 0;
}

static volatile uint8_t mutex = 0;

static inline uint8_t mutex_acquire_asm(volatile uint8_t* lock) {
	volatile uint8_t success;
	__asm volatile(
		/* Assembly routine template */
		"          MOV      %[success], #1  ;\n" /* initialize success false */
		"          MOV      R4, #1          ;\n" /* initialise our target lock state */
		"          LDREXB   %[success], "
					"[%[lock]]  ;\n" /* Load the lock value */
		"          CMP      %[success], #0  ;\n" /* Is the lock free? */

		"          ITTT     EQ              ;\n" /* IF ( EQUAL ) THEN { STREX, CMP, DMB } */
		"          STREXBEQ %[success], R4, "
					"[%[lock]]  ;\n" /* Try and claim the lock */
		"          CMPEQ    %[success], #0  ;\n" /* Did this succeed? */
		"          DMBEQ                    ;\n" /* data memory barrier before
							    accessing restricted resource */
		/* Outputs */
		: [success] "=r" (success),	/* =r: "we write to this register" */
		  [lock] "+r" (lock)		/* +r: "we write to *and read from* this register */
		/* Inputs */
		:
		/* Clobber list: this stuff is changed by our code */
		: "memory", "r4" );

	return success == 0;
}

static inline uint8_t mutex_release_asm(volatile uint8_t* lock) {
	volatile uint8_t success;
	__asm volatile(
		/* Assembly routine template */
		"          MOV      %[success], #1  ;\n" /* initialize success false */
		"          MOV      R4, #0          ;\n" /* initialise our target lock state */
		"          LDREXB   %[success], "
					"[%[lock]]  ;\n" /* Load the lock value */
		"          CMP      %[success], #1  ;\n" /* Is the lock in use? */

		"          ITTT     EQ              ;\n" /* IF ( EQUAL ) THEN { STREX, CMP, DMB } */
		"          STREXBEQ %[success], R4, "
					"[%[lock]]  ;\n" /* Try and release the lock */
		"          CMPEQ    %[success], #0  ;\n" /* Did this succeed? */
		"          DMBEQ                    ;\n" /* data memory barrier before
							    accessing restricted resource */
		/* Outputs */
		: [success] "=r" (success),	/* =r: "we write to this register" */
		  [lock] "+r" (lock)		/* +r: "we write to *and read from* this register */
		/* Inputs */
		:
		/* Clobber list: this stuff is changed by our code */
		: "memory", "r4" );

	return success == 0;
}

void disable_interrupts(void);
void enable_interrupts(void); 

static void update_cycles(void)
{
	if(cycles_paused){
		return;
	}
	disable_interrupts();

	if(!mutex_acquire_asm(&mutex)){
		enable_interrupts();
		return;
	}

	uint64_t current_dwt_sampling = dwt_read_cycle_counter();
	// Check for overflow
	if(current_dwt_sampling < last_dwt_sampling){
		global_cycles_counter += (current_dwt_sampling + (((uint64_t)1 << 32) - last_dwt_sampling));
	}
	else{
		global_cycles_counter += (current_dwt_sampling - last_dwt_sampling);
	}
	last_dwt_sampling = current_dwt_sampling;

	// Release the mutex
	while(!mutex_release_asm(&mutex)){};

	enable_interrupts();
}

void sys_tick_handler(void)
{
	update_cycles();
	if(!cycles_paused){
		global_systick_counter++;
	}
}

uint64_t hal_get_cycles(void)
{
	update_cycles();
	return global_cycles_counter;
}

void hal_reset_cycles(void)
{
	DWT_CYCCNT = 0;
}

uint64_t hal_get_time(void)
{
  return hal_get_cycles();
}

uint64_t hal_get_systick_counter(void)
{
  return global_systick_counter;
}

#if defined(LEIA_BOARD)
static inline uint64_t SWAP_KEY(uint64_t x) {
	x = (x & 0x0000FFFF0000FFFF) << 16 | (x & 0xFFFF0000FFFF0000) >> 16;
	x = (x & 0x00FF00FF00FF00FF) << 8  | (x & 0xFF00FF00FF00FF00) >> 8;
	return x;
}

static void _crypto_set_key(enum crypto_keysize keysize, uint64_t key[])
{       
        int i;

        crypto_wait_busy();

        CRYP_CR = (CRYP_CR & ~CRYP_CR_KEYSIZE) |
                  (keysize << CRYP_CR_KEYSIZE_SHIFT);

	if(keysize == CRYPTO_KEY_128BIT){
	        for (i = 0; i < 2; i++) {
        	        CRYP_KR(3-i) = SWAP_KEY(key[1-i]);
	        }
	}
	else if(keysize == CRYPTO_KEY_192BIT){
	        for (i = 0; i < 3; i++) {
        	        CRYP_KR(3-i) = SWAP_KEY(key[2-i]);
	        }
	}
	else{
	        for (i = 0; i < 4; i++) {
        	        CRYP_KR(3-i) = SWAP_KEY(key[3-i]);
	        }
	}

}

void hal_cryp_aes_128_set_key(const uint8_t key[16])
{
        _crypto_set_key(CRYPTO_KEY_128BIT, (uint64_t*)key);
        crypto_set_datatype(CRYPTO_DATA_8BIT);
        crypto_set_algorithm(ENCRYPT_AES_ECB);
}

void hal_cryp_aes_128_enc(const uint8_t *pt, uint8_t *ct, uint32_t sz)
{
 	crypto_start();
        crypto_process_block((uint32_t*)pt, (uint32_t*)ct, sz / sizeof(uint32_t));
	crypto_stop();
}

void hal_cryp_aes_256_set_key(const uint8_t key[32])
{
        _crypto_set_key(CRYPTO_KEY_256BIT, (uint64_t*)key);
        crypto_set_datatype(CRYPTO_DATA_8BIT);
        crypto_set_algorithm(ENCRYPT_AES_ECB);
}

void hal_cryp_aes_256_enc(const uint8_t *pt, uint8_t *ct, uint32_t sz)
{
        crypto_start();
        crypto_process_block((uint32_t*)pt, (uint32_t*)ct, sz / sizeof(uint32_t));
        crypto_stop();
}

#endif

void hal_setup(const enum clock_mode clock)
{ 
  clock_setup(clock);
  usart_setup();
  trigger_setup();
  led_setup();
  rng_setup();
  systick_setup();
}

// XXX: optimize this at O0 level to keep accurate stack spraying and checking
#if defined(__clang__) || defined(CLANG_DETECTED)
#pragma clang optimize off
#else
#pragma GCC optimize ("O0")
#endif

#define STACK_SPRAY_VALUE 0xbc
static volatile uint32_t sp_check = 0;

// This is the extern variable that should be provided by the linker script
extern uint32_t _ebss;

void spray_stack(uint32_t max)
{
	register uint32_t i;
	// Get the current stack pointer
	register uint32_t curr_sp;

	hal_pause_cycles();

	asm volatile ("mov %0, sp\n\t"
                : "=r" (curr_sp)
                : /* no input */
                : );

	i = (max == 0) ? (uint32_t)&_ebss : (curr_sp - max);
	sp_check = curr_sp;
	while(i < curr_sp){
		*((uint8_t*)i) = STACK_SPRAY_VALUE;
		i++;
	}

	hal_unpause_cycles();
}

uint32_t check_stack(uint32_t max)
{
	register uint32_t i;

	hal_pause_cycles();

	i = (max == 0) ? (uint32_t)&_ebss : (sp_check - max);
	while((i <= sp_check) && (*((uint8_t*)i) == STACK_SPRAY_VALUE)){
		i++;
	}

	hal_unpause_cycles();

	return (sp_check - i);
}

#if defined(__clang__)
#pragma clang optimize on
#else
#pragma GCC reset_options
#endif

void disable_interrupts(void)
{
	asm volatile ("cpsid i" : : : "memory");
}

void enable_interrupts(void)
{
	asm volatile ("cpsie i" : : : "memory");
}

void rng_setup(void)
{
  // Enable the RNG
  rng_enable();
}

__attribute__((used)) static inline uint32_t fast_get_hw_rand(void)
{
  while (1) {
    if ((RNG_SR & RNG_SR_DRDY) == 1) { // check if data is ready
      return RNG_DR;
    }
  }
}

// HW TRNG provider
// We use a "blocking" approach until we have our random
int hw_trng_get_random(unsigned char *buf, uint32_t len)
{
  uint32_t rnd;
  uint32_t treated = 0;
  unsigned int i;

  if(len == 0){
    goto out;
  }
  while(1){
    rnd = rng_get_random_blocking();
    if((len-treated) >= 4){
        *((uint32_t*)&buf[treated]) = rnd;
        treated += 4;
        if(treated == len){
          goto out;
        }
    }
    else{
        for(i = 0; i < 4; i++){
            buf[treated++] = (rnd >> (8 * i)) & 0xff;
            if(treated == len){
              goto out;
            }
        }
    }
  }

out:
  return 0;
}

void hal_sleep(uint32_t t)
{
	volatile uint32_t i;
	for(i = 0; i < t; i++){
		asm volatile ("nop");
	}
        return;
}
