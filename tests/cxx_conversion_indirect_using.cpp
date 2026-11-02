struct IndirectConversionPrefix {
    int padding;
};

struct IndirectConversionBase {
    int value;

    operator int() const {
        return value;
    }
};

struct IndirectConversionMiddle : IndirectConversionPrefix,
                                  IndirectConversionBase {
#ifdef TEST_AMBIGUOUS
    operator int() const {
        return -1;
    }
#else
    operator int() volatile {
        return -1;
    }
#endif
};

struct IndirectConversionDerived : IndirectConversionMiddle {
    using IndirectConversionMiddle::operator int;
    using IndirectConversionBase::operator int;
};

struct RefQualifiedConversionBase {
    operator int() & {
        return 31;
    }
};

struct UnqualifiedConversionDerived : RefQualifiedConversionBase {
    using RefQualifiedConversionBase::operator int;

    operator int() {
        return 47;
    }
};

int main() {
    IndirectConversionDerived value;
#ifdef TEST_AMBIGUOUS
    return value;
#else
    UnqualifiedConversionDerived ref_qualified_value;
    static_cast<IndirectConversionBase&>(value).value = 73;
    const IndirectConversionDerived& source = value;
    return source == 73 &&
           static_cast<int>(ref_qualified_value) == 47 ? 0 : 1;
#endif
}
