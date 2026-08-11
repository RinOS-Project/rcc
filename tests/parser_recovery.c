int malformed_parameter(UnknownParameterType value, int good)
{
    return value + good;
}

struct malformed_fields {
    UnknownFieldType field;
    int good;
};

int malformed_block(void)
{
    );
    int after = 3;
    return after;
}

int malformed_initializer(void)
{
    int value = ;
    return value;
}
