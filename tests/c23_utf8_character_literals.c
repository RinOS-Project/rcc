typedef unsigned char char8_t;

_Static_assert(_Generic(u8'A', char8_t: 1, default: 0),
               "UTF-8 character constants have char8_t type");
_Static_assert(_Generic(u8"A", char8_t*: 1, default: 0),
               "UTF-8 string literals have char8_t elements");
_Static_assert(u8'A' == 'A', "ASCII UTF-8 character value");
_Static_assert(u8'\xE9' == 0xE9, "numeric UTF-8 code-unit value");

char8_t utf8_ascii = u8'A';
char8_t utf8_numeric = u8'\xE9';
