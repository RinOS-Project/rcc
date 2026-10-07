struct AssignmentLeft { int value; };
struct AssignmentRight { int value; };

int invalid_assignment_types(int* integer_pointer,
                             const int* readonly_pointer,
                             float* floating_pointer,
                             struct AssignmentLeft* left,
                             struct AssignmentRight* right)
{
    integer_pointer = readonly_pointer;
    integer_pointer = floating_pointer;
    *left = *right;
    return 0;
}
