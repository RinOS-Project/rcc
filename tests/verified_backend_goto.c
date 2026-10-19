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

long long verified_goto_wide_while_edges(long long limit)
{
    long long value = 0;
    unsigned int iterations = 0u;
    while (iterations < 4u && value < limit) {
        ++iterations;
        if (iterations == 1u) {
            value += 2LL;
            goto joined;
        }
        value += 1LL;

    joined:
        value += 1LL;
        if (iterations == 2u) continue;
        if (value >= limit) break;
        value += 3LL;
    }
    return value;
}

long long verified_goto_wide_for_edges(long long limit)
{
    long long value = 0;
    for (unsigned int iterations = 0u;
         iterations < 4u && value < limit; ++iterations) {
        if (iterations == 0u) {
            value += 2LL;
            goto joined;
        }
        value += 1LL;

    joined:
        value += 1LL;
        if (iterations == 1u) continue;
        if (value >= limit) break;
        value += 3LL;
    }
    return value;
}

long long verified_goto_wide_do_edges(long long limit)
{
    long long value = 0;
    unsigned int iterations = 0u;
    do {
        ++iterations;
        if (iterations == 1u) {
            value += 2LL;
            goto joined;
        }
        value += 1LL;

    joined:
        value += 1LL;
        if (iterations == 2u) continue;
        if (value >= limit) break;
        value += 3LL;
    } while (iterations < 4u && value < limit);
    return value;
}

int verified_goto_vla_backward(int count, int rounds)
{
    int round = 0;
    int total = 0;

again:
    if (round >= rounds) return total;
    {
        int values[count];
        values[0] = round + 1;
        total += values[0];
        ++round;
        goto again;
    }
}

int verified_goto_vla_scope_exit(int count)
{
    int value = 0;
    {
        int values[count];
        values[0] = 23;
        value = values[0];
        goto done;
    }
done:
    return value;
}

int verified_goto_vla_nested_exit(int count)
{
    int value = 0;
    {
        int outer[count];
        outer[0] = 31;
        {
            int inner[count + 1];
            inner[0] = 99;
            goto keep_outer;
        }
keep_outer:
        value = outer[0];
        {
            int inner[count + 2];
            inner[0] = 101;
            goto done;
        }
    }
done:
    return value;
}

int verified_goto_vla_fixed_inner_arrays(int count)
{
    int value = 0;
    {
        int values[count][2][3];
        values[0][1][2] = 5;
        values[count - 1][1][2] = 17;
        value = values[0][1][2] + values[count - 1][1][2];
    }
    return value;
}

int verified_goto_vla_multiple_runtime_dimensions(int rows, int cols)
{
    int values[rows][cols];
    values[1][2] = 73;
    cols = 1;
    return values[1][2];
}

int verified_goto_vla_continue(int count, int rounds)
{
    int total = 0;
    for (int round = 0; round < rounds; ++round) {
        int values[count];
        values[0] = round + 1;
        if (round == 0) continue;
        total += values[0];
    }
    return total;
}

int verified_goto_vla_break(int count, int rounds)
{
    int total = 0;
    for (int round = 0; round < rounds; ++round) {
        int values[count];
        values[0] = round + 1;
        total += values[0];
        break;
    }
    return total;
}

int verified_goto_vla_while_edges(int count)
{
    int round = 0;
    int total = 0;
    while (round < 5) {
        int values[count];
        values[0] = round + 1;
        ++round;
        if (round == 2) continue;
        if (round == 4) break;
        total += values[0];
    }
    return total;
}

int verified_goto_vla_do_edges(int count)
{
    int round = 0;
    int total = 0;
    do {
        int values[count];
        values[0] = round + 1;
        ++round;
        if (round == 2) continue;
        if (round == 4) break;
        total += values[0];
    } while (round < 5);
    return total;
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
    if (verified_goto_wide_while_edges(0LL) != 0LL) return 29;
    if (verified_goto_wide_while_edges(1LL) != 3LL) return 30;
    if (verified_goto_wide_while_edges(8LL) != 8LL) return 31;
    if (verified_goto_wide_while_edges(12LL) != 13LL) return 32;
    if (verified_goto_wide_for_edges(0LL) != 0LL) return 33;
    if (verified_goto_wide_for_edges(1LL) != 3LL) return 34;
    if (verified_goto_wide_for_edges(8LL) != 8LL) return 35;
    if (verified_goto_wide_for_edges(12LL) != 13LL) return 36;
    if (verified_goto_wide_do_edges(0LL) != 3LL) return 37;
    if (verified_goto_wide_do_edges(1LL) != 3LL) return 38;
    if (verified_goto_wide_do_edges(8LL) != 8LL) return 39;
    if (verified_goto_wide_do_edges(12LL) != 13LL) return 40;
    if (verified_goto_vla_backward(32, 5000) != 12502500) return 41;
    if (verified_goto_vla_scope_exit(4) != 23) return 42;
    if (verified_goto_vla_continue(32, 5000) != 12502499) return 43;
    if (verified_goto_vla_break(32, 5000) != 1) return 44;
    if (verified_goto_vla_nested_exit(4) != 31) return 45;
    if (verified_goto_vla_while_edges(32) != 4) return 46;
    if (verified_goto_vla_do_edges(32) != 4) return 47;
    if (verified_goto_vla_fixed_inner_arrays(4) != 22) return 48;
    if (verified_goto_vla_multiple_runtime_dimensions(4, 5) != 73) {
        return 49;
    }
    return 0;
}
