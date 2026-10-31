#include <stdarg.h>

struct C23VaPair {
    unsigned left;
    unsigned right;
};

static unsigned consume_one_argument_va_start(int marker, ...) {
    va_list arguments;
    va_start(arguments);
    const int first = va_arg(arguments, int);
    const struct C23VaPair pair = va_arg(arguments, struct C23VaPair);
    const int last = va_arg(arguments, int);
    va_end(arguments);
    return (unsigned)marker + (unsigned)first + pair.left + pair.right +
        (unsigned)last;
}

static unsigned consume_two_argument_va_start(int marker, ...) {
    va_list arguments;
    va_start(arguments, marker);
    const int first = va_arg(arguments, int);
    const struct C23VaPair pair = va_arg(arguments, struct C23VaPair);
    const int last = va_arg(arguments, int);
    va_end(arguments);
    return (unsigned)marker + (unsigned)first + pair.left + pair.right +
        (unsigned)last;
}

int main(void) {
    const struct C23VaPair first = {11u, 17u};
    const struct C23VaPair second = {23u, 31u};
    if (consume_one_argument_va_start(2, 3, first, 5) != 38u) return 1;
    if (consume_two_argument_va_start(7, 13, second, 19) != 93u) return 2;
    return 0;
}
