typedef unsigned int word_t, *word_ptr_t;

int multi_global_left = 3, multi_global_right = 4;
int multi_global_values[2] = {5, 6};
int multi_function_zero(void), multi_function_value(int);

int multi_function_zero(void)
{
    return 12;
}

int multi_function_value(int value)
{
    return value + 13;
}

static int multi_local_sum(int seed)
{
    int left = seed, right = seed + 1, values[2] = {7, 8};
    int *pointer = &right;
    word_t unsigned_value = 9, another_unsigned_value = 10;
    word_ptr_t unsigned_pointer = &unsigned_value;
    return left + *pointer + values[0] + values[1] +
           (int)*unsigned_pointer + (int)another_unsigned_value;
}

int main(void)
{
    return multi_global_left + multi_global_right == 7 &&
           multi_global_values[0] + multi_global_values[1] == 11 &&
           multi_local_sum(1) == 37 &&
           multi_function_zero() + multi_function_value(1) == 26 ? 0 : 1;
}
