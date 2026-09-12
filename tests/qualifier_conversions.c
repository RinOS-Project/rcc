int* discard_volatile(volatile int* value)
{
    return value;
}

int* discard_const(const int* value)
{
    return value;
}

volatile int* add_volatile(int* value)
{
    return value;
}

const int* add_const(int* value)
{
    return value;
}
