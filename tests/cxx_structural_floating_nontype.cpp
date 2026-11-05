struct FloatingValue {
    float value;
};

constexpr FloatingValue floating_value{1.25f};

struct FloatingArrayValue {
    float elements[2];
};

constexpr FloatingArrayValue floating_array_value{{1.25f, -2.5f}};

union FloatingAlternative {
    float single;
    double wide;
};

constexpr FloatingAlternative floating_union_value{1.25f};

template<FloatingValue Value>
int fixed_structural_float_nttp() {
    return Value.value == 1.25f ? 0 : 1;
}

template<auto Value>
int deduced_structural_float_nttp() {
    return Value.value == 1.25f ? 0 : 1;
}

template<FloatingArrayValue Value>
int structural_float_array_nttp() {
    return Value.elements[0] == 1.25f && Value.elements[1] == -2.5f
        ? 0 : 1;
}

template<auto Value>
int deduced_structural_float_array_nttp() {
    return Value.elements[0] == 1.25f && Value.elements[1] == -2.5f
        ? 0 : 1;
}

template<FloatingAlternative Value>
int structural_float_union_nttp() {
    return Value.single == 1.25f ? 0 : 1;
}

template<auto Value>
int deduced_structural_float_union_nttp() {
    return Value.single == 1.25f ? 0 : 1;
}

template<FloatingValue Value>
struct FloatingBox {
    static constexpr float value = Value.value;
};

template<FloatingArrayValue Value>
struct FloatingArrayBox {
    static constexpr float first = Value.elements[0];
    static constexpr float second = Value.elements[1];
};

template<FloatingAlternative Value>
struct FloatingUnionBox {
    static constexpr float value = Value.single;
};

static_assert(sizeof(FloatingBox<floating_value>) > 0);
static_assert(FloatingBox<floating_value>::value == 1.25f);
static_assert(sizeof(FloatingArrayBox<floating_array_value>) > 0);
static_assert(FloatingArrayBox<floating_array_value>::first == 1.25f);
static_assert(FloatingArrayBox<floating_array_value>::second == -2.5f);
static_assert(sizeof(FloatingUnionBox<floating_union_value>) > 0);
static_assert(FloatingUnionBox<floating_union_value>::value == 1.25f);

int main() {
    return fixed_structural_float_nttp<floating_value>() +
        deduced_structural_float_nttp<floating_value>() +
        structural_float_array_nttp<floating_array_value>() +
        deduced_structural_float_array_nttp<floating_array_value>() +
        structural_float_union_nttp<floating_union_value>() +
        deduced_structural_float_union_nttp<floating_union_value>();
}
