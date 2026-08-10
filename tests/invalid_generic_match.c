int missing_match(int* pointer)
{
    return _Generic(pointer, char*: 1);
}
