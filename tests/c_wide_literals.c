typedef int wchar_t;

const wchar_t wide_global[] = L"A\u03a9\U0001f600";
const wchar_t wide_adjacent[] = L"A" L"\u03a9";
const wchar_t wide_mixed_prefix[] = "A" L"\u03a9";
const wchar_t wide_mixed_prefix_reverse[] = L"A" "\u03a9";
const wchar_t wide_embedded_nul[] = L"A\0B";
const wchar_t wide_character = L'\u03a9';

int main(void) {
    const wchar_t local[] = L"A\u03a9";
    return wide_global[0] == 'A' && wide_global[1] == 0x03a9 &&
                   wide_global[2] == 0x1f600 && wide_global[3] == 0 &&
                   wide_adjacent[0] == 'A' && wide_adjacent[1] == 0x03a9 &&
                   wide_adjacent[2] == 0 &&
                   wide_mixed_prefix[0] == 'A' &&
                   wide_mixed_prefix[1] == 0x03a9 &&
                   wide_mixed_prefix[2] == 0 &&
                   wide_mixed_prefix_reverse[0] == 'A' &&
                   wide_mixed_prefix_reverse[1] == 0x03a9 &&
                   wide_mixed_prefix_reverse[2] == 0 &&
                   wide_embedded_nul[1] == 0 &&
                   wide_embedded_nul[2] == 'B' && wide_character == 0x03a9 &&
                   local[1] == 0x03a9 && local[2] == 0
               ? 0
               : 1;
}
