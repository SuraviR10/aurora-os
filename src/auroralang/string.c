#include "string.h"
#include <stdint.h>

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    char *d = dest;
    while (n && (*d++ = *src++)) n--;
    while (n--) *d++ = '\0';
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

char *strchr(const char *s, int c) {
    while (*s != (char)c) {
        if (!*s++) return NULL;
    }
    return (char *)s;
}

size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n-- > 0 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    if (n == (size_t)-1) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

char *strstr(const char *haystack, const char *needle) {
    size_t len = strlen(needle);
    if (len == 0) return (char*)haystack;
    while (*haystack) {
        if (strncmp(haystack, needle, len) == 0) {
            return (char*)haystack;
        }
        haystack++;
    }
    return NULL;
}

void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *p = a, *q = b;
    while (n--) { if (*p != *q) return *p - *q; p++; q++; }
    return 0;
}

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = dst; const uint8_t *s = src;
    while (n--) *d++ = *s++;
    return dst;
}

void itoa(int n, char *buf, int base) {
    static const char digits[] = "0123456789abcdef";
    char tmp[32]; int i = 0, neg = 0;
    if (n < 0 && base == 10) { neg = 1; n = -n; }
    if (n == 0) { buf[0]='0'; buf[1]='\0'; return; }
    unsigned un = (unsigned)n;
    while (un) { tmp[i++] = digits[un % (unsigned)base]; un /= (unsigned)base; }
    if (neg) tmp[i++] = '-';
    int j = 0;
    while (i--) buf[j++] = tmp[i];
    buf[j] = '\0';
}

int atoi(const char *str) {
    int res = 0; int sign = 1;
    if (*str == '-') { sign = -1; str++; }
    while (*str >= '0' && *str <= '9') { res = res * 10 + (*str - '0'); str++; }
    return res * sign;
}

char *strtok(char *str, const char *delim, char **saveptr) {
    char *s = str ? str : *saveptr;
    if (!s) return 0;
    while (*s && strchr(delim, *s)) s++;
    if (!*s) { *saveptr = 0; return 0; }
    char *start = s;
    while (*s && !strchr(delim, *s)) s++;
    if (*s) { *s++ = '\0'; }
    *saveptr = s;
    return start;
}

int ksnprintf(char *buf, size_t n, const char *fmt, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    size_t pos = 0;
    char tmp[32];
#define EMIT(c) do { if (pos+1 < n) buf[pos++] = (c); } while(0)
    for (; *fmt; fmt++) {
        if (*fmt != '%') { EMIT(*fmt); continue; }
        fmt++;
        switch (*fmt) {
        case 's': { const char *s = __builtin_va_arg(ap, const char*);
                    if (!s) s = "(null)";
                    while (*s && pos+1 < n) buf[pos++] = *s++;
                    break; }
        case 'd': { int v = __builtin_va_arg(ap, int);
                    itoa(v, tmp, 10);
                    for (char *t = tmp; *t && pos+1 < n; t++) buf[pos++] = *t;
                    break; }
        case 'u': { unsigned v = __builtin_va_arg(ap, unsigned);
                    itoa((int)v, tmp, 10);
                    for (char *t = tmp; *t && pos+1 < n; t++) buf[pos++] = *t;
                    break; }
        case 'x': { unsigned v = __builtin_va_arg(ap, unsigned);
                    itoa((int)v, tmp, 16);
                    for (char *t = tmp; *t && pos+1 < n; t++) buf[pos++] = *t;
                    break; }
        case 'c': { char c = (char)__builtin_va_arg(ap, int); EMIT(c); break; }
        case '%': EMIT('%'); break;
        }
    }
#undef EMIT
    buf[pos] = '\0';
    __builtin_va_end(ap);
    return (int)pos;
}