int assignment_conversions(int* output, const int* input, double number)
{
    const int* readonly = output;
    void* generic = output;
    int value = 0;

    readonly = input;
    generic = output;
    value = number;
    *output = value;
    return *readonly + (*((int*)generic)) + *output;
}
