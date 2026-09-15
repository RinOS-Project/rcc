extern "C" {
    int probe_if_constexpr(void) {
        constexpr int enabled = 1;
        int result = 0;
        if constexpr (enabled) {
            result = 40;
        } else {
            result = -1;
        }
        if constexpr (0) {
            result = -2;
        } else {
            result += 2;
        }
        return result;
    }
}
