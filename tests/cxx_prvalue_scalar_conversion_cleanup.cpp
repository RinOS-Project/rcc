static int argument_evaluations;
static int pointer_argument_evaluations;
static int constructions;
static int conversions;
static int destructions;
static int constructor_argument_mismatches;
static int pointer_target;

static int next_prvalue_value() {
    ++argument_evaluations;
    return 41;
}

static int* next_prvalue_pointer() {
    ++pointer_argument_evaluations;
    return &pointer_target;
}

struct PrvalueScalarConversion {
    int value;
    int* marker;

    explicit PrvalueScalarConversion(int initial, int* initial_marker) noexcept
        : value(initial), marker(initial_marker) {
        ++constructions;
        if (initial != value || initial_marker != marker) {
            ++constructor_argument_mismatches;
        }
    }

    operator int() && noexcept {
        ++conversions;
        return value;
    }

    ~PrvalueScalarConversion() noexcept {
        ++destructions;
    }
};

int main() {
    int direct = PrvalueScalarConversion{
        next_prvalue_value(), next_prvalue_pointer()};
    if (direct != 41) return 11;
    if (argument_evaluations != 1) return 12;
    if (pointer_argument_evaluations != 1) return 17;
    if (constructions != 1) return 13;
    if (conversions != 1) return 14;
    if (destructions != 1) return 15;
    if (constructor_argument_mismatches != 0) return 16;

    int listed = {PrvalueScalarConversion{
        next_prvalue_value(), next_prvalue_pointer()}};
    if (listed != 41) return 21;
    if (argument_evaluations != 2) return 22;
    if (pointer_argument_evaluations != 2) return 27;
    if (constructions != 2) return 23;
    if (conversions != 2) return 24;
    if (destructions != 2) return 25;
    if (constructor_argument_mismatches != 0) return 26;

    return 0;
}
