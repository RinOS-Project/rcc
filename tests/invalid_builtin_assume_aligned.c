void* invalid_assume_aligned(void* value)
{
    return __builtin_assume_aligned(value, 3);
}
