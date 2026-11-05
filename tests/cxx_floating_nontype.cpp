template<float Value>
int float_nttp() {
    return Value == 1.25f ? 0 : 1;
}

template<double Value>
int double_nttp() {
    return Value == 2.5 ? 0 : 1;
}

template<long double Value>
int long_double_nttp() {
    return Value == 4.5L ? 0 : 1;
}

template<auto Value>
int deduced_float_nttp() {
    return Value == 3.25f ? 0 : 1;
}

template<float Value = 1.5f>
int default_float_nttp() {
    return Value == 1.5f ? 0 : 1;
}

template<auto Value = 1.25f>
int default_deduced_float_nttp() {
    return Value == 1.25f ? 0 : 1;
}

template<float Value>
constexpr float float_nttp_value = Value;

template<double Value>
constexpr double double_nttp_value = Value;

template<long double Value>
constexpr long double long_double_nttp_value = Value;

template<float Value>
struct FloatBox {
    char byte;
};

template<double Value>
struct DoubleBox {
    char byte;
};

template<long double Value>
struct LongDoubleBox {
    char byte;
};

template<auto Value>
struct DeducedFloatBox {
    char byte;
};

template<float Value = 1.5f>
struct DefaultFloatBox {
    char byte;
};

template<float... Values>
struct FloatPackBox {
    static constexpr int count = sizeof...(Values);
};

template<float... Values>
int float_pack_count() {
    return sizeof...(Values);
}

template<float Value>
int deduced_float_class_nttp(FloatBox<Value>) {
    return Value == 1.25f ? 0 : 1;
}

constexpr float float_value = 1.25f;
constexpr double double_value = 2.5;
constexpr long double long_double_value = 4.5L;

static_assert(sizeof(FloatBox<float_value>) == 1);
static_assert(sizeof(DoubleBox<double_value>) == 1);
static_assert(sizeof(LongDoubleBox<long_double_value>) == 1);
static_assert(sizeof(DeducedFloatBox<3.25f>) == 1);
static_assert(sizeof(DefaultFloatBox<>) == 1);
static_assert(FloatPackBox<1.0f, 2.5f>::count == 2);
static_assert(float_nttp_value<float_value> == 1.25f);
static_assert(double_nttp_value<double_value> == 2.5);
static_assert(long_double_nttp_value<long_double_value> == 4.5L);

int main() {
    return float_nttp<float_value>() + double_nttp<double_value>() +
        long_double_nttp<long_double_value>() +
        deduced_float_nttp<3.25f>() + default_float_nttp<>() +
        default_deduced_float_nttp<>() +
        deduced_float_class_nttp(FloatBox<float_value>{}) +
        float_pack_count<1.0f, 2.5f>() - 2;
}
