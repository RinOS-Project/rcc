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

int malformed_nested_initializer(void)
{
    int value = (1 + );
    int after = 7;
    return after + value;
}

typedef int recovery_type;

int malformed_missing_semicolon(void)
{
    int bad = 1
    recovery_type after_missing = ;
    return after_missing + bad;
}
