int verified_switch_nested_label_fallback(int value)
{
    switch (value) {
        if (value) {
            case 1:
                return 1;
        }
        default:
            return 0;
    }
    return -1;
}
