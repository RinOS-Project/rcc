/* Direct-image PIC coverage for compiler-owned GOT slots. */

static int local_data = 7;

static int local_function(int value)
{
    return value + 1;
}

int main(void)
{
    int result = local_data;
    if (local_function != 0) result += local_function(4);
    return result == 12 ? 0 : 1;
}
