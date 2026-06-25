#include <stdint.h>

volatile uint32_t heartbeat;

int main(void)
{
    for (;;) {
        heartbeat++;
        for (volatile uint32_t delay = 0; delay < 200000; delay++) {
            __asm__ volatile ("nop");
        }
    }
}
