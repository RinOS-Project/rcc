int vla_sum(int count);
int vla_reclaim(int count, int rounds);
int vla_continue_reclaim(int count, int rounds);
int vla_break_reclaim(int count, int rounds);
int vla_goto(int count);

int main(void)
{
    if (vla_sum(5) != 15) return 10;
    if (vla_reclaim(2048, 5000) != 5000) return 11;
    if (vla_continue_reclaim(2048, 5000) != 4999) return 12;
    if (vla_break_reclaim(2048, 5000) != 5000) return 13;
    if (vla_goto(5) != 3) return 14;
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
