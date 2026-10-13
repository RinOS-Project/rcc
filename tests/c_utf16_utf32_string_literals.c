typedef unsigned short char16_t;
typedef unsigned int char32_t;

const char16_t global_utf16[] = u"A\u03a9\U0001f600";
const char32_t global_utf32[] = U"A\u03a9\U0001f600";
const char16_t adjacent_utf16[] = u"A" u"\u03a9";
const char16_t nul_utf16[] = u"A\0B";

int probe_c_utf16_utf32_string_literals(void)
{
    const char16_t* utf16 = u"A\u03a9\U0001f600";
    const char32_t* utf32 = U"A\u03a9\U0001f600";
    char16_t local_utf16[] = u"A\u03a9";
    char32_t local_utf32[] = U"A\u03a9";
    return sizeof(u"A\u03a9\U0001f600") == 10 &&
           sizeof(U"A\u03a9\U0001f600") == 16 &&
           global_utf16[0] == 'A' && global_utf16[1] == 0x03a9 &&
           global_utf16[2] == 0xd83d && global_utf16[3] == 0xde00 &&
           global_utf16[4] == 0 && global_utf32[0] == 'A' &&
           global_utf32[1] == 0x03a9 && global_utf32[2] == 0x1f600 &&
           global_utf32[3] == 0 && adjacent_utf16[0] == 'A' &&
           adjacent_utf16[1] == 0x03a9 && adjacent_utf16[2] == 0 &&
           nul_utf16[0] == 'A' && nul_utf16[1] == 0 &&
           nul_utf16[2] == 'B' && nul_utf16[3] == 0 &&
           utf16[2] == 0xd83d && utf16[3] == 0xde00 &&
           utf32[2] == 0x1f600 && local_utf16[1] == 0x03a9 &&
           local_utf16[2] == 0 && local_utf32[1] == 0x03a9 &&
           local_utf32[2] == 0 ? 0 : 1;
}
