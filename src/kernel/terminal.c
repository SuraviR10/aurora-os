#include "kernel.h"

/* ═══════════════════════════════════════════════════════════════════
   TERMINAL  (VGA text-mode 80×25)
   ═══════════════════════════════════════════════════════════════════ */

static volatile uint16_t *vga   = VGA_BASE;
static int  term_col   = 0;
static int  term_row   = 0;
static uint8_t term_color = 0;

/* VGA cursor via I/O ports */
static void vga_cursor_update(void) {
    uint16_t pos = (uint16_t)(term_row * VGA_COLS + term_col);
    outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void term_init(void) {
    term_color = VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK);
    term_col = term_row = 0;
    /* Enable cursor (scanlines 14-15) */
    outb(0x3D4, 0x0A); outb(0x3D5, (inb(0x3D5) & 0xC0) | 14);
    outb(0x3D4, 0x0B); outb(0x3D5, (inb(0x3D5) & 0xE0) | 15);
}

void term_clear(void) {
    uint16_t blank = (uint16_t)(' ' | ((uint16_t)term_color << 8));
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++) vga[i] = blank;
    term_col = term_row = 0;
    vga_cursor_update();
}

void term_setcolor(uint8_t c) { term_color = c; }

int term_get_row(void) { return term_row; }

void term_set_cursor(int x, int y) {
    term_col = x; term_row = y;
    vga_cursor_update();
}

static void term_scroll(void) {
    uint16_t blank = (uint16_t)(' ' | ((uint16_t)term_color << 8));
    for (int r = 0; r < VGA_ROWS - 1; r++)
        for (int c = 0; c < VGA_COLS; c++)
            vga[r * VGA_COLS + c] = vga[(r+1) * VGA_COLS + c];
    for (int c = 0; c < VGA_COLS; c++)
        vga[(VGA_ROWS-1) * VGA_COLS + c] = blank;
    term_row = VGA_ROWS - 1;
}

void term_putchar(char ch) {
    if (ch == '\n') {
        term_col = 0;
        if (++term_row >= VGA_ROWS) term_scroll();
    } else if (ch == '\r') {
        term_col = 0;
    } else if (ch == '\b') {
        if (term_col > 0) {
            term_col--;
            vga[term_row * VGA_COLS + term_col] =
                (uint16_t)(' ' | ((uint16_t)term_color << 8));
        }
    } else if (ch == '\t') {
        term_col = (term_col + 8) & ~7;
        if (term_col >= VGA_COLS) { term_col = 0; if (++term_row >= VGA_ROWS) term_scroll(); }
    } else {
        vga[term_row * VGA_COLS + term_col] =
            (uint16_t)((uint8_t)ch | ((uint16_t)term_color << 8));
        if (++term_col >= VGA_COLS) {
            term_col = 0;
            if (++term_row >= VGA_ROWS) term_scroll();
        }
    }
    vga_cursor_update();
}

void term_write(const char *s) {
    while (*s) term_putchar(*s++);
}

void term_writeln(const char *s) {
    term_write(s);
    term_putchar('\n');
}

/* Minimal printf: %s %d %u %x %c %% */
void term_printf(const char *fmt, ...) {
    /* va_list via __builtin_va_list */
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    char buf[32];
    for (; *fmt; fmt++) {
        if (*fmt != '%') { term_putchar(*fmt); continue; }
        fmt++;
        switch (*fmt) {
        case 's': { const char *s = __builtin_va_arg(ap, const char*);
                    if (!s) s = "(null)";
                    term_write(s); break; }
        case 'd': { int v = __builtin_va_arg(ap, int);
                    kitoa(v, buf, 10); term_write(buf); break; }
        case 'u': { unsigned v = __builtin_va_arg(ap, unsigned);
                    kitoa((int)v, buf, 10); term_write(buf); break; }
        case 'x': { unsigned v = __builtin_va_arg(ap, unsigned);
                    kitoa((int)v, buf, 16); term_write(buf); break; }
        case 'c': { char c = (char)__builtin_va_arg(ap, int);
                    term_putchar(c); break; }
        case '%': term_putchar('%'); break;
        default:  term_putchar('%'); term_putchar(*fmt); break;
        }
    }
    __builtin_va_end(ap);
}
