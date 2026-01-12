#include <benchmark/utils.h>
#include <benchmark.h>
#include <benchmark/timing.h>

/* Redirect platform_get_cycles to the board's HAL get cycles */
extern uint64_t hal_get_cycles(void);
long long platform_get_cycles(void){
        return (long long)hal_get_cycles();
}               

/* Implement our local gettimeofday */
extern uint32_t _clock_freq;
extern uint64_t hal_get_systick_counter(void);
void gettimeofday(struct timeval *t, void *dummy)
{   
        (void)dummy;
        uint64_t ctr = hal_get_systick_counter();
        t->tv_sec  = (ctr / 100);
        t->tv_usec = (ctr - (100 * (ctr / 100))) * 10000;
 
        return;
}               
 
void panic(void){
        printf("Panic!\r\n");
        while(1){}
}
