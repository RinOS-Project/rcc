int vla_sum(int count);
int vla_reclaim(int count, int rounds);
int vla_continue_reclaim(int count, int rounds);
int vla_break_reclaim(int count, int rounds);
int vla_goto(int count);
int vla_goto_reclaim(int count, int rounds);
int vla_matrix(int rows, int cols);
int vla_parameter(int cols, int values[][cols]);
int vla_pointer_parameter(int cols, int (*values)[cols]);
int vla_star_prototype(int values[*]);
int vla_snapshot(int rows, int cols);
int vla_parameter_snapshot(int cols, int values[][cols]);
int vla_sizeof_type(int count);
int vla_typedef(int count);
int vla_typedef_captures_bound(int count);
int vla_typedef_reached_per_iteration(int count);
int vla_typedef_alias_extents(int rows, int cols);
int vla_typedef_multiple_declarators(void);
int vla_parameter_qualifiers(int values[static const 3],
                             int other[volatile restrict 3]);

static int stack_probe_vla_extent(unsigned int bytes)
{
    volatile unsigned char values[bytes];
    volatile unsigned int preserved = 0x51a7c39du;
    unsigned int offset;
    unsigned int checksum = 0;

    if (bytes == 0) return -1;
    for (offset = 0; offset < bytes; offset += 4096u) {
        unsigned char value = (unsigned char)(offset / 4096u + 17u);
        values[offset] = value;
        checksum += values[offset];
    }
    values[bytes - 1u] = 93u;
    checksum += values[bytes - 1u];
    if (preserved != 0x51a7c39du) return -2;
    return (int)checksum;
}

static int stack_probe_aligned_vla_extent(unsigned int bytes)
{
    _Alignas(64) volatile unsigned char values[bytes];
    volatile unsigned int preserved = 0xa92d6e41u;
    unsigned int offset;

    if (bytes == 0) return -1;
    if ((unsigned long)(void*)values % 64ul != 0ul) return -2;
    for (offset = 0; offset < bytes; offset += 4096u)
        values[offset] = (unsigned char)(offset / 4096u + 29u);
    values[bytes - 1u] = 117u;
    if (preserved != 0xa92d6e41u) return -3;
    return (int)values[0] + (int)values[bytes - 1u];
}

int stack_probe_vla_boundaries(void)
{
    static const unsigned int sizes[] = {
        1u, 15u, 16u, 17u, 4095u, 4096u, 4097u, 8191u, 8192u,
        8193u, 16383u, 16384u, 16385u, 32769u, 65537u
    };
    unsigned int index;

    for (index = 0; index < sizeof(sizes) / sizeof(sizes[0]); ++index) {
        int result = stack_probe_vla_extent(sizes[index]);
        if (result < 0) return (int)(index * 2u + 1u);
        result = stack_probe_aligned_vla_extent(sizes[index]);
        if (result < 0)
            return (int)(index * 3u + (result == -2 ? 2u : 3u));
    }
    return 0;
}

int stack_probe_vla_boundaries_abi(void);

int main(void)
{
    int input[2][3];
    if (vla_sum(5) != 15) return 10;
    if (vla_reclaim(2048, 5000) != 5000) return 11;
    if (vla_continue_reclaim(2048, 5000) != 4999) return 12;
    if (vla_break_reclaim(2048, 5000) != 5000) return 13;
    if (vla_goto(5) != 3) return 14;
    if (vla_goto_reclaim(2048, 5000) != 12502500) return 24;
    if (vla_matrix(2, 3) != 7) return 15;
    if (vla_parameter(3, input) != 9) return 16;
    if (vla_pointer_parameter(3, input) != 13) return 23;
    if (vla_snapshot(2, 3) != 7) return 17;
    if (vla_parameter_snapshot(3, input) != 11) return 18;
    input[0][0] = 4;
    input[1][0] = 5;
    if (vla_parameter_qualifiers(input[0], input[1]) != 9) return 19;
    if (vla_star_prototype(input[0]) != 4) return 20;
    if (vla_sizeof_type(7) != 28) return 21;
    if (vla_typedef(7) != 7) return 22;
    if (vla_typedef_captures_bound(3) != 0) return 25;
    if (vla_typedef_reached_per_iteration(1) != 0) return 26;
    if (vla_typedef_alias_extents(2, 3) != 0) return 27;
    if (vla_typedef_multiple_declarators() != 0) return 28;
    if (stack_probe_vla_boundaries_abi() != 0) return 29;
    return 0;
}

