#ifndef RCC_BOOTSTRAP_STDARG_H
#define RCC_BOOTSTRAP_STDARG_H

typedef __builtin_va_list va_list;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define va_start(list, ...) \
    __builtin_va_start(list __VA_OPT__(,) __VA_ARGS__)
#else
#define va_start(list, last) __builtin_va_start(list, last)
#endif
#define va_end(list) __builtin_va_end(list)
#define va_copy(destination, source) \
    __builtin_va_copy(destination, source)
#define va_arg(list, type) __builtin_va_arg(list, type)

#endif
