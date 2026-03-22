#include "kernel.h"

/* ── Multiboot info structure (partial) ────────────────────────────── */
#define MULTIBOOT_MAGIC_VALUE 0x2BADB002

typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    /* ... more fields we don't need right now */
} multiboot_info_t;

/* ── Boot banner ───────────────────────────────────────────────────── */
static void print_banner(void) {
    term_setcolor(VGA_COLOR(VGA_LIGHT_CYAN, VGA_BLACK));
    term_writeln("  ___                           ___  ____  ");
    term_writeln(" / _ \\ _   _ _ __ ___  _ __ __ _/ _ \\/ ___| ");
    term_writeln("| | | | | | | '__/ _ \\| '__/ _` | | | \\___ \\ ");
    term_writeln("| |_| | |_| | | | (_) | | | (_| | |_| |___) |");
    term_writeln(" \\___/ \\__,_|_|  \\___/|_|  \\__,_|\\___/|____/ ");
    term_setcolor(VGA_COLOR(VGA_LIGHT_GREEN, VGA_BLACK));
    term_writeln("  Research-Level Operating System  v2.0");
    term_writeln("  Activity-Centric | Semantic FS | AuroraLang");
    term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
    term_writeln("─────────────────────────────────────────────────────────────────────────────────");
}

static void boot_step(const char *msg) {
    term_setcolor(VGA_COLOR(VGA_LIGHT_GREEN, VGA_BLACK));
    term_write("  [ OK ] ");
    term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
    term_writeln(msg);
    timeline_record("kernel", msg);
}

/* ── Kernel entry point ────────────────────────────────────────────── */
void kernel_main(uint32_t magic, void *mbi) {
    /* 1. Terminal first so we can print */
    term_init();
    term_clear();
    print_banner();

    /* 2. Validate multiboot */
    uint32_t mem_upper_kb = 64 * 1024; /* default 64 MB */
    if (magic == MULTIBOOT_MAGIC_VALUE && mbi) {
        multiboot_info_t *info = (multiboot_info_t *)mbi;
        if (info->flags & 0x1)
            mem_upper_kb = info->mem_upper;
    }

    /* 3. Core subsystems */
    timeline_init();
    boot_step("Timeline subsystem initialized");

    mem_init(mem_upper_kb);
    boot_step("Memory manager initialized");

    idt_init();
    pic_init();
    boot_step("Interrupt descriptor table loaded");

    timer_init(100);   /* 100 Hz tick */
    boot_step("System timer started (100 Hz)");

    keyboard_init();
    boot_step("PS/2 keyboard driver loaded");

    /* 4. File system */
    vfs_init();
    boot_step("Semantic virtual file system mounted");

    /* 5. Scheduler */
    sched_init();
    boot_step("Process scheduler initialized");

    /* 6. Activity workspace */
    activity_init();
    boot_step("Activity workspace engine ready");

    /* 7. Settings */
    settings_init();
    boot_step("System settings loaded");

    packages_init();
    boot_step("Package manager initialized");

    /* 8. AuroraLang runtime */
    aurora_runtime_init();
    boot_step("AuroraLang runtime initialized");

    /* 9. Enable interrupts */
    sti();
    boot_step("Interrupts enabled");

    term_setcolor(VGA_COLOR(VGA_LIGHT_CYAN, VGA_BLACK));
    term_writeln("\n  AuroraOS is ready. Type 'help' to explore.\n");
    term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));

    timeline_record("kernel", "Boot complete — entering shell");

    /* 10. Hand off to shell */
    shell_main();

    /* Should never return */
    for (;;) __asm__ volatile("hlt");
}
