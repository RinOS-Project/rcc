extern int imported_function(int value);

int call_import(void)
{
    return imported_function(37);
}
