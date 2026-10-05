extern "C" int cxx_loop_for_two(void)
{
    int result = 0;
    for (int index = 0; index < 2; ++index) {
        result += 73;
    }
    return result;
}

extern "C" int cxx_loop_while_two(void)
{
    int index = 0;
    int result = 0;
    while (index < 2) {
        result += 79;
        ++index;
    }
    return result;
}

extern "C" int cxx_loop_do_two(void)
{
    int index = 0;
    int result = 0;
    do {
        result += 83;
        ++index;
    } while (index < 2);
    return result;
}
