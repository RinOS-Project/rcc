template<class... Types>
int count_explicit_pack(Types... values) {
    (void)sizeof...(values);
    return sizeof...(Types);
}

template<class T>
int count_deferred_explicit_pack(T value) {
    return count_explicit_pack<
        int, int, int, int, int, int, int, int, int, int, int,
        int, int, int, int, int, int, int, int, int, int,
        int, int, int, int, int, int, int, int, int, int, int, int>(
            value, value, value, value, value, value, value, value, value,
            value, value, value, value, value, value, value, value, value,
            value, value, value, value, value, value, value, value, value,
            value, value, value, value, value, value);
}

template<class... Types>
struct ExplicitAndDeducedPackTypeList {};

template<class... Types>
ExplicitAndDeducedPackTypeList<Types...> explicit_then_deduced_pack(
    Types...);

template<class Actual, class Expected>
struct ExplicitAndDeducedPackSameType {
    static constexpr bool value = false;
};

template<class Type>
struct ExplicitAndDeducedPackSameType<Type, Type> {
    static constexpr bool value = true;
};

using ExplicitAndDeducedPackResult = decltype(
    explicit_then_deduced_pack<int, short>(1L, 2, 3.0));
static_assert(ExplicitAndDeducedPackSameType<
                  ExplicitAndDeducedPackResult,
                  ExplicitAndDeducedPackTypeList<int, short, double>>::value,
              "deduction must extend an explicitly specified parameter pack");

using StandardExplicitAndDeducedPackResult = decltype(
    explicit_then_deduced_pack<int*, float*>(0, 0, 0));
static_assert(ExplicitAndDeducedPackSameType<
                  StandardExplicitAndDeducedPackResult,
                  ExplicitAndDeducedPackTypeList<int*, float*, int>>::value,
              "deduction must append to the explicitly specified pack");

template<int Value>
struct ExplicitAndDeducedValuePackTag {};

template<int... Values>
int explicit_then_deduced_value_pack(ExplicitAndDeducedValuePackTag<Values>...) {
    return static_cast<int>(sizeof...(Values)) * 100 +
           (0 + ... + Values);
}

int main() {
    return count_explicit_pack<
               int, int, int, int, int, int, int, int, int, int, int,
               int, int, int, int, int, int, int, int, int, int,
               int, int, int, int, int, int, int, int, int, int, int, int>(
               1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
               12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
               22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33) == 33 &&
                   count_explicit_pack(
                       1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
                       12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
                       22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33) == 33 &&
                   count_deferred_explicit_pack(7) == 33 &&
                   explicit_then_deduced_value_pack<1, 2>(
                       ExplicitAndDeducedValuePackTag<1>{},
                       ExplicitAndDeducedValuePackTag<2>{},
                       ExplicitAndDeducedValuePackTag<3>{}) == 306
               ? 0
               : 1;
}
