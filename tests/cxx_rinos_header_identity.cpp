#include <uchar.h>

int rcc_cxx_rinos_header_identity_probe(const char* input) {
    rin_unicode_mbstate_t state{0u, 0u};
    uint32_t codepoint = 0u;
    int initial = rin_unicode_mbsinit(&state);
    size_t converted = rin_locale_current_mbrtowc32(
        &codepoint, input, 1u, &state);
    return initial && converted == 1u ? static_cast<int>(codepoint) : -1;
}
