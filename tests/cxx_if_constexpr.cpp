struct ConstructorIfConstexpr {
    int value;

#if __cplusplus >= 202002L
    constexpr explicit ConstructorIfConstexpr(int initial) {
        if constexpr (1) {
            this->value = initial;
        }
    }
#else
    constexpr explicit ConstructorIfConstexpr(int initial) : value(0) {
        if constexpr (1) {
            this->value = initial;
        }
    }
#endif
};

constexpr ConstructorIfConstexpr constexpr_constructor_if_constexpr{37};

extern "C" {
    int probe_if_constexpr(void) {
        constexpr int enabled = 1;
        int result = 0;
        ConstructorIfConstexpr runtime_constructor_if_constexpr(23);
        if (runtime_constructor_if_constexpr.value != 23 ||
            constexpr_constructor_if_constexpr.value != 37) {
            return -3;
        }
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
