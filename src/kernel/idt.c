#include "kernel.h"

/* ═══════════════════════════════════════════════════════════════════
   IDT  –  Interrupt Descriptor Table  (256 entries)
   PIC  –  8259A Programmable Interrupt Controller
   ═══════════════════════════════════════════════════════════════════ */

/* ── IDT entry ─────────────────────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint16_t offset_lo;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;   /* 0x8E = present, ring0, 32-bit interrupt gate */
    uint16_t offset_hi;
} idt_entry_t;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint32_t base;
} idt_ptr_t;

static idt_entry_t idt[256];
static idt_ptr_t   idt_ptr;

/* IRQ handler table (IRQ 0-15) */
static void (*irq_handlers[16])(void);

/* ── PIC constants ─────────────────────────────────────────────────── */
#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI   0x20

/* ── Set one IDT gate ──────────────────────────────────────────────── */
static void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_lo = (uint16_t)(base & 0xFFFF);
    idt[num].offset_hi = (uint16_t)((base >> 16) & 0xFFFF);
    idt[num].selector  = sel;
    idt[num].zero      = 0;
    idt[num].type_attr = flags;
}

/* ── ISR stubs (exceptions 0-31) ───────────────────────────────────── */
/* We use a macro to generate naked stubs that push the vector and jump
   to a common C handler.  The __attribute__((naked)) + inline asm
   approach works with -ffreestanding GCC.                             */

