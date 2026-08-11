#ifndef RCC_BOOTSTRAP_STDIO_H
#define RCC_BOOTSTRAP_STDIO_H

#include <stddef.h>
#include <stdarg.h>

typedef struct RCCBootstrapFile FILE;

FILE* __rin_stdin(void);
FILE* __rin_stdout(void);
FILE* __rin_stderr(void);

#define stdin (__rin_stdin())
#define stdout (__rin_stdout())
#define stderr (__rin_stderr())

#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

FILE* fopen(const char* path, const char* mode);
int fclose(FILE* stream);
size_t fread(void* buffer, size_t size, size_t count, FILE* stream);
size_t fwrite(const void* buffer, size_t size, size_t count, FILE* stream);
char* fgets(char* buffer, int size, FILE* stream);
int fseek(FILE* stream, long offset, int origin);
long ftell(FILE* stream);
int feof(FILE* stream);
int ferror(FILE* stream);
int fflush(FILE* stream);
int fputc(int character, FILE* stream);
int fputs(const char* text, FILE* stream);
int remove(const char* path);
int rename(const char* old_path, const char* new_path);
int printf(const char* format, ...);
int fprintf(FILE* stream, const char* format, ...);
int snprintf(char* buffer, size_t size, const char* format, ...);
int vsnprintf(char* buffer, size_t size, const char* format, va_list arguments);
int vfprintf(FILE* stream, const char* format, va_list arguments);
int puts(const char* text);
void perror(const char* text);

#endif
