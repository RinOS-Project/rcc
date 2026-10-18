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

int main(void)
{
    if (verified_goto_wide_forward(1, 100) != 105) return 1;
    if (verified_goto_wide_forward(0, 100) != 109) return 2;
    if (verified_goto_wide_backward(5) != 15) return 3;
    return 0;
}
