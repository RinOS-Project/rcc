int main(void)
{
    int count = 4;
    _Atomic int values[count];
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

    return old_value == 13 && values[0] == 14 &&
        prefix_value == 19 && values[1] == 16 &&
        compound_value == 22 && values[3] == 22 &&
        subtract_value == 16 && values[2] == 6 ? 0 : 1;
}
