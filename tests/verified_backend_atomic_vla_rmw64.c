int exercise_atomic_vla_rmw64(int count)
{
    _Atomic long long values[count];
    long long old_value;
    long long prefix_value;
    long long compound_value;
    long long subtract_value;

    values[0] = 21;
    values[2] = 11;
    old_value = values[0]++;
    prefix_value = --values[2];
    compound_value = (values[0] += 5);
    subtract_value = (values[2] -= 4);

    return old_value == 21 && values[0] == 27 &&
        prefix_value == 10 && values[2] == 6 &&
        compound_value == 27 && subtract_value == 6 ? 0 : 1;
}

int main(void)
{
    return exercise_atomic_vla_rmw64(3);
}
