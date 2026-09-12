int goto_forward(int value)
{
    int result = 0;
    goto selected;
    result = 1000;
selected:
    result += value;
    return result;
}

int goto_backward(int limit)
{
    int index = 0;
    int result = 0;
loop:
    if (index >= limit)
        goto done;
    result += index;
    ++index;
    goto loop;
done:
    return result;
}

int goto_into_constant_if(void)
{
    int result = 0;
    goto false_arm;
    if (0) {
false_arm:
        result = 41;
    }
    return result + 1;
}

int goto_into_constant_while(void)
{
    int result = 0;
    goto loop_body;
    while (0) {
loop_body:
        result = 77;
        break;
    }
    return result;
}

int goto_into_switch(int value)
{
    int result = 0;
    goto direct;
    switch (value) {
        case 1:
            result = 1;
            break;
direct:
        result = 7;
        break;
    }
    return result;
}

int goto_reused_label_a(int value)
{
    goto done;
done:
    return value + 1;
}

int goto_reused_label_b(int value)
{
    goto done;
done:
    return value + 2;
}
