int select16(char16_t) { return 16; }
int select16(unsigned short) { return 8; }
int select32(char32_t) { return 32; }
int select32(unsigned int) { return 4; }

extern "C" int probe_char16_char32_types(void) {
    char16_t value16 = 0x03a9;
    char32_t value32 = 0x1f600;
    return sizeof(value16) == 2 && sizeof(value32) == 4 &&
                   select16(value16) == 16 && select32(value32) == 32
               ? 0
               : 1;
}
