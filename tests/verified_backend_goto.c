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

long long verified_goto_wide_nested_backward_restart(
    unsigned int outer_count, unsigned int inner_count,
    unsigned int restart_count, long long initial)
{
    long long value = initial;
    while (outer_count != 0u) {
        unsigned int inner = inner_count;
        unsigned int restarts = restart_count;

    restart_inner:
        while (inner != 0u) {
            --inner;
            value += 0x0000000100000001LL;
            if (restarts != 0u) {
                --restarts;
                inner = inner_count;
                goto restart_inner;
            }
        }
        value += (long long)outer_count;
        --outer_count;
    }
    return value;
}

long long verified_goto_wide_multiple_predecessors(unsigned int first,
                                                   unsigned int second,
                                                   long long initial)
{
    long long value = initial;
    if (first != 0u) {
        value += 0x0000000100000001LL;
        goto joined;
    }
    if (second != 0u) {
        value += 0x0000000200000002LL;
        goto joined;
    }
    value += 0x0000000300000003LL;

joined:
    return value;
}

long long verified_goto_wide_loop_condition(long long limit)
{
    long long value = 0;
    unsigned int iterations = 0u;
    while (value < limit && iterations < 4u) {
        ++iterations;
        if ((limit & 1LL) != 0LL) {
            value += 2LL;
            goto joined;
        }
        value += 1LL;

    joined:
        value += 1LL;
    }
    return value;
}

long long verified_goto_wide_for_condition(long long limit)
{
    long long value = 0;
    for (unsigned int iterations = 0u;
         value < limit && iterations < 4u; ++iterations) {
        if ((limit & 1LL) != 0LL) {
            value += 2LL;
            goto joined;
        }
        value += 1LL;

    joined:
        value += 1LL;
    }
    return value;
}

long long verified_goto_wide_do_condition(long long limit)
{
    long long value = 0;
    unsigned int iterations = 0u;
    do {
        ++iterations;
        if ((limit & 1LL) != 0LL) {
            value += 2LL;
            goto joined;
        }
        value += 1LL;

    joined:
        value += 1LL;
    } while (value < limit && iterations < 4u);
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
    if (verified_goto_wide_nested_backward_restart(2u, 2u, 2u, 7) !=
        34359738386LL) return 13;
    if (verified_goto_wide_nested_backward_restart(1u, 0u, 2u, 7) != 8) {
        return 14;
    }
    if (verified_goto_wide_multiple_predecessors(1u, 1u, 5) !=
        4294967302LL) return 10;
    if (verified_goto_wide_multiple_predecessors(0u, 1u, 7) !=
        8589934601LL) return 11;
    if (verified_goto_wide_multiple_predecessors(0u, 0u, 9) !=
        12884901900LL) return 12;
    if (verified_goto_wide_loop_condition(0LL) != 0LL) return 17;
    if (verified_goto_wide_loop_condition(3LL) != 3LL) return 18;
    if (verified_goto_wide_loop_condition(4LL) != 4LL) return 19;
    if (verified_goto_wide_loop_condition(5LL) != 6LL) return 20;
    if (verified_goto_wide_for_condition(0LL) != 0LL) return 21;
    if (verified_goto_wide_for_condition(3LL) != 3LL) return 22;
    if (verified_goto_wide_for_condition(4LL) != 4LL) return 23;
    if (verified_goto_wide_for_condition(5LL) != 6LL) return 24;
    if (verified_goto_wide_do_condition(0LL) != 2LL) return 25;
    if (verified_goto_wide_do_condition(3LL) != 3LL) return 26;
    if (verified_goto_wide_do_condition(4LL) != 4LL) return 27;
    if (verified_goto_wide_do_condition(5LL) != 6LL) return 28;
    return 0;
}
