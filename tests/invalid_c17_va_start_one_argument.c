#include <stdarg.h>

void invalid_c17_optional_va_start(int marker, ...) {
    va_list arguments;
    va_start(arguments);
    va_end(arguments);
}
