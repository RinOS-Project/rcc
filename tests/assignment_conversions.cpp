typedef int (*AssignmentFunction)(int);

static int assignment_callback(int value)
{
    return value;
}

int assignment_conversions(int* output, const int* input, double number)
{
    const int* readonly = output;
    void* generic = output;
    int* nullable = nullptr;
    AssignmentFunction callback = assignment_callback;
    int value = 0;

    readonly = input;
    generic = output;
    nullable = nullptr;
    nullable = 0;
    callback = assignment_callback;
    value = number;
    *output = value;
    return *readonly + (*((int*)generic)) + *output + callback(value) +
           (nullable == nullptr);
}
