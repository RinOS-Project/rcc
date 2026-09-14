static int next_value(void)
{
    static int value = 3;
    return ++value;
}

static int static_storage(void)
{
    static int value;
    static int *pointer = &value;
    *pointer += 5;
    return value;
}

static int branch_storage(void)
{
    if (1) {
        static int value = 10;
        return ++value;
    }
    return 0;
}

static int loop_storage(void)
{
    int total = 0;
    for (int index = 0; index != 2; ++index) {
        static int value = 20;
        total += ++value;
    }
    return total;
}

int main(void)
{
    return next_value() == 4 && next_value() == 5 &&
                   static_storage() == 5 && static_storage() == 10 &&
                   branch_storage() == 11 && branch_storage() == 12 &&
                   loop_storage() == 42 && loop_storage() == 46
               ? 0 : 1;
}
