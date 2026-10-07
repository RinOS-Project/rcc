#include <stdarg.h>

unsigned long long verified_wide_scalar_constant_return(void)
{
    return ((unsigned long long)0x11223344ULL << 32) |
        0x55667788ULL;
}

unsigned long long verified_wide_scalar_parameter(
    unsigned long long value)
{
    return value;
}

unsigned long long verified_wide_scalar_const_parameter(
    const unsigned long long value)
{
    return value;
}

unsigned long long verified_wide_scalar_add(unsigned long long value)
{
    return value + 0x0102030405060708ULL;
}

unsigned long long verified_wide_scalar_carry(unsigned long long value)
{
    return value + 0xffffffffULL;
}

unsigned long long verified_wide_scalar_subtract(unsigned long long value)
{
    return value - 1ULL;
}

unsigned long long verified_wide_scalar_local(unsigned long long value)
{
    unsigned long long copy = value;
    return copy;
}

unsigned long long verified_wide_scalar_const_local(unsigned long long value)
{
    const unsigned long long copy = value;
    return copy;
}

unsigned long long verified_wide_scalar_narrow(unsigned int value)
{
    return (unsigned long long)value;
}

int verified_wide_scalar_equal(unsigned long long value)
{
    return value == 0x1122334455667788ULL;
}

int verified_wide_scalar_not_equal(unsigned long long value)
{
    return value != 0x1122334455667788ULL;
}

int verified_wide_scalar_unsigned_less(unsigned long long value)
{
    return value < 0x0000000100000000ULL;
}

int verified_wide_scalar_signed_less(long long value)
{
    return value < 0;
}

int verified_wide_scalar_unsigned_le(unsigned long long value)
{
    return value <= 0x0000000100000000ULL;
}

int verified_wide_scalar_unsigned_ge(unsigned long long value)
{
    return value >= 0x0000000100000000ULL;
}

int verified_wide_scalar_signed_le(long long value)
{
    return value <= 0;
}

int verified_wide_scalar_signed_ge(long long value)
{
    return value >= 0;
}

unsigned long long verified_wide_scalar_lshift(
    unsigned long long value, unsigned int count)
{
    return value << count;
}

unsigned long long verified_wide_scalar_lshr(
    unsigned long long value, unsigned int count)
{
    return value >> count;
}

long long verified_wide_scalar_ashr(long long value, unsigned int count)
{
    return value >> count;
}

unsigned long long verified_wide_scalar_conditional(int condition)
{
    return condition ? 0x1122334455667788ULL : 0x8877665544332211ULL;
}

unsigned long long verified_wide_scalar_conditional_assign(
    int condition, unsigned long long value)
{
    unsigned long long local = 0ULL;
    local = (condition && value != 0ULL) ? value + 1ULL : 7ULL;
    return local;
}

unsigned long long verified_wide_scalar_conditional_compound(
    int condition, unsigned long long value)
{
    unsigned long long local = value;
    local += condition ? 1ULL : 2ULL;
    return local;
}

unsigned long long verified_wide_scalar_pure_comma_compound(
    int condition, unsigned long long value)
{
    unsigned long long local = value;
    local += (value, condition ? 1ULL : 2ULL);
    return local;
}

unsigned long long verified_wide_scalar_size_align_compound(
    unsigned long long value)
{
    unsigned long long local = value;
    local += sizeof(char) + _Alignof(char);
    return local;
}

unsigned long long verified_wide_scalar_truth_conditional(
    unsigned long long value)
{
    return value ? 0x1122334455667788ULL : 0x8877665544332211ULL;
}

unsigned long long verified_wide_scalar_mul(unsigned long long value)
{
    return value * 0x0000000100000001ULL;
}

unsigned long long verified_wide_scalar_identity(unsigned long long value)
{
    return value;
}

unsigned long long verified_wide_scalar_call(unsigned long long value)
{
    return verified_wide_scalar_identity(value);
}

