typedef unsigned short char16_t;
typedef unsigned int char32_t;
#ifdef __RINOS__
typedef int wchar_t;
#else
#include <stddef.h>
#endif

int invalid_ordinary_character = '\x100';
const char invalid_ordinary_string[] = "\x100";
const char invalid_concatenated_string[] = "\x100" "";
char16_t invalid_utf16_character = u'\x10000';
const char16_t invalid_utf16_string[] = u"\x10000";
char32_t invalid_utf32_character = U'\x100000000';
const wchar_t invalid_wide_string[] = L"\x100000000";
