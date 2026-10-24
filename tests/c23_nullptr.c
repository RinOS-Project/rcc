#include <stddef.h>
#include <stdarg.h>

static_assert(sizeof(nullptr_t) == sizeof(void *));

_Static_assert(sizeof(nullptr_t) == sizeof(void *),
               "nullptr_t must use the pointer representation size");
_Static_assert(_Alignof(nullptr_t) == _Alignof(void *),
               "nullptr_t must use pointer alignment");
_Static_assert(_Generic(nullptr, nullptr_t: 1, default: 0),
               "nullptr must have type nullptr_t");
_Static_assert(_Generic((nullptr_t)0, nullptr_t: 1, default: 0),
               "a null pointer constant converts to nullptr_t");

static nullptr_t static_nullptr;
static void *static_pointer = nullptr;

static nullptr_t last_variadic_nullptr(int marker, ...)
{
    va_list arguments;
    nullptr_t value;
    va_start(arguments, marker);
    value = va_arg(arguments, nullptr_t);
    va_end(arguments);
    return value;
}

int main(void)
{
    int object = 41;
    nullptr_t value = nullptr;
    nullptr_t zero_value = 0;
    nullptr_t empty_value = {};
    void *object_pointer = &object;
    int *conditional_pointer = 1 ? nullptr : &object;

    if (static_nullptr != nullptr || static_pointer != nullptr) return 1;
    if (value != 0 || zero_value != nullptr || empty_value != nullptr)
        return 2;
    if ((nullptr == nullptr) != 1 || (nullptr != 0) != 0) return 3;
    if ((object_pointer == value) != 0 || (object_pointer != value) != 1)
        return 4;
    if (value || !value != 1 || (value ? 1 : 2) != 2) return 5;
    if (conditional_pointer != 0) return 6;

    value = 0;
    if (value != nullptr) return 7;
    if (last_variadic_nullptr(1, nullptr) != nullptr) return 8;
    if (_Generic(value, nullptr_t: 1, default: 0) != 1) return 9;
    return 0;
}
