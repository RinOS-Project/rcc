struct ConversionReferenceBase {
    int value;
};

struct ConversionReferenceLeft : ConversionReferenceBase {};

struct ConversionReferenceRight : ConversionReferenceBase {};

struct ConversionReferenceRankingSource {
    ConversionReferenceLeft left;
    ConversionReferenceRight right;

    operator ConversionReferenceLeft&() {
        return left;
    }

    operator ConversionReferenceRight&() {
        return right;
    }
};

void consume_reference_base(const ConversionReferenceBase& value);

int invoke_ambiguous_reference_conversion(
    ConversionReferenceRankingSource& source) {
    consume_reference_base(source);
    return 0;
}

int initialize_ambiguous_reference(
    ConversionReferenceRankingSource& source) {
    const ConversionReferenceBase& reference = source;
    return reference.value;
}

struct ScalarConversionRankingSource {
    operator int() { return 1; }
    operator unsigned int() { return 2; }
};

int initialize_ambiguous_scalar(ScalarConversionRankingSource& source) {
    long long value = source;
    return static_cast<int>(value);
}

int initialize_ambiguous_scalar_prvalue() {
    long long value = ScalarConversionRankingSource{};
    return static_cast<int>(value);
}
