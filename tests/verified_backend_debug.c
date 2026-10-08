static int verified_debug_multi_return(int value)
{
    if (value == 0) return 3;
    if (value < 0) return -value;
    return value + 1;
}

static int verified_debug_static(int value)
{
    int local = value + 3;
    {
        int nested = local + 1;
        return nested;
    }
}

int verified_debug_entry(int value)
{
    return verified_debug_static(value) + 1;
}

int verified_debug_for_scope(int limit)
{
    int verified_outer_value = 0;
    for (int verified_loop_index = 0;
         verified_loop_index < limit; ++verified_loop_index) {
        verified_outer_value += verified_loop_index;
    }
    return verified_outer_value;
}

static int verified_debug_call(int value)
{
    return value * 3 + 1;
}

int verified_debug_preserved_registers(int value)
{
    int first = value + 1;
    int second = value + 2;
    int third = value + 3;
    int fourth = value + 4;
    int fifth = value + 5;
    int sixth = value + 6;
    int seventh = value + 7;
    int eighth = value + 8;
    int ninth = value + 9;
    int tenth = value + 10;
    int call_result = verified_debug_call(value);
    return call_result + first + second + third + fourth + fifth + sixth +
        seventh + eighth + ninth + tenth;
}

struct VerifiedDebugAligned32 {
    _Alignas(32) int value;
};

int verified_debug_overaligned_local(void)
{
    struct VerifiedDebugAligned32 aligned;
    aligned.value = 37;
    return aligned.value;
}
