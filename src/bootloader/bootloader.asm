; =============================================================================
;  AuroraOS  —  Stage-1 Bootloader
;  Assembled:  nasm -f bin bootloader.asm -o bootloader.bin
;
;  Memory map after this runs:
;    0x0000:0x7C00  this bootloader (loaded by BIOS)
;    0x1000:0x0000  kernel binary   (we load it here = physical 0x10000)
;    0x0000:0x9000  stack top       (grows downward, safe gap below kernel)
; =============================================================================
[org  0x7C00]
[bits 16]

; ── Constants ────────────────────────────────────────────────────────────────
KERNEL_SEG    equ 0x1000     ; physical 0x10000 = seg 0x1000, offset 0
KERNEL_SECTS  equ 2880       ; 2880 × 512 = 1,474,560 bytes  (covers 1.4 MB kernel)
CODE_SEG      equ 0x08       ; GDT selector: code
DATA_SEG      equ 0x10       ; GDT selector: data

; ── Entry point ──────────────────────────────────────────────────────────────
    ; Some BIOSes jump to 0x07C0:0000 instead of 0x0000:7C00 — normalise CS:IP
    jmp  0x0000:start

start:
    cli
    xor  ax, ax
    mov  ds, ax
    mov  es, ax
    mov  ss, ax
    mov  sp, 0x7C00          ; stack grows down from 0x7C00 (safe, below bootloader)
    sti

    mov  [boot_drive], dl    ; BIOS stores boot drive in DL on entry

    mov  si, msg_boot
    call print_str

    ; ── Verify INT 13h LBA extensions are available ──────────────────────────
    mov  ah, 0x41
    mov  bx, 0x55AA
    mov  dl, [boot_drive]
    int  0x13
    jc   no_lba
    cmp  bx, 0xAA55
    jne  no_lba

    ; ── Load kernel using INT 13h AH=0x42 (LBA extended read) ───────────────
    ;   We read in chunks of 127 sectors (safe maximum per BIOS call).
    ;   Destination advances by (sectors_read × 512) bytes each iteration.
    mov  si, msg_loading
    call print_str

    mov  word [dap_lba_lo], 1    ; start at LBA 1 (sector after bootloader)
    mov  word [dap_lba_hi], 0
    mov  word [dap_seg],    KERNEL_SEG
    mov  word [dap_off],    0

    mov  cx, KERNEL_SECTS        ; total sectors left to read

.read_loop:
    test cx, cx
    jz   .read_done

    mov  ax, cx
    cmp  ax, 127
    jbe  .set_count
    mov  ax, 127
.set_count:
    mov  [dap_count], ax

    mov  ah, 0x42
    mov  dl, [boot_drive]
    mov  si, dap                 ; DS:SI → Disk Address Packet
    int  0x13
    jc   disk_error

    ; Advance LBA counter
    mov  ax, [dap_count]
    add  [dap_lba_lo], ax
    adc  word [dap_lba_hi], 0

    ; Advance destination segment (each sector = 512 bytes = 32 paragraphs)
    mov  ax, [dap_count]
    shl  ax, 5                   ; × 32 paragraphs
    add  [dap_seg], ax

    ; Decrement remaining count
    mov  ax, [dap_count]
    sub  cx, ax
    jmp  .read_loop

.read_done:
    mov  si, msg_loaded
    call print_str

    ; ── Switch to 32-bit protected mode ──────────────────────────────────────
    cli
    lgdt [gdt_descriptor]

    mov  eax, cr0
    or   eax, 1
    mov  cr0, eax

    ; Far jump: flushes CPU prefetch queue, loads CS = CODE_SEG (0x08)
    jmp  CODE_SEG:pm_entry

; ── Subroutines ──────────────────────────────────────────────────────────────
print_str:                       ; SI = pointer to null-terminated string
    pusha
    mov  ah, 0x0E
    mov  bh, 0x00
.loop:
    lodsb
    test al, al
    jz   .done
    int  0x10
    jmp  .loop
.done:
    popa
    ret

no_lba:
    mov  si, msg_no_lba
    call print_str
    jmp  halt

disk_error:
    mov  si, msg_disk_err
    call print_str
halt:
    cli
    hlt
    jmp  halt

; ── GDT ──────────────────────────────────────────────────────────────────────
align 4
gdt_start:
    dq  0x0000000000000000   ; [0x00] null descriptor

gdt_code:                    ; [0x08] ring-0 code: base=0, limit=4GB, 32-bit
    dw  0xFFFF               ; limit  [0:15]
    dw  0x0000               ; base   [0:15]
    db  0x00                 ; base   [16:23]
    db  10011010b            ; P=1 DPL=0 S=1 Type=0xA (execute/read)
    db  11001111b            ; G=1 D/B=1 (32-bit) limit [16:19]=0xF
    db  0x00                 ; base   [24:31]

gdt_data:                    ; [0x10] ring-0 data: base=0, limit=4GB
    dw  0xFFFF
    dw  0x0000
    db  0x00
    db  10010010b            ; P=1 DPL=0 S=1 Type=0x2 (read/write)
    db  11001111b
    db  0x00

gdt_end:

gdt_descriptor:
    dw  gdt_end - gdt_start - 1   ; GDT limit (size - 1)
    dd  gdt_start                  ; GDT linear base address

; ── 32-bit protected-mode stub (still inside bootloader sector) ───────────────
[bits 32]
pm_entry:
    mov  ax, DATA_SEG
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax
    mov  esp, 0x9000         ; stack top at 36 KB — below kernel at 64 KB (0x10000)

    jmp  0x10000             ; jump to kernel entry point

; ── Disk Address Packet  (MUST be inside the 512-byte sector) ────────────────
[bits 16]
dap:
    db  0x10                 ; packet size = 16 bytes
    db  0x00                 ; reserved (must be 0)
dap_count:
    dw  0                    ; number of sectors to transfer
dap_off:
    dw  0                    ; destination buffer offset
dap_seg:
    dw  0                    ; destination buffer segment
dap_lba_lo:
    dw  0                    ; LBA address bits  0–15
dap_lba_hi:
    dw  0                    ; LBA address bits 16–31
    dd  0                    ; LBA address bits 32–63 (always 0 for us)

; ── Strings and variables ─────────────────────────────────────────────────────
boot_drive   db 0
msg_boot     db "AuroraOS booting...", 13, 10, 0
msg_loading  db "Loading kernel...",   13, 10, 0
msg_loaded   db "OK. Entering protected mode...", 13, 10, 0
msg_no_lba   db "ERROR: No INT13 LBA support!", 13, 10, 0
msg_disk_err db "ERROR: Disk read failed!", 13, 10, 0

; ── Boot sector signature ─────────────────────────────────────────────────────
times 510 - ($ - $$) db 0
dw 0xAA55
