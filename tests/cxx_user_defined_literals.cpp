int operator""_answer(unsigned long long value) {
    return static_cast<int>(value + 1);
}

int operator""_scaled(double value) {
    return static_cast<int>(value * 2.0);
}

int operator""_character(char value) {
    return value == 'R' ? 1 : 0;
}

int operator""_text(const char* value, unsigned long length) {
    return value[0] == 'R' && length == 4 ? 1 : 0;
}

int operator""_u8character(unsigned char value) {
    return value == 'R' ? 1 : 0;
}

int operator""_u8text(const unsigned char* value, unsigned long length) {
    return value[0] == 'R' && length == 4 ? 1 : 0;
}

int main() {
    return 42_answer == 43 && 42u_answer == 43 &&
           2.5_scaled == 5 && 'R'_character == 1 &&
           "RinO"_text == 1 && u8'R'_u8character == 1 &&
           u8"RinO"_u8text == 1 ? 0 : 1;
}
