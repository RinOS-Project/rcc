int main(void)
{
    int static_assert = 37;
    static_assert += 5;
    return static_assert != 42;
}