unsigned long long verified_wide_scalar_call_local(unsigned long long value)
{
    unsigned long long local = verified_wide_scalar_identity(value);
    local += verified_wide_scalar_identity(value);
    return local;
}

typedef unsigned long long (*VerifiedWideScalarUnary)(
    unsigned long long);

unsigned long long verified_wide_scalar_indirect_call(
    VerifiedWideScalarUnary function, unsigned long long value)
{
    return function(value);
}

unsigned long long verified_wide_scalar_variadic_target(int increment, ...)
{
    va_list arguments;
    unsigned long long value;
    va_start(arguments, increment);
    value = va_arg(arguments, unsigned long long);
    va_end(arguments);
    return value + (unsigned int)increment;
}

unsigned long long verified_wide_scalar_variadic_call(
    unsigned long long value)
{
    return verified_wide_scalar_variadic_target(5, value);
}

long long verified_wide_scalar_expect(long long value)
{
    return __builtin_expect(value, 1LL);
}

unsigned long long verified_wide_scalar_assignment(
    unsigned long long value)
{
    unsigned long long target = 0ULL;
    target = value;
    return target;
}

unsigned long long verified_wide_scalar_compound(
    unsigned long long value)
{
    unsigned long long target = value;
    target += 0x0000000100000001ULL;
    target ^= 0x00000000ffffffffULL;
    return target;
}

unsigned long long verified_wide_scalar_postincrement(
    unsigned long long value)
{
    unsigned long long target = value;
    return target++;
}

int verified_wide_scalar_logical_not(unsigned long long value)
{
    return !value;
}

int verified_wide_scalar_logical_and(unsigned long long value)
{
    return value && 7;
}

int verified_wide_scalar_logical_or(unsigned long long value)
{
    return value || 0;
}

unsigned long long verified_wide_scalar_comma(unsigned long long value)
{
    unsigned long long target = 0ULL;
    return (target = 1ULL, value);
}

unsigned long long verified_wide_scalar_udiv(unsigned long long value)
{
    return value / 0x0000000100000001ULL;
}

unsigned long long verified_wide_scalar_udiv_small(unsigned long long value)
{
    return value / 3ULL;
}

unsigned long long verified_wide_scalar_umod(unsigned long long value)
{
    return value % 0x0000000100000001ULL;
}

long long verified_wide_scalar_sdiv(long long value)
{
    return value / 3LL;
}

long long verified_wide_scalar_smod(long long value)
{
    return value % 3LL;
}

unsigned long long verified_wide_scalar_branch_assign(
    int condition, unsigned long long value)
{
    unsigned long long local = value;
    if (condition) {
        local += 0x0000000100000001ULL;
    } else {
        local -= 1ULL;
    }
    return local;
}

unsigned long long verified_wide_scalar_branch_read(
    int condition, unsigned long long value)
{
    unsigned long long local = value;
    if (condition) {
        local += 5ULL;
    }
    return local + 7ULL;
}

unsigned long long verified_wide_scalar_forward_goto(
    int condition, unsigned long long value)
{
    unsigned long long local = value;
    if (condition) {
        local += 3ULL;
        goto join;
    }
    local += 5ULL;
join:
    local += 7ULL;
    return local;
}

unsigned long long verified_wide_scalar_backward_goto(
    unsigned int count, unsigned long long value)
{
    unsigned long long local = value;
again:
    if (count == 0u) goto done;
    local += 0x0000000100000001ULL;
    --count;
    goto again;
done:
    return local;
}

unsigned long long verified_wide_scalar_while_loop(
    unsigned long long value, unsigned int count)
{
    unsigned long long local = value;
    while (count != 0u) {
        local += 0x0000000100000001ULL;
        --count;
    }
    return local;
}

unsigned long long verified_wide_scalar_for_loop(
    unsigned long long value, unsigned int count)
{
    unsigned long long local = value;
    for (unsigned int index = 0u; index < count; ++index) {
        local += 0x0000000100000001ULL;
    }
    return local;
}

