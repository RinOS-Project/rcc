#if !defined(__has_builtin)
#error RCC must expose the __has_builtin preprocessing operator
#endif

#if !defined(__DBL_MIN__)
#error RCC must define the minimum normal double value for libc math headers
#endif

#if !__has_builtin(__builtin_expect)
#error RCC must report implemented builtins
#endif

#if !__has_builtin(__atomic_load_n)
#error RCC must report implemented atomic builtins
#endif

#if !__has_builtin(__builtin_ia32_paddb)
#error RCC must report implemented target builtins
#endif

#if __has_builtin(__builtin_bit_cast)
#error RCC must not advertise an unimplemented builtin
#endif

#if __has_builtin(__rcc_unknown_builtin_probe)
#error unknown builtin names must not be advertised
#endif

#if __has_builtin(__builtin_ia32_unknown_probe)
#error unknown target builtin names must not be advertised
#endif

int preprocessor_has_builtin_c(void) {
    return 0;
}
