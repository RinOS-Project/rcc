int cxx_multi_global = 2, *cxx_multi_pointer = &cxx_multi_global;
int cxx_multi_array[2] = {3, 4};
int cxx_multi_zero(), cxx_multi_value(int);

int cxx_multi_zero()
{
    return 5;
}

int cxx_multi_value(int value)
{
    return value + 6;
}

int main()
{
    int local = 7, second = 8;
    int *pointer = &second;
    return cxx_multi_global + *cxx_multi_pointer == 4 &&
           cxx_multi_array[0] + cxx_multi_array[1] == 7 &&
           local + *pointer == 15 &&
           cxx_multi_zero() + cxx_multi_value(1) == 12 ? 0 : 1;
}
