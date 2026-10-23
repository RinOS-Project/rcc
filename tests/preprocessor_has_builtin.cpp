#if !defined(__has_builtin)
#error RCC must expose the __has_builtin preprocessing operator
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

#if !__has_builtin(__is_trivially_destructible)
#error RCC must report implemented C++ type-trait builtins
#endif

#if !__has_builtin(__is_trivial)
#error RCC must report the implemented triviality type-trait builtin
#endif

#if !__has_builtin(__has_virtual_destructor)
#error RCC must report implemented virtual-destructor type-trait builtins
#endif

#if !__has_builtin(__is_empty)
#error RCC must report implemented class-property type-trait builtins
#endif

#if !__has_builtin(__underlying_type) || \
    !__has_builtin(__is_aggregate) || !__has_builtin(__is_base_of) || \
    !__has_builtin(__is_trivially_copyable) || !__has_builtin(__is_enum) || \
    !__has_builtin(__is_class) || !__has_builtin(__is_union) || \
    !__has_builtin(__is_final) || !__has_builtin(__is_abstract) || \
    !__has_builtin(__is_polymorphic)
#error RCC must report every implemented C++ class type-trait builtin
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

int preprocessor_has_builtin_cxx() {
    return 0;
}
