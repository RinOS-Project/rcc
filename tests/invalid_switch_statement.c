int invalid_outside_switch(int value)
{
    case 7:
        value++;
    default:
        value++;
    return value;
}

unsigned invalid_duplicate_cases(unsigned value)
{
    switch (value) {
        case -1:
            break;
        case 0xffffffffu:
            break;
        default:
            break;
        default:
            break;
    }
    return value;
}

int invalid_nonconstant_case(int value)
{
    switch (value) {
        case value:
            return 1;
        default:
            return 0;
    }
}

int invalid_switch_type(void)
{
    switch (1.0) {
        default:
            return 0;
    }
}
