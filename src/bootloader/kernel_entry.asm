; =============================================================================
;  AuroraOS  —  Kernel Entry Point
;  Assembled:  nasm -f win32 kernel_entry.asm -o build/kernel_entry.o
;
;  This is the VERY FIRST code the CPU runs after the bootloader jumps to
;  0x10000.  The linker script places this object's .text section first,
;  so kernel_entry_start lands at exactly byte 0 of kernel.bin.
; =============================================================================
[bits 32]

section .text

; MinGW/COFF prepends '_' to every C symbol and every linker-script symbol.
extern  _kernel_main        ; void kernel_main(uint32_t magic, void *mbi)
extern  ___bss_start        ; defined in kernel.ld  → start of BSS
extern  ___bss_end          ; defined in kernel.ld  → end   of BSS

global  _kernel_entry_start
_kernel_entry_start:

    ; 1. Reload all data-segment registers with the PM data selector (0x10).
    ;    The bootloader already set them, but being explicit is safer.
    mov  ax, 0x10
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax

    ; 2. Set up the kernel stack.
    ;    We place it at 0x9000 (36 KB), growing downward.
    ;    The kernel binary starts at 0x10000 (64 KB), so there is a
    ;    28 KB gap — plenty of stack space before any collision.
    mov  esp, 0x9000

    ; 3. Zero the BSS section.
    ;    C assumes all uninitialised globals start at zero.
    ;    No OS loader does this for us, so we must do it ourselves.
    ;    ___bss_start and ___bss_end are provided by the linker script.
    cld                         ; make sure STOSB increments EDI
    mov  edi, ___bss_start
    mov  ecx, ___bss_end
    sub  ecx, edi               ; byte count = end - start
    xor  eax, eax
    rep  stosb                  ; memset(bss, 0, size)

    ; 4. Call kernel_main(magic=0, mbi=NULL).
    ;    Arguments are pushed right-to-left (cdecl calling convention).
    push dword 0                ; arg2: multiboot_info* mbi  = NULL
    push dword 0                ; arg1: uint32_t        magic = 0
    call _kernel_main
    add  esp, 8                 ; clean up (never actually reached)

    ; 5. kernel_main should never return.  If it does, halt the CPU.
.hang:
    cli
    hlt
    jmp  .hang
