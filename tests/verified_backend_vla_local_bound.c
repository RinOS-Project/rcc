static int vla_array_expression_stride(void)
{
    int count = 3;
    int rows[2][count];

    count = 7;
    return (char*)(rows + 1) - (char*)rows ==
        3 * (int)sizeof(int) ? 0 : 1;
}

static int vla_pointer_declaration(void)
{
    int count = 3;
    int rows[2][count];
    int (*cursor)[count] = rows;

    return cursor == &rows[0] ? 0 : 1;
}

static int vla_pointer_scope_exit(void)
{
    int count = 3;
    int rows[2][count];
    int value = 0;

    rows[0][0] = 17;
    {
        int (*cursor)[count] = rows;
        value = cursor[0][0];
        goto done;
    }
done:
    return value == 17 ? 0 : 1;
}

static int vla_pointer_stride(void)
{
    int count = 3;
    int rows[2][count];
    int (*cursor)[count] = rows;
    int (*advanced)[count];
    int (*minus_one)[count];
    int (*commuted)[count];
    int (*compound_advanced)[count];
    _Atomic(int (*)[count]) atomic_cursor = rows;
    int (*atomic_advanced)[count];
    int (*atomic_postfix_old)[count];
    int (*atomic_postfix_next)[count];
    int (*atomic_prefix_row)[count];

    count = 7;
    advanced = cursor + 1;
    minus_one = advanced - 1;
    commuted = 1 + cursor;
    cursor += 1;
    compound_advanced = cursor;
    cursor -= 1;
    atomic_postfix_old = atomic_cursor++;
    atomic_postfix_next = atomic_cursor;
    atomic_prefix_row = --atomic_cursor;
    atomic_advanced = (atomic_cursor += 1);
    atomic_cursor -= 1;

    return count == 7 &&
        (char*)advanced - (char*)rows ==
            3 * (int)sizeof(int) &&
        minus_one == &rows[0] &&
        (char*)commuted - (char*)rows ==
            3 * (int)sizeof(int) &&
        (char*)compound_advanced - (char*)rows ==
            3 * (int)sizeof(int) &&
        cursor == &rows[0] &&
        atomic_postfix_old == &rows[0] &&
        (char*)atomic_postfix_next - (char*)rows ==
            3 * (int)sizeof(int) &&
        atomic_prefix_row == &rows[0] &&
        (char*)atomic_advanced - (char*)rows ==
            3 * (int)sizeof(int) &&
        atomic_cursor == &rows[0] ? 0 : 1;
}

static int vla_pointer_parameter_stride(int count, int (*rows)[count])
{
    int (*next)[count];
    count = 1;
    next = rows + 1;
    return rows[1][2] == 43 && next - rows == 1 && rows - next == -1
        ? 0 : 1;
}

static int verified_vla_stack_probe_boundary(unsigned int bytes)
{
    _Alignas(64) volatile unsigned char values[bytes];
    if ((unsigned long long)(void*)values % 64ull != 0ull) return 1;
    values[0] = 31u;
    values[bytes - 1u] = 73u;
    if (bytes == 1u) return values[0] == 73u ? 0 : 2;
    return values[0] == 31u && values[bytes - 1u] == 73u ? 0 : 2;
}

static int verified_fixed_stack_probe_boundaries(void)
{
    _Alignas(8192) volatile unsigned char values[16385];

    if ((unsigned long long)(void*)values % 8192ull != 0ull) return 1;
    values[0] = 19u;
    values[4096] = 20u;
    values[8192] = 21u;
    values[12288] = 22u;
    values[sizeof(values) - 1u] = 107u;
    return values[sizeof(values) - 1u] == 107u ? 0 : 2;
}

int main(void)
{
    int count = 3;
    int values[count];
    int rows[2][3];

    values[0] = 7;
    values[2] = 9;
    rows[1][2] = 43;

    if (!(count == 3 && values[0] == 7 && values[2] == 9 &&
        vla_array_expression_stride() == 0 &&
        vla_pointer_declaration() == 0 &&
        vla_pointer_scope_exit() == 0 &&
        vla_pointer_stride() == 0 &&
        vla_pointer_parameter_stride(3, rows) == 0)) return 1;
    {
        int result = verified_vla_stack_probe_boundary(1u);
        if (result != 0) return 10 + result;
    }
    {
        int result = verified_vla_stack_probe_boundary(4096u);
        if (result != 0) return 20 + result;
    }
    {
        int result = verified_vla_stack_probe_boundary(4097u);
        if (result != 0) return 30 + result;
    }
    {
        int result = verified_vla_stack_probe_boundary(16385u);
        if (result != 0) return 40 + result;
    }
    {
        int result = verified_vla_stack_probe_boundary(32769u);
        if (result != 0) return 50 + result;
    }
    {
        int result = verified_fixed_stack_probe_boundaries();
        if (result != 0) return 60 + result;
    }
    return 0;
}
