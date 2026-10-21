class ImplicitIntegerConversion {
public:
    int value;
    int calls;

    operator int() {
        ++calls;
        return value + 2;
    }
};

class ExplicitIntegerConversion {
public:
    int value;
    int calls;

    explicit operator int() {
        ++calls;
        return value + 3;
    }
};

class ExplicitBooleanConversion {
public:
    int value;
    int calls;

    explicit operator bool() {
        ++calls;
        return value == 2;
    }
};

class ExplicitPointerConversion {
public:
    int* pointer;
    int calls;

    explicit operator int*() {
        ++calls;
        return pointer;
    }
};

#if defined(TEST_EXPLICIT_STANDARD_SEQUENCE_REJECTED)
unsigned reject_explicit_standard_conversion(
    ExplicitIntegerConversion value) {
    return static_cast<unsigned>(value);
}
#endif

#if defined(TEST_IMPLICIT_CONVERSION_REJECTED)
int reject_implicit_conversion(ExplicitIntegerConversion value) {
    return value;
}
#endif

class RankedConversion {
public:
    int integer_calls;
    int floating_calls;

    operator int() {
        ++integer_calls;
        return 5;
    }

    explicit operator double() {
        ++floating_calls;
        return 2.5;
    }
};

int main() {
    ImplicitIntegerConversion implicit_value{7, 0};
    ExplicitIntegerConversion explicit_value{13, 0};
    ExplicitBooleanConversion explicit_boolean{1, 0};
    int pointed_value = 29;
    ExplicitPointerConversion explicit_pointer{&pointed_value, 0};
    RankedConversion ranked_value{0, 0};

    return static_cast<int>(implicit_value) == 9 &&
                   static_cast<unsigned>(implicit_value) == 9u &&
                   implicit_value.calls == 2 &&
                   static_cast<int>(ImplicitIntegerConversion{8, 0}) == 10 &&
                   static_cast<int>(explicit_value) == 16 &&
                   explicit_value.calls == 1 &&
                   !static_cast<bool>(explicit_boolean) &&
                   explicit_boolean.calls == 1 &&
                   static_cast<int*>(explicit_pointer) == &pointed_value &&
                   explicit_pointer.calls == 1 &&
                   static_cast<double>(ranked_value) == 2.5 &&
                   ranked_value.integer_calls == 0 &&
                   ranked_value.floating_calls == 1 &&
                   static_cast<int>(ranked_value) == 5 &&
                   ranked_value.integer_calls == 1 &&
                   ranked_value.floating_calls == 1
        ? 0
        : 1;
}
