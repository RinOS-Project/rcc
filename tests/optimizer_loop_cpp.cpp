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

extern "C" int cxx_switch_constant_direct(void)
{
    int result = 0;
    switch (2) {
        case 1:
            result = 11;
            break;
        case 2:
            result = 22;
            break;
        default:
            result = 33;
            break;
    }
    return result;
}

extern "C" int cxx_switch_constant_fallthrough(void)
{
    int result = 0;
    switch (1) {
        case 1:
            result += 3;
        case 2:
            result += 5;
            break;
        default:
            result += 9;
            break;
    }
    return result;
}

extern "C" int cxx_switch_constant_no_match(void)
{
    int result = 17;
    switch (9) {
        case 1:
            result = 23;
            break;
        case 2:
            result = 29;
            break;
    }
    return result;
}
