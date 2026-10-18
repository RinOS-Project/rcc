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

const int* const* add_nested_const_through_protected_pointer(int** value)
{
    return value;
}

typedef int QualifierArray[2];

int read_const_typedef_array(const QualifierArray values)
{
    return values[0] + values[1];
}

const int* decay_const_typedef_array(const QualifierArray values)
{
    return values;
}
