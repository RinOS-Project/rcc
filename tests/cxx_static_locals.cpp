static int next_value()
{
    static int value = 3;
    return value++;
}

int main()
{
    if (next_value() != 3) return 1;
    if (next_value() != 4) return 2;
    return next_value() == 5 ? 0 : 3;
}
