void invalid_vla_storage(int count)
{
    static int static_values[count];
    extern int external_values[count];
}

void invalid_vla_member(int count)
{
    struct InvalidVlaMember {
        int values[count];
    } value;
    (void)value;
}

extern int runtime_count;
typedef int InvalidFileScopeTypedef[runtime_count];
