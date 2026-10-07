extern "C" int verified_cpp_debug(int value)
{
    int local = value + 3;
    {
        int nested = local + 1;
        return nested;
    }
}
