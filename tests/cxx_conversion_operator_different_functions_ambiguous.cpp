struct FirstBase {
    int value;
};

struct FirstDerived : FirstBase {};

struct SecondBase {
    int value;
};

class DivergentConversionSource {
public:
    operator FirstDerived*() const { return 0; }
    operator SecondBase*() const { return 0; }
};

int choose_divergent_conversion(FirstBase*) { return 1; }
int choose_divergent_conversion(SecondBase*) { return 2; }

template<typename T>
int choose_divergent_template(T, FirstBase*) { return 3; }

template<typename T>
int choose_divergent_template(T, SecondBase*) { return 4; }

#if defined(TEST_TEMPLATE_CASE)
int probe_different_conversion_functions(DivergentConversionSource source) {
    return choose_divergent_template(0, source);
}
#else
int probe_different_conversion_functions(DivergentConversionSource source) {
    return choose_divergent_conversion(source);
}
#endif