#define ISR_NOERR(n) \
    __attribute__((naked)) static void isr##n(void) { \
        __asm__ volatile("cli; push $0; push $" #n "; jmp isr_common"); }

#define ISR_ERR(n) \
    __attribute__((naked)) static void isr##n(void) { \
        __asm__ volatile("cli; push $" #n "; jmp isr_common"); }

ISR_NOERR(0)  ISR_NOERR(1)  ISR_NOERR(2)  ISR_NOERR(3)
ISR_NOERR(4)  ISR_NOERR(5)  ISR_NOERR(6)  ISR_NOERR(7)
ISR_ERR(8)    ISR_NOERR(9)  ISR_ERR(10)   ISR_ERR(11)
ISR_ERR(12)   ISR_ERR(13)   ISR_ERR(14)   ISR_NOERR(15)
ISR_NOERR(16) ISR_ERR(17)   ISR_NOERR(18) ISR_NOERR(19)
ISR_NOERR(20) ISR_NOERR(21) ISR_NOERR(22) ISR_NOERR(23)
ISR_NOERR(24) ISR_NOERR(25) ISR_NOERR(26) ISR_NOERR(27)
ISR_NOERR(28) ISR_NOERR(29) ISR_ERR(30)   ISR_NOERR(31)

/* ── IRQ stubs (IRQ 0-15 → vectors 32-47) ──────────────────────────── */
#define IRQ_STUB(n, v) \
    __attribute__((naked)) static void irq##n(void) { \
        __asm__ volatile("cli; push $0; push $" #v "; jmp irq_common"); }

IRQ_STUB(0,32) IRQ_STUB(1,33) IRQ_STUB(2,34)  IRQ_STUB(3,35)
IRQ_STUB(4,36) IRQ_STUB(5,37) IRQ_STUB(6,38)  IRQ_STUB(7,39)
IRQ_STUB(8,40) IRQ_STUB(9,41) IRQ_STUB(10,42) IRQ_STUB(11,43)
IRQ_STUB(12,44) IRQ_STUB(13,45) IRQ_STUB(14,46) IRQ_STUB(15,47)

/* ── Saved registers pushed by stubs ───────────────────────────────── */
typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} regs_t;

/* ── Common ISR handler (called from asm stub) ─────────────────────── */
__attribute__((used))
void isr_handler(regs_t *r) {
    char buf[64];
    ksnprintf(buf, sizeof(buf), "EXCEPTION %u (err=%u) at EIP=0x%x",
              r->int_no, r->err_code, r->eip);
    timeline_record("kernel", buf);
    term_setcolor(VGA_COLOR(VGA_WHITE, VGA_RED));
    term_writeln("\n  *** KERNEL EXCEPTION ***");
    term_writeln(buf);
    term_writeln("  System halted. Reboot required.");
    for (;;) __asm__ volatile("cli; hlt");
}

/* ── Common IRQ handler ────────────────────────────────────────────── */
__attribute__((used))
void irq_handler(regs_t *r) {
    uint8_t irq = (uint8_t)(r->int_no - 32);
    /* Send EOI */
    if (irq >= 8) outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
    /* Dispatch */
    if (irq < 16 && irq_handlers[irq])
        irq_handlers[irq]();
}

/* ── Common stubs in asm ───────────────────────────────────────────── */
__asm__(
"isr_common:\n"
"  pusha\n"
"  mov %ds, %ax\n"
"  push %eax\n"
"  mov $0x10, %ax\n"
"  mov %ax, %ds\n"
"  mov %ax, %es\n"
"  mov %ax, %fs\n"
"  mov %ax, %gs\n"
"  push %esp\n"
"  call _isr_handler\n"
"  add $4, %esp\n"
"  pop %eax\n"
"  mov %ax, %ds\n"
"  mov %ax, %es\n"
"  mov %ax, %fs\n"
"  mov %ax, %gs\n"
"  popa\n"
"  add $8, %esp\n"
"  iret\n"

"irq_common:\n"
"  pusha\n"
"  mov %ds, %ax\n"
"  push %eax\n"
"  mov $0x10, %ax\n"
"  mov %ax, %ds\n"
"  mov %ax, %es\n"
"  mov %ax, %fs\n"
"  mov %ax, %gs\n"
"  push %esp\n"
"  call _irq_handler\n"
"  add $4, %esp\n"
"  pop %eax\n"
"  mov %ax, %ds\n"
"  mov %ax, %es\n"
"  mov %ax, %fs\n"
"  mov %ax, %gs\n"
"  popa\n"
"  add $8, %esp\n"
"  iret\n"
);

/* ── PIC init ──────────────────────────────────────────────────────── */
void pic_init(void) {
    /* ICW1 */
    outb(PIC1_CMD, 0x11); io_wait();
    outb(PIC2_CMD, 0x11); io_wait();
    /* ICW2: remap IRQ 0-7 → 32-39, IRQ 8-15 → 40-47 */
    outb(PIC1_DATA, 0x20); io_wait();
    outb(PIC2_DATA, 0x28); io_wait();
    /* ICW3 */
    outb(PIC1_DATA, 0x04); io_wait();
    outb(PIC2_DATA, 0x02); io_wait();
    /* ICW4 */
    outb(PIC1_DATA, 0x01); io_wait();
    outb(PIC2_DATA, 0x01); io_wait();
    /* Mask all except IRQ0 (timer) and IRQ1 (keyboard) */
    outb(PIC1_DATA, 0xFC);
    outb(PIC2_DATA, 0xFF);
}

/* ── IDT init ──────────────────────────────────────────────────────── */
void idt_init(void) {
    kmemset(idt, 0, sizeof(idt));
    kmemset(irq_handlers, 0, sizeof(irq_handlers));

#define SET(n, fn) idt_set_gate(n, (uint32_t)(fn), 0x08, 0x8E)
    SET(0,isr0);  SET(1,isr1);  SET(2,isr2);  SET(3,isr3);
    SET(4,isr4);  SET(5,isr5);  SET(6,isr6);  SET(7,isr7);
    SET(8,isr8);  SET(9,isr9);  SET(10,isr10); SET(11,isr11);
    SET(12,isr12); SET(13,isr13); SET(14,isr14); SET(15,isr15);
    SET(16,isr16); SET(17,isr17); SET(18,isr18); SET(19,isr19);
    SET(20,isr20); SET(21,isr21); SET(22,isr22); SET(23,isr23);
    SET(24,isr24); SET(25,isr25); SET(26,isr26); SET(27,isr27);
    SET(28,isr28); SET(29,isr29); SET(30,isr30); SET(31,isr31);
    SET(32,irq0);  SET(33,irq1);  SET(34,irq2);  SET(35,irq3);
    SET(36,irq4);  SET(37,irq5);  SET(38,irq6);  SET(39,irq7);
    SET(40,irq8);  SET(41,irq9);  SET(42,irq10); SET(43,irq11);
    SET(44,irq12); SET(45,irq13); SET(46,irq14); SET(47,irq15);
#undef SET

    idt_ptr.limit = (uint16_t)(sizeof(idt) - 1);
    idt_ptr.base  = (uint32_t)&idt;
    __asm__ volatile("lidt (%0)" :: "r"(&idt_ptr));
}

void irq_install_handler(int irq, void (*handler)(void)) {
    if (irq >= 0 && irq < 16) irq_handlers[irq] = handler;
}

void sti(void) { __asm__ volatile("sti"); }
void cli(void) { __asm__ volatile("cli"); }
