int target(void);

int first_value = 11;
extern int second_value;
int second_value = 22;
int second_value;
extern int zero_value;
int zero_value;
int zero_value;
int static_values[2];
char* static_literal = "StaticRinOS";
int* static_zero = &zero_value;
int* static_second = &static_values[1];
int (*static_target)(void) = &target;

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

char* literal_address(void)
{
    return "RinOS";
}

int main(void)
{
    return *(&second_value) + *zero_address() + (target_address() != 0) +
           (literal_address() != 0) + (static_literal[0] == 'S') +
           *static_zero + *static_second + (static_target() != 0);
}
