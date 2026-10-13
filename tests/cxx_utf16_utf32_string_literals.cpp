const char16_t global_utf16[] = u"A\u03a9\U0001f600";
const char32_t global_utf32[] = U"A\u03a9\U0001f600";
const char16_t raw_utf16[] = uR"(Ω)";
const char32_t raw_utf32[] = UR"(Ω)";
const char16_t adjacent_utf16[] = u"A" u"\u03a9";
const char16_t nul_utf16[] = u"A\0B";
const char16_t utf16_character = u'\u03a9';
const char32_t utf32_character = U'\U0001f600';
const char16_t source_utf16_character = u'Ω';
const char32_t source_utf32_character = U'😀';

int select_code_unit(char16_t) { return 16; }
int select_code_unit(char32_t) { return 32; }
int select_code_unit(int) { return 1; }

extern "C" int probe_utf16_utf32_string_literals(void) {
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
           global_utf32[3] == 0 && raw_utf16[0] == 0x03a9 &&
           raw_utf16[1] == 0 && raw_utf32[0] == 0x03a9 &&
           raw_utf32[1] == 0 && adjacent_utf16[0] == 'A' &&
           adjacent_utf16[1] == 0x03a9 && adjacent_utf16[2] == 0 &&
           nul_utf16[0] == 'A' && nul_utf16[1] == 0 &&
           nul_utf16[2] == 'B' && nul_utf16[3] == 0 &&
           sizeof(u'\u03a9') == 2 && sizeof(U'\U0001f600') == 4 &&
           utf16_character == 0x03a9 && utf32_character == 0x1f600 &&
           source_utf16_character == 0x03a9 &&
           source_utf32_character == 0x1f600 &&
           select_code_unit(u'\u03a9') == 16 &&
           select_code_unit(U'\U0001f600') == 32 &&
           utf16[2] == 0xd83d && utf16[3] == 0xde00 &&
           utf32[2] == 0x1f600 && local_utf16[1] == 0x03a9 &&
           local_utf16[2] == 0 && local_utf32[1] == 0x03a9 &&
           local_utf32[2] == 0 ? 0 : 1;
}
