struct QualifierPair {
    int value;
};

int invalid_const_assignment(void)
{
    int const value = 1;
    value = 2;
    return value;
}

int invalid_const_increment(void)
{
    const int value = 3;
    return value++;
}

int invalid_const_pointer(int* left, int* right)
{
    int* const pointer = left;
    pointer = right;
    return *pointer;
}

int invalid_const_member(const struct QualifierPair* pair)
{
    pair->value = 4;
    return pair->value;
}
