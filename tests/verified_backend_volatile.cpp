struct VerifiedVolatileFields {
    int value;
};

volatile int verified_cpp_volatile_global;

int verified_cpp_volatile_local(int input)
{
    volatile int value = input;
    value += 1;
    return value;
}

int verified_cpp_volatile_reference(volatile int& value)
{
    value += 1;
    return value;
}

int verified_cpp_volatile_member(volatile VerifiedVolatileFields* fields,
                                 int input)
{
    fields->value = input;
    return fields->value;
}

int verified_cpp_volatile_global_access(int input)
{
    verified_cpp_volatile_global = input;
    return verified_cpp_volatile_global;
}
