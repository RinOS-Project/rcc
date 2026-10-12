int main(void)
{
    const char unicode[] = "\u03a9";
    const char embedded_nul[] = "\0R";
    const char permitted_basic_ucns[] = "\u0024\u0040\u0060";
    const int multibyte_character = '\u03a9';
    return sizeof(unicode) == 3 &&
                   (unsigned char)unicode[0] == 0xce &&
                   (unsigned char)unicode[1] == 0xa9 && unicode[2] == 0 &&
                   sizeof(embedded_nul) == 3 && embedded_nul[0] == 0 &&
                   embedded_nul[1] == 'R' && embedded_nul[2] == 0 &&
                   sizeof(permitted_basic_ucns) == 4 &&
                   permitted_basic_ucns[0] == '$' &&
                   permitted_basic_ucns[1] == '@' &&
                   permitted_basic_ucns[2] == '`' &&
                   sizeof('\u03a9') == sizeof(int) &&
                   multibyte_character == 0xcea9
               ? 0
               : 1;
}
