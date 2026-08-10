#ifndef RCC_STRING_H
#define RCC_STRING_H

#include <stddef.h>

void* memcpy(void* destination, const void* source, size_t length);
void* memset(void* destination, int value, size_t length);
size_t strlen(const char* value);
int memcmp(const void* first, const void* second, size_t length);

#endif
