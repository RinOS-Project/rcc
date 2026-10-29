/* SPDX-License-Identifier: MIT */
typedef int (*callback_t)(int);
typedef int c_void_array_t[32];
typedef c_void_array_t* c_void_array_pointer_t;

static void c_void_pointer_consumer(void* pointer)
{
    (void)pointer;
}

int c_void_pointer_array_conversion(c_void_array_pointer_t pointer)
{
    void* opaque = pointer;
    const void* const_opaque = pointer;
    c_void_array_pointer_t restored = opaque;
    c_void_pointer_consumer(pointer);
    return restored == pointer && const_opaque == opaque;
}

int c_null_void_pointer_constant(void)
{
    int* object_pointer = (void*)0;
    callback_t function_pointer = (void*)0;

    object_pointer = (void*)0;
    function_pointer = (void*)0;
    return object_pointer == 0 && function_pointer == 0;
}
