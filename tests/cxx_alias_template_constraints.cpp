template<int... Values>
struct AliasConstraintHolder {
    int values[sizeof...(Values)];
};

template<int... Values>
    requires (sizeof...(Values) > 0)
using NonEmptyAliasPack = AliasConstraintHolder<Values...>;

template<int... Values>
    requires ((Values > 0) && ...)
using PositiveAliasPack = int[sizeof...(Values) + 1];

template<int... Values>
    requires ((Values == 0) || ...)
using HasZeroAliasPack = int[sizeof...(Values) + 1];

int main() {
    return sizeof(NonEmptyAliasPack<5, 8>) == 2 * sizeof(int) &&
                   sizeof(PositiveAliasPack<2, 5>) == 3 * sizeof(int) &&
                   sizeof(PositiveAliasPack<>) == sizeof(int) &&
                   sizeof(HasZeroAliasPack<3, 0, 5>) == 4 * sizeof(int)
               ? 0
               : 1;
}
