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

template<int... Values>
struct AliasTransformedValues {
    unsigned char values[sizeof...(Values) + 1];
};

template<int... Values>
using TransformedAliasPack =
    AliasTransformedValues<(Values * 3 + 2)...>;

template<int... Values>
using NestedTransformedAliasPack =
    AliasTransformedValues<((Values + 2) * 3 - 1)...>;

TransformedAliasPack<2, 4> transformed_alias_pack_value;
NestedTransformedAliasPack<1, 3> nested_transformed_alias_pack_value;

int check_alias_transformed_values(AliasTransformedValues<8, 14>* value) {
    return value ? 0 : 1;
}

int main() {
    if (sizeof(NonEmptyAliasPack<5, 8>) != 2 * sizeof(int)) return 1;
    if (sizeof(PositiveAliasPack<2, 5>) != 3 * sizeof(int)) return 2;
    if (sizeof(PositiveAliasPack<>) != sizeof(int)) return 3;
    if (sizeof(HasZeroAliasPack<3, 0, 5>) != 4 * sizeof(int)) return 4;
    if (check_alias_transformed_values(&transformed_alias_pack_value)) return 5;
    if (check_alias_transformed_values(&nested_transformed_alias_pack_value)) {
        return 6;
    }
    return 0;
}