int vla_sum(int count)
{
    int values[count];
    int index;
    int total = 0;

    for (index = 0; index < count; ++index) {
        values[index] = index + 1;
        total += values[index];
    }
    if (sizeof(values) != count * sizeof(int)) return -100;
    return total;
}

int vla_reclaim(int count, int rounds)
{
    int round;
    int total = 0;
    for (round = 0; round < rounds; ++round) {
        int values[count];
        values[0] = 1;
        total += values[0];
    }
    return total;
}

int vla_continue_reclaim(int count, int rounds)
{
    int round;
    int total = 0;
    for (round = 0; round < rounds; ++round) {
        int values[count];
        values[0] = 1;
        if (round == 0) continue;
        total += values[0];
    }
    return total;
}

int vla_break_reclaim(int count, int rounds)
{
    int round;
    int total = 0;
    for (round = 0; round < rounds; ++round) {
        for (;;) {
            int values[count];
            values[0] = 1;
            total += values[0];
            break;
        }
    }
    return total;
}

int vla_goto(int count)
{
    int value = 0;
    {
        int values[count];
        values[0] = 3;
        value = values[0];
        goto done;
    }
done:
    return value;
}

int vla_goto_reclaim(int count, int rounds)
{
    int round = 0;
    int total = 0;

again:
    if (round >= rounds) goto done;
    {
        int values[count];
        values[0] = round + 1;
        total += values[0];
        ++round;
        goto again;
    }

done:
    return total;
}

int vla_matrix(int rows, int cols)
{
    int values[rows][cols];
    values[1][2] = 7;
    if (sizeof(values) != rows * cols * sizeof(int)) return -1;
    return values[1][2];
}

int vla_parameter(int cols, int values[][cols])
{
    values[1][2] = 9;
    return values[1][2];
}

int vla_pointer_parameter(int cols, int (*values)[cols])
{
    if (sizeof(*values) != cols * sizeof(int)) return -1;
    values[1][2] = 13;
    return values[1][2];
}

int vla_snapshot(int rows, int cols)
{
    int values[rows][cols];
    values[1][2] = 7;
    cols = 1;
    if (sizeof(values) != rows * 3 * sizeof(int)) return -1;
    return values[1][2];
}

int vla_parameter_snapshot(int cols, int values[][cols])
{
    cols = 1;
    values[1][2] = 11;
    return values[1][2];
}

int vla_star_prototype(int values[3])
{
    return values[0];
}

int vla_sizeof_type(int count)
{
    return sizeof(int[count]);
}

int vla_typedef(int count)
{
    typedef int Values[count];
    Values values;
    values[0] = count;
    if (sizeof(values) != count * sizeof(int)) return -1;
    return values[0];
}

int vla_typedef_captures_bound(int count)
{
    int original = count;
    typedef int Values[count++];
    int after_typedef = count;
    count += 3;
    Values values;
    return after_typedef == original + 1 &&
                   sizeof(values) == (unsigned)(original * sizeof(int)) &&
                   count == original + 4
               ? 0 : -1;
}

int vla_typedef_reached_per_iteration(int count)
{
    int iterations = 0;
    int total = 0;
    while (iterations < 3) {
        typedef int Values[count++];
        Values values;
        total += (int)(sizeof(values) / sizeof(int));
        ++iterations;
    }
    return count == 4 && total == 6 ? 0 : -1;
}

int vla_typedef_alias_extents(int rows, int cols)
{
    int original_rows = rows;
    int original_cols = cols;
    typedef int Row[cols++];
    typedef Row Matrix[rows++];
    int after_typedefs = rows * 10 + cols;
    rows = 8;
    cols = 9;
    Matrix matrix;
    return after_typedefs == (original_rows + 1) * 10 + original_cols + 1 &&
                   sizeof(matrix) ==
                       (unsigned)(original_rows * original_cols * sizeof(int)) &&
                   rows == 8 && cols == 9
               ? 0 : -1;
}

int vla_typedef_multiple_declarators(void)
{
    int left = 2;
    int right = 3;
    typedef int Left[left++], Right[right++];
    int after_left = left;
    int after_right = right;
    left = 11;
    right = 12;
    Left first;
    Right second;
    return after_left == 3 && after_right == 4 &&
                   sizeof(first) == (unsigned)(2 * sizeof(int)) &&
                   sizeof(second) == (unsigned)(3 * sizeof(int)) &&
                   left == 11 && right == 12
               ? 0 : -1;
}

int vla_parameter_qualifiers(int values[static const 3],
                             int other[volatile restrict 3])
{
    return values[0] + other[0];
}
