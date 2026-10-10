int verified_switch_nested_case(int value)
{
    switch (value) {
        if (value != 1) {
            case 1:
                return 1;
        }
        default:
            return 0;
    }
    return -1;
}
