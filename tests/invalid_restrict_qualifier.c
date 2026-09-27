restrict int invalid_scalar;
int (*restrict invalid_function_pointer)(void);

int invalid_type_name(void)
{
    return sizeof(int (*restrict)(void));
}
