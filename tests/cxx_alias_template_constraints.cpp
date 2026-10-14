template<int... Values>
struct AliasConstraintHolder {
    int values[sizeof...(Values)];
};

template<int... Values>
    requires (sizeof...(Values) > 0)
using NonEmptyAliasPack = AliasConstraintHolder<Values...>;

int main() {
    return sizeof(NonEmptyAliasPack<5, 8>) == 2 * sizeof(int) ? 0 : 1;
}
