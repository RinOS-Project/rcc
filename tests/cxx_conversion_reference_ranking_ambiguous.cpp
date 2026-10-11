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
