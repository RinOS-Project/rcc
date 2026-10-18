struct ConversionResultBase {
    operator short() const {
        return 11;
    }
};

struct ConversionResultDerived : ConversionResultBase {
    using ConversionResultBase::operator short;

    operator int() const {
        return 22;
    }
};

long convert_inherited_result(ConversionResultDerived value) {
    return value;
}
