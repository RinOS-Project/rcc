extern "C" int invalid_utf8_array(void) {
    char ordinary[] = u8"R";
    char8_t utf8[] = "i";
    return ordinary[0] + utf8[0];
}
