#ifdef __RINOS__
typedef int wchar_t;
#else
#include <stddef.h>
#endif

static const char ordinary_string[] = "\xff";
static const wchar_t wide_string[] = L"\xffff";

int main(void) {
    return '\xff' == -1 && ordinary_string[0] == -1 &&
                   wide_string[0] == 0xffff
               ? 0
               : 1;
}
