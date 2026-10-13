extern "C" int probe_cxx_utf8_literals(void) {
    const char8_t* text = u8"Ri" u8"nOS";
    const char8_t* ordinary_adjacent = "Ri" u8"nOS";
    const char8_t* unicode = u8"\u0041\u03a9";
    const char8_t* supplementary = u8"\U0001F600";
    const char8_t* embedded_nul = u8"\u0000R";
    const char* ordinary_unicode = "\u0041\u03a9";
    const char* ordinary_nul = "\u0000R";
    const int ordinary_multibyte_character = '\u03a9';
    char8_t marker = u8'R';
    char8_t escaped = u8'\u0041';
    char8_t nul_character = u8'\u0000';
    char8_t bytes[] = u8"Rin";
    return (int)sizeof(u8'a') + (marker == text[0]) + text[1] + text[4] +
           (escaped == u8'A') + (sizeof(bytes) == 4) + (bytes[1] == 'i') +
           (unicode[0] == 'A') + (unicode[1] == 0xce) +
           (unicode[2] == 0xa9) + (supplementary[0] == 0xf0) +
           (supplementary[1] == 0x9f) + (supplementary[2] == 0x98) +
           (supplementary[3] == 0x80) + (supplementary[4] == 0) +
           (sizeof(u8"\u0000") == 2) + (embedded_nul[0] == 0) +
           (embedded_nul[1] == 'R') + (embedded_nul[2] == 0) +
           (ordinary_adjacent[0] == 'R') +
           (ordinary_adjacent[2] == 'n') + (ordinary_adjacent[5] == 0) +
           (sizeof("Ri" u8"nOS") == 6) +
           (nul_character == 0) + (ordinary_unicode[0] == 'A') +
           ((unsigned char)ordinary_unicode[1] == 0xce) +
           ((unsigned char)ordinary_unicode[2] == 0xa9) +
           (ordinary_unicode[3] == 0) +
           (sizeof("\u0041\u03a9") == 4) +
           (sizeof("\u0000R") == 3) + (ordinary_nul[0] == 0) +
           (ordinary_nul[1] == 'R') + (ordinary_nul[2] == 0) +
           (sizeof('\u03a9') == sizeof(int)) +
           (ordinary_multibyte_character == 0xcea9);
}
