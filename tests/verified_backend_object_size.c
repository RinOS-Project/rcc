#if defined(__x86_64__)
typedef unsigned long long verified_size_t;
#else
typedef unsigned int verified_size_t;
#endif

static unsigned char verified_object_array[16];

verified_size_t verified_object_size_array(void)
{
    return __builtin_object_size(verified_object_array, 0);
}

verified_size_t verified_object_size_array_offset(void)
{
    return __builtin_object_size(&verified_object_array[4], 0);
}

verified_size_t verified_object_size_string(void)
{
    return __builtin_object_size("RinOS", 0);
}

verified_size_t verified_object_size_unknown(const unsigned char* value)
{
    return __builtin_object_size(value, 0);
}

verified_size_t verified_object_size_unknown_zero(const unsigned char* value)
{
    return __builtin_object_size(value, 2);
}
