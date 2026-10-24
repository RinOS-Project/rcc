#include <stddef.h>

int invalid_nullptr_arithmetic(void)
{
    return (nullptr + 1) != 0;
}
