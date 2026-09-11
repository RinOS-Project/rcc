static int ir_lower_helper(int value)
{
    return value * 3;
}

int ir_lower_control_flow(int input)
{
    int value = input;
    int index;

    if (value < 0) {
        value = -value;
    } else {
        value += 1;
    }
    while (value < 10) {
        value += 2;
    }
    for (index = 0; index < 2; ++index) {
        value += index;
    }
    do {
        value -= 1;
    } while (value > 12);
    return ir_lower_helper(value);
}

int ir_lower_pointer_truth(int* value)
{
    if (!value) {
        return 0;
    }
    *value += 1;
    return *value;
}

int ir_lower_conditional(int input)
{
    return input ? 1 : 0;
}
