int target(void);

int first_value = 11;
int second_value = 22;

int target(void)
{
    return second_value;
}

unsigned long target_address(void)
{
    return (unsigned long)target;
}

int main(void)
{
    return *(&second_value) + (target_address() != 0);
}
