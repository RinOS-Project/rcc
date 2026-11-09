#include <stdckdint.h>

#if !defined(__STDC_VERSION_STDCKDINT_H__) || \
    __STDC_VERSION_STDCKDINT_H__ != 202311L
#error "stdckdint.h must publish its C23 header version"
#endif
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L
#error "checked arithmetic tests require C23"
#endif

#define TEST_SIGNED(type, maximum, error) do { \
    type result = 0; \
    type max_value = (type)(maximum); \
    type min_value = (type)(-(long long)(maximum) - 1LL); \
    if (!ckd_add(&result, max_value, (type)1) || result != min_value) \
        return (error); \
    if (!ckd_sub(&result, min_value, (type)1) || result != max_value) \
        return (error) + 1; \
    if (!ckd_mul(&result, max_value, (type)2) || result != (type)-2) \
        return (error) + 2; \
    if (ckd_add(&result, (type)20, (type)22) || result != (type)42) \
        return (error) + 3; \
    if (ckd_sub(&result, (type)42, (type)20) || result != (type)22) \
        return (error) + 4; \
    if (ckd_mul(&result, (type)-6, (type)7) || result != (type)-42) \
        return (error) + 5; \
} while (0)

#define TEST_UNSIGNED(type, error) do { \
    type result = 0; \
    type max_value = (type)~(type)0; \
    if (!ckd_add(&result, max_value, (type)1) || result != (type)0) \
        return (error); \
    if (!ckd_sub(&result, (type)0, (type)1) || result != max_value) \
        return (error) + 1; \
    if (!ckd_mul(&result, max_value, (type)2) || \
        result != (type)(max_value - (type)1)) \
        return (error) + 2; \
    if (ckd_add(&result, (type)20, (type)22) || result != (type)42) \
        return (error) + 3; \
    if (ckd_sub(&result, (type)42, (type)20) || result != (type)22) \
        return (error) + 4; \
    if (ckd_mul(&result, (type)6, (type)7) || result != (type)42) \
        return (error) + 5; \
} while (0)

int main(void)
{
    TEST_SIGNED(signed char, __SCHAR_MAX__, 10);
    TEST_UNSIGNED(unsigned char, 20);
    TEST_SIGNED(short, __SHRT_MAX__, 30);
    TEST_UNSIGNED(unsigned short, 40);
    TEST_SIGNED(int, __INT_MAX__, 50);
    TEST_UNSIGNED(unsigned int, 60);
    TEST_SIGNED(long, __LONG_MAX__, 70);
    TEST_UNSIGNED(unsigned long, 80);
    TEST_SIGNED(long long, __LONG_LONG_MAX__, 90);
    TEST_UNSIGNED(unsigned long long, 100);
    {
        volatile int result = 0;
        if (ckd_add(&result, 20, 22) || result != 42) return 110;
    }
    return 0;
}
