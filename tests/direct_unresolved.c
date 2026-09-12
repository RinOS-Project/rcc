int external_target(void);
extern int external_data;
int* external_pointer = &external_data;

unsigned long main(void)
{
    return (unsigned long)external_target + (unsigned long)external_pointer;
}
