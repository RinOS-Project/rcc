#include <stddef.h>

int invalid_nullptr_conditional(void)
{
    return (1 ? nullptr : 0) == nullptr;
}
