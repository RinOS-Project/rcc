int read_legacy_parameter(int legacy_value [[deprecated("use current_value")]]);

int read_legacy_parameter(int legacy_value) {
    return legacy_value;
}
