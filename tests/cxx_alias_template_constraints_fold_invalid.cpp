template<int... Values>
    requires ((Values > 0) && ...)
using PositiveAliasPack = int[sizeof...(Values) + 1];

PositiveAliasPack<1, 0> non_positive_alias_pack;

int main() {
    return 0;
}
