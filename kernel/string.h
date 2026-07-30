#ifndef STRING_H
#define STRING_H

#include <stddef.h>
#include <stdint.h>

void  *memset(void *dst, int val, size_t len);
void  *memcpy(void *dst, const void *src, size_t len);
int    memcmp(const void *a, const void *b, size_t len);

size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strcpy(char *dst, const char *src);

/* Formats `value` in the given base (2-16) into buf (caller-owned,
 * must be big enough) and returns buf. Used by kprintf-style debug
 * output where we don't have a real printf. */
char  *itoa(int value, char *buf, int base);
char  *utoa(uint32_t value, char *buf, int base);

#endif /* STRING_H */
