int select_char_type(char) { return 1; }
int select_char_type(signed char) { return 2; }
int select_char8_type(unsigned char) { return 3; }
int select_char8_type(char8_t) { return 4; }

extern "C" int main() {
    char plain = 'a';
    signed char signed_value = 'a';
    unsigned char unsigned_value = 'a';
    char8_t utf8_value = u8'a';
    return (&typeid(char) != &typeid(signed char) &&
            &typeid(char) != &typeid(unsigned char) &&
            &typeid(char8_t) != &typeid(unsigned char) &&
            select_char_type(plain) == 1 &&
            select_char_type(signed_value) == 2 &&
            select_char8_type(unsigned_value) == 3 &&
            select_char8_type(utf8_value) == 4) ? 0 : 1;
}
