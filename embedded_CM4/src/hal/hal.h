/*
 * File: hal.h
 * Description: Hardware Abstraction Layer for the
 * STM32F407 Discovery and STM32L4R5ZI Nucleo boards.
 *
 * Author: CryptoExperts
 * 
 * Copyright CryptoExperts, 2024
 *
 */

#ifndef HAL_H
#define HAL_H

#include <stdint.h>
#include <stdlib.h>

#include <libopencm3/cm3/dwt.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/rng.h>
#include <libopencm3/stm32/pwr.h>
#include <libopencm3/stm32/dma.h>

#include "cryp_dma.h"

#if defined(STM32F407VG)
#define SERIAL_GPIO GPIOA
#define SERIAL_USART USART2
#define SERIAL_PINS (GPIO2 | GPIO3)
#define STM32
#define DISCOVERY_BOARD
#elif defined(STM32L4R5ZI)
#define SERIAL_GPIO GPIOG
#define SERIAL_USART LPUART1
#define SERIAL_PINS (GPIO8 | GPIO7)
#define NUCLEO_L4R5_BOARD
#elif defined(STM32F437)
#define SERIAL_GPIO GPIOB
#define SERIAL_USART USART1
#define SERIAL_PINS (GPIO6 | GPIO7)
#define LEIA_BOARD
// Include crypto for AES
#include <libopencm3/stm32/crypto.h>
#else
#error "Unsupported board"
#endif

#define _RCC_CAT(A, B) A ## _ ## B
#define RCC_ID(NAME) _RCC_CAT(RCC, NAME)

// USART SERIAL BAUD
#define SERIAL_BAUD 38400

enum clock_mode {
    CLOCK_FAST,
    CLOCK_BENCHMARK
};

void hal_setup(const enum clock_mode clock);
uint64_t hal_get_cycles(void);
void hal_reset_cycles(void);
uint64_t hal_get_time(void);
void hal_pause_cycles(void);
void hal_unpause_cycles(void);
uint64_t hal_get_systick_counter(void);

void led_set(void);
void led_clear(void);
void led_toggle(void);
void trigger_set(void);
void trigger_clear(void);

void spray_stack(uint32_t max);
uint32_t check_stack(uint32_t max);

void disable_interrupts(void);
void enable_interrupts(void);

void rng_setup(void);
int hw_trng_get_random(unsigned char *buf, uint32_t len);

void hal_sleep(uint32_t t);

#ifdef LEIA_BOARD
void hal_cryp_aes_128_set_key(const uint8_t key[16]);
void hal_cryp_aes_128_set_key_dma(const uint8_t key[16]);
void hal_cryp_aes_128_enc(const uint8_t *pt, uint8_t *ct, uint32_t sz);
void hal_cryp_aes_128_enc_dma(const uint8_t *pt, uint8_t *ct, uint32_t sz);
void hal_cryp_aes_256_set_key(const uint8_t key[32]);
void hal_cryp_aes_256_enc(const uint8_t *pt, uint8_t *ct, uint32_t sz);
#endif

#endif

