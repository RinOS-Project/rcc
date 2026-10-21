int main(void)
{
    int count = 4;
    _Atomic int values[count];
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
    int old_value;
    int prefix_value;
    int compound_value;
    int subtract_value;

    values[0] = 13;
    values[1] = 20;
    values[2] = 3;
    values[3] = 17;
    old_value = values[0]++;
    prefix_value = --values[1];
    compound_value = (values[3] += 5);
    subtract_value = (values[1] -= 3);
    values[2] *= 2;
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

    return old_value == 13 && values[0] == 14 &&
        prefix_value == 19 && values[1] == 16 &&
        compound_value == 22 && values[3] == 22 &&
        subtract_value == 16 && values[2] == 6 &&
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
