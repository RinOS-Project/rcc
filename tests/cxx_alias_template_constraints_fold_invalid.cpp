template<int... Values>
struct AliasTransformedValues {
    unsigned char values[sizeof...(Values) + 1];
};

template<int... Values>
    requires ((Values > 0) && ...)
using PositiveTransformedAliasPack =
    AliasTransformedValues<(Values * 3 + 2)...>;

PositiveTransformedAliasPack<1, 0> non_positive_alias_pack;

int main() {
    return 0;
}
