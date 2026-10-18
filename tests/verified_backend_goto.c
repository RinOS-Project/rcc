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

int main(void)
{
    if (verified_goto_wide_forward(1, 100) != 105) return 1;
    if (verified_goto_wide_forward(0, 100) != 109) return 2;
    if (verified_goto_wide_backward(5) != 15) return 3;
    return 0;
}
