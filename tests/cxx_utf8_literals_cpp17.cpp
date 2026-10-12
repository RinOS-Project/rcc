typedef unsigned char char8_t;

extern "C" int probe_cxx17_utf8_literals(void)
{
    const char* text = u8"Rin";
    char bytes[] = u8"Rin";
    char8_t user_type_name = 1;
    return (text[0] == 'R') + (bytes[1] == 'i') +
           (sizeof(u8'a') == sizeof(char)) + (sizeof(bytes) == 4) +
           user_type_name - 1;
}
