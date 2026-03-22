#include "kernel.h"

/* ═══════════════════════════════════════════════════════════════════
   PIT  TIMER  (Intel 8253/8254)
   ═══════════════════════════════════════════════════════════════════ */

#define PIT_CH0   0x40
#define PIT_CMD   0x43
#define PIT_BASE  1193182u   /* Hz */

static volatile uint32_t ticks = 0;
static uint32_t tick_hz = 100;

static void timer_irq(void) {
    ticks++;
    sched_tick();
}

void timer_init(uint32_t hz) {
    tick_hz = hz;
    uint32_t divisor = PIT_BASE / hz;
    outb(PIT_CMD, 0x36);                        /* channel 0, lobyte/hibyte, mode 3 */
    outb(PIT_CH0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0, (uint8_t)((divisor >> 8) & 0xFF));
    irq_install_handler(0, timer_irq);
    /* Unmask IRQ0 */
    outb(0x21, inb(0x21) & ~0x01);
}

uint32_t timer_ticks(void)   { return ticks; }
uint32_t timer_seconds(void) { return ticks / tick_hz; }
