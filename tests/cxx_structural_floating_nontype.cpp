struct FloatingValue {
    float value;
};

constexpr FloatingValue floating_value{1.25f};

template<FloatingValue Value>
int fixed_structural_float_nttp() {
    return Value.value == 1.25f ? 0 : 1;
}

template<auto Value>
int deduced_structural_float_nttp() {
    return Value.value == 1.25f ? 0 : 1;
}

template<FloatingValue Value>
struct FloatingBox {
    static constexpr float value = Value.value;
};

static_assert(sizeof(FloatingBox<floating_value>) > 0);
static_assert(FloatingBox<floating_value>::value == 1.25f);

int main() {
    return fixed_structural_float_nttp<floating_value>() +
        deduced_structural_float_nttp<floating_value>();
}
