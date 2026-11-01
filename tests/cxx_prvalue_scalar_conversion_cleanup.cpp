static int constructions;
static int conversions;
static int destructions;

struct PrvalueScalarConversion {
    int value;

    PrvalueScalarConversion() noexcept : value(41) {
        ++constructions;
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
    int direct = PrvalueScalarConversion{};
    if (direct != 41) return 11;
    if (constructions != 1) return 13;
    if (conversions != 1) return 14;
    if (destructions != 1) return 15;

    int listed = {PrvalueScalarConversion{}};
    if (listed != 41) return 21;
    if (constructions != 2) return 23;
    if (conversions != 2) return 24;
    if (destructions != 2) return 25;

    return 0;
}
