int main(void)
{
    int count = 4;
    _Atomic int values[count];
    int rows[2][count];
    _Atomic(int (*)[count]) row_cursor = rows;
    int (*old_row)[count];
    int (*prefix_row)[count];
    int (*plain_cursor)[count] = rows;
    int (*old_plain_row)[count];
    int (*prefix_plain_row)[count];
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
    old_plain_row = plain_cursor++;
    prefix_plain_row = --plain_cursor;
    prefix_row = --row_cursor;

    return old_value == 13 && values[0] == 14 &&
        prefix_value == 19 && values[1] == 16 &&
        compound_value == 22 && values[3] == 22 &&
        subtract_value == 16 && values[2] == 6 &&
        old_row == &rows[0] && row_cursor == &rows[0] &&
        prefix_row == &rows[0] && old_plain_row == &rows[0] &&
        plain_cursor == &rows[0] && prefix_plain_row == &rows[0] ? 0 : 1;
}
