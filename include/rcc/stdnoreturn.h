#ifndef RCC_STDNORETURN_H
#define RCC_STDNORETURN_H

#if defined(__cplusplus)
#define noreturn [[noreturn]]
#else
#define noreturn _Noreturn
#endif

#endif
