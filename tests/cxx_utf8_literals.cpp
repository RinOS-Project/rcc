extern "C" int probe_cxx_utf8_literals(void) {
    const char8_t* text = u8"Ri" u8"nOS";
    const char8_t* unicode = u8"\u0041\u03a9";
    char8_t marker = u8'R';
    char8_t bytes[] = u8"Rin";
    return (int)sizeof(u8'a') + (marker == text[0]) + text[1] + text[4] +
           (sizeof(bytes) == 4) + (bytes[1] == 'i') +
           (unicode[0] == 'A') + (unicode[1] == 0xce) +
           (unicode[2] == 0xa9);
}
