int exercise_atomic_int_vla_rmw(int count)
{
    _Atomic int values[count];
    _Atomic signed char narrow_values[count];
    _Atomic short short_values[count];
    _Atomic unsigned char wrap_values[count];
    _Atomic unsigned int bit_values[count];
    _Atomic int divmod_values[count];
    _Atomic int shift_values[count];
    _Atomic unsigned int unsigned_shift_values[count];

    values[0] = 13;
    values[1] = 20;
    values[2] = 3;
    values[3] = 17;
    narrow_values[0] = 21;
    short_values[0] = 1500;
    wrap_values[0] = 200;
    bit_values[0] = 0x5a;
    divmod_values[0] = 101;
    shift_values[0] = 3;
    unsigned_shift_values[0] = 0x80000000u;

    int old_value = values[0]++;
    int prefix_value = --values[1];
    int compound_value = (values[3] += 5);
    int subtract_value = (values[1] -= 3);
    int multiply_value = (values[2] *= 2);
    narrow_values[0] *= 3;
    short_values[0] *= 2;
    wrap_values[0] *= 2;
    bit_values[0] &= 0x3c;
    bit_values[0] |= 0x80;
    bit_values[0] ^= 0x88;
    int division_value = (divmod_values[0] /= 7);
    int remainder_value = (divmod_values[0] %= 5);
    int left_shift_value = (shift_values[0] <<= 4);
    int right_shift_value = (shift_values[0] >>= 2);
    unsigned int unsigned_shift_value =
        (unsigned_shift_values[0] >>= 31);

    return old_value == 13 && values[0] == 14 &&
        prefix_value == 19 && values[1] == 16 &&
        compound_value == 22 && values[3] == 22 &&
        subtract_value == 16 && multiply_value == 6 && values[2] == 6 &&
        narrow_values[0] == 63 && short_values[0] == 3000 &&
        wrap_values[0] == 144 && bit_values[0] == 0x10 &&
        division_value == 14 && remainder_value == 4 &&
        left_shift_value == 48 && right_shift_value == 12 &&
        unsigned_shift_value == 1 ? 0 : 1;
}

int main(void)
{
    int count = 4;
    int rows[2][count];
    _Atomic(int (*)[count]) row_cursor = rows;
    int (*old_row)[count];
    int (*prefix_row)[count];
    int (*postfix_next_row)[count];
    int (*plain_cursor)[count] = rows;
    int (*old_plain_row)[count];
    int (*prefix_plain_row)[count];
    int (*postfix_next_plain_row)[count];
    int (*sum_plain_row)[count];
    int (*reverse_sum_plain_row)[count];
    int (*compound_plain_row)[count];
    int (*sub_plain_row)[count];
    int (*array_sum_row)[count];
    int (*compound_atomic_row)[count];
    int scalar_rmw_result = exercise_atomic_int_vla_rmw(count);
    count = 5;
    old_row = row_cursor++;
    postfix_next_row = row_cursor;
    old_plain_row = plain_cursor++;
    postfix_next_plain_row = plain_cursor;
    prefix_plain_row = --plain_cursor;
    prefix_row = --row_cursor;
    sum_plain_row = plain_cursor + 1;
    reverse_sum_plain_row = 1 + plain_cursor;
    array_sum_row = rows + 1;
    compound_plain_row = (plain_cursor += 1);
    sub_plain_row = postfix_next_plain_row - 1;
    plain_cursor -= 1;
    compound_atomic_row = (row_cursor += 1);
    row_cursor -= 1;

    return scalar_rmw_result == 0 &&
        old_row == &rows[0] && row_cursor == &rows[0] &&
        prefix_row == &rows[0] &&
        (char*)postfix_next_row - (char*)rows == 4 * sizeof(int) &&
        old_plain_row == &rows[0] &&
        (char*)postfix_next_plain_row - (char*)rows == 4 * sizeof(int) &&
        plain_cursor == &rows[0] && prefix_plain_row == &rows[0] &&
        (char*)sum_plain_row - (char*)rows == 4 * sizeof(int) &&
        (char*)reverse_sum_plain_row - (char*)rows == 4 * sizeof(int) &&
        (char*)compound_plain_row - (char*)rows == 4 * sizeof(int) &&
        (char*)array_sum_row - (char*)rows == 4 * sizeof(int) &&
        sub_plain_row == &rows[0] &&
        (char*)compound_atomic_row - (char*)rows == 4 * sizeof(int) ? 0 : 1;
}
