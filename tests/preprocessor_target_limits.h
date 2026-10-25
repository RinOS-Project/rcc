#if !defined(__CHAR_BIT__) || !defined(__SCHAR_MAX__) || \
    !defined(__SHRT_MAX__) || !defined(__INT_MAX__) || \
    !defined(__LONG_MAX__) || !defined(__LONG_LONG_MAX__) || \
    !defined(__SIZE_MAX__) || !defined(__PTRDIFF_MAX__) || \
    !defined(__INTPTR_MAX__) || !defined(__UINTPTR_MAX__) || \
    !defined(__INT8_MAX__) || !defined(__UINT8_MAX__) || \
    !defined(__INT16_MAX__) || !defined(__UINT16_MAX__) || \
    !defined(__INT32_MAX__) || !defined(__UINT32_MAX__) || \
    !defined(__INT64_MAX__) || !defined(__UINT64_MAX__) || \
    !defined(__INT_FAST8_MAX__) || !defined(__UINT_FAST8_MAX__) || \
    !defined(__INT_FAST16_MAX__) || !defined(__UINT_FAST16_MAX__) || \
    !defined(__INT_FAST32_MAX__) || !defined(__UINT_FAST32_MAX__) || \
    !defined(__INT_FAST64_MAX__) || !defined(__UINT_FAST64_MAX__) || \
    !defined(__WCHAR_MAX__) || !defined(__SIZEOF_SHORT__) || \
    !defined(__SIZEOF_INT__) || !defined(__SIZEOF_LONG__) || \
    !defined(__SIZEOF_LONG_LONG__) || !defined(__SIZEOF_POINTER__) || \
    !defined(__SIZEOF_SIZE_T__) || !defined(__SIZEOF_WCHAR_T__) || \
    !defined(__ORDER_LITTLE_ENDIAN__) || !defined(__ORDER_BIG_ENDIAN__) || \
    !defined(__ORDER_PDP_ENDIAN__) || !defined(__BYTE_ORDER__)
#error "RinOS target integer limits, sizes, and byte order must be predefined"
#endif

#if __CHAR_BIT__ != 8 || __SCHAR_MAX__ != 127 || __SHRT_MAX__ != 32767 || \
    __INT_MAX__ != 2147483647 || __LONG_LONG_MAX__ != 9223372036854775807LL || \
    __INT8_MAX__ != 127 || __UINT8_MAX__ != 255 || \
    __INT16_MAX__ != 32767 || __UINT16_MAX__ != 65535 || \
    __INT32_MAX__ != 2147483647 || __UINT32_MAX__ != 4294967295U || \
    __INT64_MAX__ != 9223372036854775807LL || \
    __UINT64_MAX__ != 18446744073709551615ULL || \
    __INT_FAST8_MAX__ != 127 || __UINT_FAST8_MAX__ != 255 || \
    __INT_FAST16_MAX__ != 2147483647 || \
    __UINT_FAST16_MAX__ != 4294967295U || \
    __INT_FAST32_MAX__ != 2147483647 || \
    __UINT_FAST32_MAX__ != 4294967295U || \
    __INT_FAST64_MAX__ != 9223372036854775807LL || \
    __UINT_FAST64_MAX__ != 18446744073709551615ULL || \
    __WCHAR_MAX__ != 2147483647 || __SIZEOF_SHORT__ != 2 || \
    __SIZEOF_INT__ != 4 || __SIZEOF_LONG_LONG__ != 8 || \
    __SIZEOF_WCHAR_T__ != 4 || __ORDER_LITTLE_ENDIAN__ != 1234 || \
    __ORDER_BIG_ENDIAN__ != 4321 || __ORDER_PDP_ENDIAN__ != 3412 || \
    __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "RinOS common integer limits, sizes, or byte order are incorrect"
#endif

#if defined(__i386__)
#if __LONG_MAX__ != 2147483647L || __SIZE_MAX__ != 4294967295U || \
    __PTRDIFF_MAX__ != 2147483647 || __INTPTR_MAX__ != 2147483647 || \
    __UINTPTR_MAX__ != 4294967295U || __SIZEOF_LONG__ != 4 || \
    __SIZEOF_POINTER__ != 4 || __SIZEOF_SIZE_T__ != 4
#error "i686 predefined limits must match the RinOS ILP32 ABI"
#endif
#elif defined(__x86_64__)
#if __LONG_MAX__ != 9223372036854775807L || \
    __SIZE_MAX__ != 18446744073709551615ULL || \
    __PTRDIFF_MAX__ != 9223372036854775807LL || \
    __INTPTR_MAX__ != 9223372036854775807LL || \
    __UINTPTR_MAX__ != 18446744073709551615ULL || \
    __SIZEOF_LONG__ != 8 || __SIZEOF_POINTER__ != 8 || \
    __SIZEOF_SIZE_T__ != 8
#error "x86_64 predefined limits must match the RinOS LP64 ABI"
#endif
#else
#error "RinOS predefined-limit tests require an x86 target"
#endif
