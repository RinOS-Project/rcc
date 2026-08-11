#include <stddef.h>
#include <stdint.h>

uint64_t tool_relative_c_size(void) {
    return (uint64_t)sizeof(size_t);
}
