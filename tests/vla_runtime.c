int vla_sum(int count);

int main(void)
{
    return vla_sum(5) == 15 ? 0 : 1;
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
