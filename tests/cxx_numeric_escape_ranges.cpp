static const char ordinary_string[] = "\xff";
static const char8_t utf8_string[] = u8"\xff";
static const wchar_t wide_string[] = L"\xffff";
static const wchar_t promoted_wide_string[] = "\x100" L"";
#ifdef __RINOS__
static const wchar_t wrapped_wide_character = L'\xffffffff';
static const wchar_t wrapped_wide_string[] = L"\xffffffff";
#endif

int main() {
    return '\xff' == -1 && u8'\xff' == 255 && L'\xffff' == 0xffff &&
                   ordinary_string[0] == -1 &&
                   static_cast<unsigned char>(utf8_string[0]) == 255 &&
                   wide_string[0] == 0xffff &&
                   promoted_wide_string[0] == 0x100
#ifdef __RINOS__
                   && wrapped_wide_character == -1 &&
                   wrapped_wide_string[0] == -1
#endif
               ? 0
               : 1;
}
