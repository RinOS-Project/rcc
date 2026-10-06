extern "C" int verified_cxx(int value)
{
    return value + 2;
}

extern "C" int verified_cxx_indirect_target(int value)
{
    return value + 4;
}

extern "C" int verified_cxx_indirect_parameter(int (*function)(int),
                                                 int value)
{
    return function(value) + 5;
}

namespace verified_cxx_pointer_overload {
int target(int value)
{
    return value + 6;
}

long target(long value)
{
    return value + 60;
}
}

extern "C" int verified_cxx_overload_pointer_call(int value)
{
    int (*function)(int) = verified_cxx_pointer_overload::target;
    return function(value) + 7;
}

extern "C" int verified_cxx_overload_pointer_assignment(int value)
{
    int (*function)(int);
    function = verified_cxx_pointer_overload::target;
    return function(value) + 8;
}

extern "C" int verified_cxx_overload_address_of_call(int value)
{
    int (*function)(int) = &verified_cxx_pointer_overload::target;
    return function(value) + 9;
}

extern "C" unsigned long long verified_cxx_wide_scalar_conditional_assign(
    int condition, unsigned long long value)
{
    unsigned long long local = 0ULL;
    local = (condition && value != 0ULL) ? value + 1ULL : 7ULL;
    return local;
}

extern "C" unsigned long long verified_cxx_wide_scalar_conditional_compound(
    int condition, unsigned long long value)
{
    unsigned long long local = value;
    local += condition ? 1ULL : 2ULL;
    return local;
}

extern "C" unsigned long long verified_cxx_wide_scalar_pure_comma_compound(
    int condition, unsigned long long value)
{
    unsigned long long local = value;
    local += (value, condition ? 1ULL : 2ULL);
    return local;
}

extern "C" unsigned long long verified_cxx_wide_scalar_noexcept_compound(
    unsigned long long value)
{
    unsigned long long local = value;
    local += noexcept(value + 1ULL) ? 1ULL : 2ULL;
    return local;
}
