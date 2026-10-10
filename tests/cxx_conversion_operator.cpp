class Meter {
public:
    int value;

    operator int() {
        return value;
    }
};

class Flag {
public:
    int value;

    explicit operator bool() {
        return value != 0;
    }
};

class RankedMeter {
public:
    operator float() {
        return 13.0f;
    }

    operator double() {
        return 17.0;
    }

    operator int() {
        return 19;
    }
};

class PromotionMeter {
public:
    operator short() {
        return 23;
    }

    operator long() {
        return 29;
    }
};

class FloatingPromotionMeter {
public:
    operator float() {
        return 31.0f;
    }

    operator int() {
        return 37;
    }
};

class TemplatedConversionSource {
public:
    template <class T>
    operator T() const {
        return static_cast<T>(67);
    }
};

class ConstrainedTemplatedConversionSource {
public:
    template <class T>
        requires (sizeof(T) == 4)
    operator T() const {
        return static_cast<T>(73);
    }
};

struct ReferenceConversionResult {
    int value;
};

struct ConversionBasePointerTarget {
    int value;
};

struct ConversionDerivedPointerTarget : ConversionBasePointerTarget {};

class ConversionPointerSource {
public:
    ConversionDerivedPointerTarget* value;

    operator ConversionDerivedPointerTarget*() const {
        return value;
    }
};

class ReferenceConversionSource {
public:
    operator ReferenceConversionResult() && {
        return {43};
    }
};

class RefQualifiedConversionSource {
public:
    operator ReferenceConversionResult() & {
        return {47};
    }

    operator ReferenceConversionResult() && {
        return {53};
    }
};

class CvRefQualifiedConversionSource {
public:
    operator ReferenceConversionResult() const & {
        return {59};
    }

    operator ReferenceConversionResult() && {
        return {61};
    }
};

int take_integer(int value) {
    return value;
}

int take_ranked_integer(int value) {
    return value;
}

int take_promoted_integer(int value) {
    return value;
}

double take_promoted_double(double value) {
    return value;
}

int choose_conversion_reference(ReferenceConversionResult&& value) {
    return value.value + 100;
}

int choose_conversion_reference(const ReferenceConversionResult& value) {
    return value.value + 200;
}

int read_conversion_reference(const ReferenceConversionResult& value) {
    return value.value;
}

long take_long(long value) {
    return value;
}

int choose_conversion(int value) {
    return value + 100;
}

int choose_constrained_conversion(int) {
    return 101;
}

int choose_constrained_conversion(double) {
    return 102;
}

long choose_conversion(long value) {
    return value + 200;
}

int choose_pointer_conversion(ConversionBasePointerTarget*) {
    return 71;
}

int choose_pointer_conversion(void*) {
    return 73;
}

template<typename T>
int choose_template_pointer_conversion(T, ConversionBasePointerTarget*) {
    return 79;
}

template<typename T>
int choose_template_pointer_conversion(T, void*) {
    return 83;
}

template<typename T>
int take_template_integer(T, int value) {
    return value + 10;
}

template<typename T>
int choose_template_conversion(T, int value) {
    return value + 100;
}

template<typename T>
long choose_template_conversion(T, long value) {
    return value + 200;
}

int return_integer() {
    Meter meter{9};
    return meter;
}

long return_long() {
    Meter meter{11};
    return meter;
}

int main() {
    Meter meter{7};
    RefQualifiedConversionSource ref_qualified_source;
    const CvRefQualifiedConversionSource const_ref_qualified_source{};
    ConversionDerivedPointerTarget derived_pointer_target{67};
    ConversionPointerSource pointer_source{&derived_pointer_target};
    Flag flag{1};
    RankedMeter ranked_meter;
    PromotionMeter promotion_meter;
    FloatingPromotionMeter floating_promotion_meter;
    TemplatedConversionSource templated_source;
    ConstrainedTemplatedConversionSource constrained_templated_source;
    int direct = meter;
    int templated_direct = templated_source;
    int argument = take_integer(meter);
    int templated_argument = take_integer(templated_source);
    long widened = take_long(meter);

    if (!flag) return 1;
    if (direct != 7 || templated_direct != 67 ||
        templated_argument != 67 || argument != 7 || widened != 7 ||
        choose_constrained_conversion(constrained_templated_source) != 101 ||
        return_integer() != 9 || return_long() != 11 ||
        choose_conversion(meter) != 107 ||
        take_template_integer(0, meter) != 17 ||
        choose_template_conversion(0, meter) != 107 ||
        take_ranked_integer(ranked_meter) != 19 ||
        take_promoted_integer(promotion_meter) != 23 ||
        take_promoted_double(floating_promotion_meter) != 31.0 ||
        choose_conversion_reference(ReferenceConversionSource{}) != 143 ||
        read_conversion_reference(ref_qualified_source) != 47 ||
        read_conversion_reference(RefQualifiedConversionSource{}) != 53 ||
        read_conversion_reference(const_ref_qualified_source) != 59 ||
        read_conversion_reference(CvRefQualifiedConversionSource{}) != 61 ||
        choose_pointer_conversion(pointer_source) != 71 ||
        choose_template_pointer_conversion(0, pointer_source) != 79) {
        return 1;
    }
    return 0;
}
