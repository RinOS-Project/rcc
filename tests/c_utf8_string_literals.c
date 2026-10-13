int probe_c_utf8_string_literals(void)
{
    int char8_t = 3;
    int char16_t = 5;
    int char32_t = 7;
    const char* text = u8"Rin";
    unsigned char bytes[] = u8"Rin";
    char ordinary[] = "OS";
    return (text[0] == 'R') + (bytes[1] == 'i') +
           (ordinary[0] == 'O') + (sizeof(bytes) == 4) + char8_t - 3 +
           char16_t - 5 + char32_t - 7;
}
