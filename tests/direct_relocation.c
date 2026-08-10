int target(void);

int first_value = 11;
extern int second_value;
int second_value = 22;
int second_value;
extern int zero_value;
int zero_value;
int zero_value;

int target(void)
{
    return second_value;
}

unsigned long target_address(void)
{
    return (unsigned long)target;
}

int* zero_address(void)
{
    return &zero_value;
}

int main(void)
{
    return *(&second_value) + *zero_address() + (target_address() != 0);
}
