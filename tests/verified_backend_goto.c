int verified_goto_value(int selector)
{
    if (selector) goto selected;
    return 11;

selected:
    return 42;
}

long long verified_goto_wide_forward(int selector, long long initial)
{
    long long value = initial;
    if (selector) {
        value += 5;
        goto selected;
    }
    value += 9;

selected:
    return value;
}

long long verified_goto_wide_backward(int count)
{
    long long value = 0;

again:
    if (count <= 0) goto done;
    --count;
    value += 3;
    goto again;

done:
    return value;
}

long long verified_goto_wide_inside_loop(unsigned int count,
                                         long long initial)
{
    long long value = initial;
    while (count != 0u) {
        if ((count & 1u) != 0u) {
            value += 0x0000000100000001LL;
            goto joined;
        }
        value += 0x0000000200000002LL;

    joined:
        value += (long long)count;
        --count;
    }
    return value;
}

long long verified_goto_wide_nested_loop(unsigned int count,
                                         long long initial)
{
    long long value = initial;
    while (count != 0u) {
        unsigned int inner = 3u;
        while (inner != 0u) {
            if ((inner & 1u) != 0u) {
                value += 0x0000000100000001LL;
                goto joined;
            }
            value += 0x0000000200000002LL;

        joined:
            value += (long long)(count + inner);
            --inner;
        }
        value += (long long)count;
        --count;
    }
    return value;
}

long long verified_goto_wide_nested_exit(unsigned int count,
                                         long long initial)
{
    long long value = initial;
    while (count != 0u) {
        unsigned int inner = 3u;
        while (inner != 0u) {
            if ((count & 1u) != 0u && inner == 2u) goto outer_join;
            value += 0x0000000100000001LL;
            --inner;
        }

    outer_join:
        value += (long long)(count * 3u);
        --count;
    }
    return value;
}

long long verified_goto_wide_nested_backward(unsigned int outer_count,
                                             unsigned int inner_count,
                                             long long initial)
{
    long long value = initial;
    while (outer_count != 0u) {
        unsigned int remaining = inner_count;

    repeat_inner:
        if (remaining != 0u) {
            value += 0x0000000100000001LL;
            --remaining;
            if (remaining != 0u) goto repeat_inner;
        }
        value += (long long)outer_count;
        --outer_count;
    }
    return value;
}

int main(void)
{
    if (verified_goto_wide_forward(1, 100) != 105) return 1;
    if (verified_goto_wide_forward(0, 100) != 109) return 2;
    if (verified_goto_wide_backward(5) != 15) return 3;
    if (verified_goto_wide_nested_exit(3u, 11) != 21474836514LL) return 4;
    if (verified_goto_wide_nested_exit(2u, 7) != 17179869204LL) return 5;
    if (verified_goto_wide_nested_exit(0u, 7) != 7) return 6;
    if (verified_goto_wide_nested_backward(2u, 3u, 7) !=
        25769803792LL) return 7;
    if (verified_goto_wide_nested_backward(2u, 0u, 7) != 10) return 8;
    if (verified_goto_wide_nested_backward(1u, 1u, 0) !=
        4294967298LL) return 9;
    return 0;
}
