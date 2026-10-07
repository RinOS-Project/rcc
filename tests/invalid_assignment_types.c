struct AssignmentLeft { int value; };
struct AssignmentRight { int value; };
typedef int (*AssignmentFunction)(int);

int invalid_assignment_types(int* integer_pointer,
                             const int* readonly_pointer,
                             float* floating_pointer,
                             void* erased_pointer,
                             AssignmentFunction function_pointer,
                             int integer_value,
                             long long wide_value,
                             struct AssignmentLeft* left,
                             struct AssignmentRight* right)
{
    integer_pointer = readonly_pointer;
    integer_pointer = floating_pointer;
    *left = *right;
    integer_pointer = integer_value;
    wide_value = integer_pointer;
    erased_pointer = function_pointer;
    function_pointer = erased_pointer;
    return 0;
}
