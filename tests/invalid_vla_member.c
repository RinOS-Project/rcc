void invalid_vla_member(int count)
{
    struct InvalidVlaMember {
        int values[count];
    } value;
    (void)value;
}
