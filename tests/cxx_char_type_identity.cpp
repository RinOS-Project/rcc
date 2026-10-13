int select_char_type(char) { return 1; }
int select_char_type(signed char) { return 2; }
int select_char8_type(unsigned char) { return 3; }
int select_char8_type(char8_t) { return 4; }
int select_char16_type(unsigned short) { return 5; }
int select_char16_type(char16_t) { return 6; }
int select_char32_type(unsigned int) { return 7; }
int select_char32_type(char32_t) { return 8; }

extern "C" int main() {
    char plain = 'a';
    signed char signed_value = 'a';
    unsigned char unsigned_value = 'a';
    char8_t utf8_value = u8'a';
    char16_t utf16_value = 0x03a9;
    char32_t utf32_value = 0x1f600;
    return (&typeid(char) != &typeid(signed char) &&
            &typeid(char) != &typeid(unsigned char) &&
            &typeid(char8_t) != &typeid(unsigned char) &&
            &typeid(char16_t) != &typeid(unsigned short) &&
            &typeid(char32_t) != &typeid(unsigned int) &&
            sizeof(char16_t) == sizeof(unsigned short) &&
            sizeof(char32_t) == sizeof(unsigned int) &&
            select_char_type(plain) == 1 &&
            select_char_type(signed_value) == 2 &&
            select_char8_type(unsigned_value) == 3 &&
            select_char8_type(utf8_value) == 4 &&
            select_char16_type(utf16_value) == 6 &&
            select_char32_type(utf32_value) == 8) ? 0 : 1;
}
