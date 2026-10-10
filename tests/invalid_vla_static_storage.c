void invalid_static_vla_storage(int count)
{
    static int values[count];
    (void)values;
}
