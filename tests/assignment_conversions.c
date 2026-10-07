typedef int (*AssignmentFunction)(int);
enum AssignmentNull { AssignmentNullZero = 0 };

static int assignment_callback(int value)
{
    return value;
}

int assignment_conversions(int* output, const int* input, double number)
{
    const int* readonly = output;
    void* generic = output;
    int* restored = generic;
    AssignmentFunction callback = assignment_callback;
    int* nullable = output;
    int value = 0;

    readonly = input;
    generic = output;
    nullable = 1 - 1;
    nullable = AssignmentNullZero;
    value = number;
    *output = value;
    return *readonly + *restored + *output + callback(value);
}
