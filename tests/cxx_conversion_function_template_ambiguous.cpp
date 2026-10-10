class TemplatedConversionSource {
public:
    template <class T>
    operator T() const {
        return static_cast<T>(67);
    }
};

int choose_templated_conversion(int) {
    return 1;
}

int choose_templated_conversion(double) {
    return 2;
}

int trigger_ambiguous_conversion() {
    return choose_templated_conversion(TemplatedConversionSource{});
}
