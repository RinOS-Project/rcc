int assignment_conversions(int* output, const int* input, double number)
{
    const int* readonly = output;
    void* generic = output;
    int* nullable = nullptr;
    int value = 0;

    readonly = input;
    generic = output;
    nullable = nullptr;
    value = number;
    *output = value;
    return *readonly + (*((int*)generic)) + *output + (nullable == nullptr);
}
