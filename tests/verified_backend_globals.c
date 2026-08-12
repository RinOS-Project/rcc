int verified_global_data = 7;
int verified_global_zero;
static int verified_static_data = 5;
extern int verified_external_data;

int verified_global_read(void)
{
    return verified_global_data + verified_global_zero +
        verified_static_data;
}

int verified_global_write(int value)
{
    verified_global_data = value;
    verified_global_zero = value + 1;
    verified_static_data += 2;
    return verified_global_read();
}

int verified_external_read(void)
{
    return verified_external_data;
}
