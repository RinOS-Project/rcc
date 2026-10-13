int select_wide(wchar_t) { return 1; }
int select_wide(int) { return 2; }

const wchar_t global_wide[] = L"A\u03a9\U0001f600";
const wchar_t adjacent_wide[] = L"A" L"\u03a9";
const wchar_t mixed_wide_prefix[] = "A" L"\u03a9";
const wchar_t mixed_wide_prefix_reverse[] = L"A" "\u03a9";
const wchar_t numeric_wide_prefix[] = "\x03a9" L"";
const wchar_t raw_wide[] = LR"RIN(\u03a9 Ω)RIN";
const wchar_t embedded_nul_wide[] = L"A\0B";
const wchar_t wide_character = L'\u03a9';

int main() {
    const wchar_t local[] = L"A\u03a9";
    return sizeof(wchar_t) == 4 &&
                   sizeof(global_wide) / sizeof(global_wide[0]) == 4 &&
                   global_wide[0] == L'A' && global_wide[1] == 0x03a9 &&
                   global_wide[2] == 0x1f600 && global_wide[3] == 0 &&
                   adjacent_wide[1] == 0x03a9 && adjacent_wide[2] == 0 &&
                   mixed_wide_prefix[0] == 'A' &&
                   mixed_wide_prefix[1] == 0x03a9 &&
                   mixed_wide_prefix[2] == 0 &&
                   mixed_wide_prefix_reverse[0] == 'A' &&
                   mixed_wide_prefix_reverse[1] == 0x03a9 &&
                   mixed_wide_prefix_reverse[2] == 0 &&
                   numeric_wide_prefix[0] == 0x3a9 &&
                   raw_wide[0] == '\\' && raw_wide[1] == 'u' &&
                   raw_wide[7] == 0x03a9 && embedded_nul_wide[1] == 0 &&
                   embedded_nul_wide[2] == 'B' && wide_character == 0x03a9 &&
                   select_wide(wide_character) == 1 && select_wide(0) == 2 &&
                   local[1] == 0x03a9 && local[2] == 0
               ? 0
               : 1;
}
