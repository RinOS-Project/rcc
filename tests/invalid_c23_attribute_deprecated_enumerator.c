enum LegacyValue {
    legacy_value [[deprecated("use replacement_value")]] = 1
};

int read_legacy_value(void) {
    return legacy_value;
}
