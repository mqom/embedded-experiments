/*
 * Author: CryptoExperts
 * 
 */

#include "hal.h"
// Add stdio header for printf
#include <stdio.h>

#if defined(STM32F407VG) || defined(STM32F437)
#define _USART_ISR_RXNE USART_SR_RXNE
#define _USART_ISR_TXE USART_SR_TXE
#define _USART_ISR(a) USART_SR(a)
#elif defined(STM32L4R5ZI)
#define _USART_ISR_RXNE USART_ISR_RXNE
#define _USART_ISR_TXE USART_ISR_TXE
#define _USART_ISR(a) USART_ISR(a)
#else
#error "Unsupported board"
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// Wire the printf and scanf functions to the USART methods
int _write(int iFileHandle, char *pcBuffer, int iLength)
{
	int i;
	for(i = 0; i < iLength; i++){
		usart_send_blocking(SERIAL_USART, pcBuffer[i]);
	}
	return 0;
}

int _read(int iFileHandle, char *pcBuffer, int iLength)
{
	int i;
	for(i = 0; i < iLength; i++){
		pcBuffer[i] = usart_recv_blocking(SERIAL_USART);
	}
	return iLength;
}

/* Dummy definitions for unused syscalls */
__attribute__((weak)) void _close(void)
{
	return;
}
__attribute__((weak)) void _fstat(void)
{
	return;
}
__attribute__((weak)) void _getpid(void)
{
	return;
}
__attribute__((weak)) void _isatty(void)
{
	return;
}
__attribute__((weak)) void _kill(void)
{
	return;
}
__attribute__((weak)) void _lseek(void)
{
	return;
}


__attribute__((constructor)) void setup(void)
{
#if !defined(EMBEDDED_CLOCK_FAST)
        hal_setup(CLOCK_BENCHMARK);
	printf("[+] HAL setup done for %s (clock CLOCK_BENCHMARK)\r\n", TOSTRING(CURRENT_BOARD));
#else
        hal_setup(CLOCK_FAST);
	printf("[+] HAL setup done for %s (clock CLOCK_FAST)\r\n", TOSTRING(CURRENT_BOARD));
#endif
#if !defined(NO_EMBEDDED_GO_CHAR)
	// Wait for the user to press 'g' to launch
	char c;
	led_set();

	printf("[+] Waiting for the user to press on 'g' to go ...\r\n");
	while(1){
		c = usart_recv_blocking(SERIAL_USART);
		if(c == 'g'){
			printf("'g' received, go!\r\n");
			break;
		}
		led_toggle();
	}
#endif
}

int user_defined_main(void)
{
	printf("=== Welcome to the tests! (on board %s)\r\n", TOSTRING(CURRENT_BOARD));

	return 0;
}

__attribute__((weak)) int main(int argc, char *argv[])
{
	// Execute the user main
	user_defined_main();	

	while(1){};	
}
