/* SPDX-License-Identifier: MIT */
typedef int (*callback_t)(int);

int c_null_void_pointer_constant(void)
{
    int* object_pointer = (void*)0;
    callback_t function_pointer = (void*)0;

    object_pointer = (void*)0;
    function_pointer = (void*)0;
    return object_pointer == 0 && function_pointer == 0;
}
