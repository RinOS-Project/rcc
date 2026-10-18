struct ConversionAccessBase {
    int value;
};

struct ConversionAccessDerived : ConversionAccessBase {};

class PrivatePreferredConversionSource {
    ConversionAccessBase base_value;
    ConversionAccessDerived derived_value;

    operator ConversionAccessBase*() {
        return &base_value;
    }

public:
    operator ConversionAccessDerived*() {
        return &derived_value;
    }
};

class PrivateContextualBoolSource {
    explicit operator bool() const {
        return true;
    }
};

ConversionAccessBase* select_private_preferred_conversion(
    PrivatePreferredConversionSource& source) {
    return source;
}

int select_private_contextual_bool(
    const PrivateContextualBoolSource& source) {
    if (source) return 1;
    return 0;
}
