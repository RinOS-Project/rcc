#if defined(__x86_64__)
using verified_size_t = unsigned long long;
#else
using verified_size_t = unsigned int;
#endif

static unsigned char verified_cxx_object_array[16];

extern "C" verified_size_t verified_cxx_object_size_array()
{
    return __builtin_object_size(verified_cxx_object_array, 0);
}

extern "C" verified_size_t verified_cxx_object_size_string()
{
    return __builtin_object_size("RinOS", 0);
}

extern "C" verified_size_t verified_cxx_object_strlen()
{
    return __builtin_strlen("RinOS");
}
