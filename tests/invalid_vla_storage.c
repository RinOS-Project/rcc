void invalid_vla_storage(int count)
{
    static int static_values[count];
    extern int external_values[count];
}

extern int runtime_count;
typedef int InvalidFileScopeTypedef[runtime_count];
