#include <arch/x86_64/pit.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/io.h>
#include <arch/x86_64/pic.h>
#include <drivers/ioapic.h>
#include <kernel/kprint.h>

#define PIT_CHANNEL_0_DATA 0x40
#define PIT_COMMAND_REG 0x43
#define PIT_BASE_FREQUENCY 1193182

static volatile uint64_t g_pit_ticks = 0;
static uint32_t g_pit_freq = 100;

#include <sched/sched.h>
#include <drivers/keyboard.h>
#include <drivers/xhci.h>
#include <drivers/ehci.h>

static void pit_irq_handler(interrupt_frame_t *frame) {
    UNUSED(frame);
    g_pit_ticks++;
    /* NOTE: do NOT poll the i8042 here — on laptop ECs each port read costs
     * ~5-10 µs and a streaming touchpad keeps OBF set, so this handler would
     * consume most of the CPU in interrupt context and starve every thread.
     * The console read path polls port 0x60 itself; IRQ1 delivers when it
     * works. */
    xhci_poll();
    ehci_poll();
    sched_tick();
}


void pit_init(uint32_t frequency_hz) {
    if (frequency_hz == 0)
        frequency_hz = 100;
    g_pit_freq = frequency_hz;

    uint32_t divisor = PIT_BASE_FREQUENCY / frequency_hz;
    if (divisor > 65535)
        divisor = 65535;
    if (divisor == 0)
        divisor = 1;

    /* Channel 0, Access mode low/high byte, Mode 2 (rate generator), Binary mode */
    outb(PIT_COMMAND_REG, 0x36);
    outb(PIT_CHANNEL_0_DATA, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL_0_DATA, (uint8_t)((divisor >> 8) & 0xFF));

    isr_register_handler(IRQ0, pit_irq_handler);
    /* NOTE: IRQ0 routing happens in pit_route_irq() — called by the platform
     * init only when the LAPIC timer is unavailable. */

    klog_info("PIT initialized at %u Hz (divisor: %u)", frequency_hz, divisor);
}

/*
 * Route the legacy PIT IRQ0 to the CPU. Used ONLY when the LAPIC timer is
 * unavailable (e.g. machines without a Local APIC). The PIT chip itself
 * keeps running in all cases (TSC calibration reads its counter latch).
 */
void pit_route_irq(void) {
    if (ioapic_is_active()) {
        ioapic_map_irq(0, 0x20, 0, false, false);
        klog_info("PIT: IRQ0 routed via IO-APIC (GSI 0/ISO)");
    } else {
        pic_clear_mask(0); /* Unmask IRQ0 (Timer) on legacy PIC */
        klog_info("PIT: IRQ0 routed via legacy 8259 PIC");
    }
}

uint64_t pit_get_ticks(void) {
    return g_pit_ticks;
}

uint32_t pit_get_frequency(void) {
    return g_pit_freq ? g_pit_freq : 1000;
}

void pit_sleep(uint32_t ms) {
    uint64_t target_ticks = g_pit_ticks + ((uint64_t)ms * g_pit_freq) / 1000;
    while (g_pit_ticks < target_ticks) {
        __asm__ volatile("pause");
        io_wait();
    }
}
