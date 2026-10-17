struct ConversionBase {
    int value;
};

struct ConversionPrefix {
    int padding;
};

struct ConversionDerived : ConversionPrefix, ConversionBase {
    int tail;
};

struct ConversionPrvalueSource {
    int value;

    operator ConversionDerived() const {
        return {{5}, {value}, 9};
    }
};

struct ConversionLvalueSource {
    ConversionDerived value;

    operator ConversionDerived&() {
        return value;
    }
};

int read_const_base(const ConversionBase& value) {
    return value.value;
}

int read_rvalue_base(ConversionBase&& value) {
    return value.value;
}

int read_base_value(ConversionBase value) {
    return value.value;
}

int mutate_base(ConversionBase& value) {
    return ++value.value;
}

int main() {
    ConversionPrvalueSource prvalue{47};
    ConversionLvalueSource lvalue{};
    lvalue.value.value = 59;
    return read_const_base(prvalue) != 47 ||
        read_rvalue_base(prvalue) != 47 ||
        read_base_value(prvalue) != 47 ||
        mutate_base(lvalue) != 60 || lvalue.value.value != 60;
}
