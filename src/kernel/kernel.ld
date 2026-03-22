/*
 * kernel.ld  —  AuroraOS Kernel Linker Script  (MinGW i386pe)
 *
 * The bootloader loads kernel.bin to physical address 0x10000 and jumps there.
 * kernel_entry_start MUST be the very first byte of the output binary.
 *
 * How to use:
 *   ld -m i386pe -T src\kernel\kernel.ld -o build\kernel.elf \
 *      build\kernel_entry.o  [all other .o files]
 *   objcopy -O binary build\kernel.elf build\kernel.bin
 */

ENTRY(_kernel_entry_start)

SECTIONS
{
    . = 0x10000;        /* kernel load address — must match bootloader */

    /* ── Code ─────────────────────────────────────────────────────────
       kernel_entry.o's .text section is placed FIRST so that
       _kernel_entry_start lands at exactly 0x10000 (byte 0 of kernel.bin).
       All other .text sections follow immediately after.              */
    .text : {
        build\kernel_entry.o(.text)   /* entry point — MUST be first */
        *(.text)
        *(.text.*)
    }

    /* ── Read-only data ───────────────────────────────────────────── */
    .rodata : {
        *(.rodata)
        *(.rodata.*)
        *(.rdata)           /* MinGW puts string literals here */
    }

    /* ── Initialised data ─────────────────────────────────────────── */
    .data : {
        *(.data)
        *(.data.*)
    }

    /* ── BSS (zero-initialised globals) ───────────────────────────────
       kernel_entry.asm zeroes this region before calling kernel_main.
       ___bss_start / ___bss_end are exported so the assembly can use them.
       MinGW ld adds an extra '_' prefix, so the C-visible names are
       __bss_start / __bss_end, but in NASM we reference ___bss_start.  */
    .bss : {
        ___bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        ___bss_end = .;
    }

    /* ── Discard sections that are useless in a bare-metal binary ──── */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.idata*)
        *(.pdata*)
        *(.reloc*)
        *(.debug*)
    }
}