unsigned long long verified_wide_scalar_do_loop(
    unsigned long long value, unsigned int count)
{
    unsigned long long local = value;
    do {
        local += 0x0000000100000001ULL;
        --count;
    } while (count != 0u);
    return local;
}

unsigned long long verified_wide_scalar_nested_while(
    unsigned long long value, unsigned int count)
{
    unsigned long long local = value;
    while (count != 0u) {
        unsigned int inner = 2u;
        while (inner != 0u) {
            local += 1ULL;
            --inner;
        }
        --count;
    }
    return local;
}

unsigned long long verified_wide_scalar_nested_for(
    unsigned long long value, unsigned int count)
{
    unsigned long long local = value;
    while (count != 0u) {
        for (unsigned int inner = 0u; inner < 2u; ++inner) {
            local += 2ULL;
        }
        --count;
    }
    return local;
}

unsigned long long verified_wide_scalar_nested_do(
    unsigned long long value, unsigned int count)
{
    unsigned long long local = value;
    while (count != 0u) {
        unsigned int inner = 2u;
        do {
            local += 1ULL;
            --inner;
        } while (inner != 0u);
        --count;
    }
    return local;
}

unsigned long long verified_wide_scalar_switch(
    unsigned int selector, unsigned long long value)
{
    unsigned long long local = value;
    switch (selector) {
        case 0u:
            local += 1ULL;
            break;
        case 1u:
            local += 2ULL;
            break;
        default:
            local += 3ULL;
            break;
    }
    return local;
}

unsigned long long verified_wide_scalar_switch_loop(
    unsigned long long value, unsigned int count)
{
    unsigned long long local = value;
    while (count != 0u) {
        switch (count) {
            case 1u:
                local += 1ULL;
                break;
            default:
                local += 2ULL;
                break;
        }
        --count;
    }
    return local;
}

unsigned long long verified_wide_scalar_switch_fallthrough(
    unsigned long long value, unsigned int selector)
{
    unsigned long long local = value;
    switch (selector) {
        case 3u:
            local += 1ULL;
        case 2u:
            local += 2ULL;
            break;
        default:
            local += 4ULL;
            break;
    }
    return local;
}

unsigned long long verified_wide_scalar_switch_if(
    unsigned long long value, unsigned int selector)
{
    unsigned long long local = value;
    switch (selector) {
        case 0u:
            if (selector == 0u) {
                local += 1ULL;
            } else {
                local += 2ULL;
            }
            break;
        default:
            local += 3ULL;
            break;
    }
    return local;
}

unsigned long long verified_wide_scalar_switch_no_default(
    unsigned long long value, unsigned int selector)
{
    unsigned long long local = value;
    switch (selector) {
        case 0u:
            local += 1ULL;
            break;
        case 1u:
            local += 2ULL;
            break;
    }
    return local;
}

unsigned long long verified_wide_scalar_while_continue(
    unsigned long long value, unsigned int count)
{
    unsigned long long local = value;
    while (count != 0u) {
        --count;
        if (count == 2u) {
            continue;
        }
        local += 0x0000000100000001ULL;
    }
    return local;
}

unsigned long long verified_wide_scalar_for_break(
    unsigned long long value, unsigned int count)
{
    unsigned long long local = value;
    for (unsigned int index = 0u; index < count; ++index) {
        if (index == 2u) {
            break;
        }
        local += 0x0000000100000001ULL;
    }
    return local;
}

unsigned long long verified_wide_scalar_do_control(
    unsigned long long value, unsigned int count)
{
    unsigned long long local = value;
    do {
        --count;
        if (count == 2u) {
            continue;
        }
        if (count == 1u) {
            break;
        }
        local += 0x0000000100000001ULL;
    } while (count != 0u);
    return local;
}

unsigned long long verified_wide_scalar_loop_return(
    unsigned long long value, unsigned int count)
{
    unsigned long long local = value;
    while (count != 0u) {
        if (count == 2u) {
            return local;
        }
        local += 0x0000000100000001ULL;
        --count;
    }
    return local;
}
