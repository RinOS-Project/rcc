struct Implicit {
    explicit(false) Implicit(int value) : value(value) {}

    operator int() const { return value; }

    int value;
};

struct Explicit {
    explicit(true) Explicit(int value) : value(value) {}

    operator int() const { return value; }

    int value;
};

template <typename T>
struct ConditionalTemplate {
    explicit(sizeof(T) > 4) ConditionalTemplate(int value) : value(value) {}

    explicit(sizeof(T) > 4) operator bool() const { return value != 0; }

    int value;
};

struct ConditionalConversionTemplate {
    template <typename T>
    explicit(sizeof(T) > 4) operator T() const {
        return static_cast<T>(23);
    }
};

int main() {
    Implicit implicit_value = 7;
    Explicit explicit_value(9);
    int first = implicit_value;
    int second = explicit_value;
    if (first != 7) return 2;
    if (second != 9) return 3;

    ConditionalTemplate<char> small = 11;
    bool small_conversion = small;
    ConditionalTemplate<long long> large{13};
    ConditionalConversionTemplate conversion_template;
    int templated_conversion = conversion_template;
    if (small.value != 11 || !small_conversion) return 4;
    if (large.value != 13 || !large) return 5;
    if (templated_conversion != 23) return 6;
    return 0;
}
