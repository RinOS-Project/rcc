template<int... Values>
    requires (sizeof...(Values) > 0)
using NonEmptyAliasPack = int[sizeof...(Values)];

NonEmptyAliasPack<> empty_alias_pack_value;

int main() {
    return 0;
}
