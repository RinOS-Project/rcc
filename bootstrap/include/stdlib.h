#ifndef RCC_BOOTSTRAP_STDLIB_H
#define RCC_BOOTSTRAP_STDLIB_H

#include <stddef.h>

void* malloc(size_t size);
void* calloc(size_t count, size_t size);
void* realloc(void* pointer, size_t size);
void free(void* pointer);
void abort(void);
void exit(int status);
int atexit(void (*function)(void));
int atoi(const char* text);
double strtod(const char* text, char** end);
long strtol(const char* text, char** end, int base);
unsigned long strtoul(const char* text, char** end, int base);
long long strtoll(const char* text, char** end, int base);
unsigned long long strtoull(const char* text, char** end, int base);
void qsort(void* base, size_t count, size_t size,
           int (*compare)(const void*, const void*));

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#endif
